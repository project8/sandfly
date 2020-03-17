/*
 * mt_run_server.cc
 *
 *  Created on: May 6, 2015
 *      Author: nsoblath
 */

#include "run_server.hh"

#include "psyllid_constants.hh"
#include "daq_control.hh"
#include "message_relayer.hh"
#include "request_receiver.hh"
#include "signal_handler.hh"
#include "stream_manager.hh"
#include "batch_executor.hh"

#include "logger.hh"

#include <condition_variable>
#include <thread>

using dripline::request_ptr_t;

using scarab::param_node;
using scarab::version_semantic;

namespace sandfly
{
    LOGGER( plog, "run_server" );

    run_server::run_server() :
            scarab::cancelable(),
            f_return( RETURN_SUCCESS ),
            f_request_receiver(),
            f_batch_executor(),
            f_daq_control(),
            f_stream_manager(),
            f_component_mutex(),
            f_status( k_initialized )
    {
    }

    run_server::~run_server()
    {
    }

    void run_server::execute( const param_node& a_config )
    {
        LPROG( plog, "Creating server objects" );

        set_status( k_starting );

        scarab::signal_handler t_sig_hand;
        t_sig_hand.add_cancelable( this );

        // configuration manager
        //config_manager t_config_mgr( a_config, &t_dev_mgr );

        std::unique_lock< std::mutex > t_lock( f_component_mutex );

        std::thread t_msg_relay_thread;
        try
        {
            // dripline relayer
            try
            {
                message_relayer* t_msg_relay = message_relayer::create_instance( a_config["amqp"].as_node() );
                if( a_config["post-to-slack"]().as_bool() )
                {
                    LDEBUG( plog, "Starting message relayer thread" );
                    t_msg_relay_thread = std::thread( &message_relayer::execute_relayer, t_msg_relay );
                    t_msg_relay->slack_notice( "Psyllid is starting up" );
                }
            }
            catch(...)
            {
                LWARN( plog, "Message relayer already exists, and you're trying to create it again" );
            }

            // node manager
            LDEBUG( plog, "Creating stream manager" );
            f_stream_manager.reset( new stream_manager() );

            // daq control
            LDEBUG( plog, "Creating DAQ control" );
            f_daq_control.reset( new daq_control( a_config, f_stream_manager ) );
            // provide the pointer to the daq_control to control_access
            control_access::set_daq_control( f_daq_control );
            f_daq_control->initialize();

            if( a_config.has( "streams" ) && a_config["streams"].is_node() )
            {
                if( ! f_stream_manager->initialize( a_config["streams"].as_node() ) )
                {
                    throw error() << "Unable to initialize the stream manager";
                }
            }

            // request receiver
            LDEBUG( plog, "Creating request receiver" );
            f_request_receiver.reset( new request_receiver( a_config ) );
            // batch executor
            LDEBUG( plog, "Creating batch executor" );
            f_batch_executor.reset( new batch_executor( a_config, f_request_receiver ) );

        }
        catch( std::exception& e )
        {
            LERROR( plog, "Exception caught while creating server objects: " << e.what() );
            f_return = RETURN_ERROR;
            return;
        }

        // tie the various request handlers of psyllid to the request receiver

        using namespace std::placeholders;

        // set the run request handler
        f_request_receiver->set_run_handler( std::bind( &daq_control::handle_start_run_request, f_daq_control, _1 ) );

        // add get request handlers
        //f_request_receiver->register_get_handler( "server-status", std::bind( &run_server::handle_get_server_status_request, this, _1 ) );
        f_request_receiver->register_get_handler( "node-config", std::bind( &stream_manager::handle_dump_config_node_request, f_stream_manager, _1 ) );
        f_request_receiver->register_get_handler( "active-config", std::bind( &daq_control::handle_dump_config_request, f_daq_control, _1 ) );
        f_request_receiver->register_get_handler( "daq-status", std::bind( &daq_control::handle_get_status_request, f_daq_control, _1 ) );
        f_request_receiver->register_get_handler( "filename", std::bind( &daq_control::handle_get_filename_request, f_daq_control, _1 ) );
        f_request_receiver->register_get_handler( "description", std::bind( &daq_control::handle_get_description_request, f_daq_control, _1 ) );
        f_request_receiver->register_get_handler( "duration", std::bind( &daq_control::handle_get_duration_request, f_daq_control, _1 ) );
        f_request_receiver->register_get_handler( "use-monarch", std::bind( &daq_control::handle_get_use_monarch_request, f_daq_control, _1 ) );
        f_request_receiver->register_get_handler( "stream-list", std::bind( &stream_manager::handle_get_stream_list_request, f_stream_manager, _1 ) );
        f_request_receiver->register_get_handler( "node-list", std::bind( &stream_manager::handle_get_stream_node_list_request, f_stream_manager, _1 ) );

        // add set request handlers
        f_request_receiver->register_set_handler( "node-config", std::bind( &stream_manager::handle_configure_node_request, f_stream_manager, _1 ) );
        f_request_receiver->register_set_handler( "active-config", std::bind( &daq_control::handle_apply_config_request, f_daq_control, _1 ) );
        f_request_receiver->register_set_handler( "filename", std::bind( &daq_control::handle_set_filename_request, f_daq_control, _1 ) );
        f_request_receiver->register_set_handler( "description", std::bind( &daq_control::handle_set_description_request, f_daq_control, _1 ) );
        f_request_receiver->register_set_handler( "duration", std::bind( &daq_control::handle_set_duration_request, f_daq_control, _1 ) );
        f_request_receiver->register_set_handler( "use-monarch", std::bind( &daq_control::handle_set_use_monarch_request, f_daq_control, _1 ) );

        // add cmd request handlers
        f_request_receiver->register_cmd_handler( "add-stream", std::bind( &stream_manager::handle_add_stream_request, f_stream_manager, _1 ) );
        f_request_receiver->register_cmd_handler( "remove-stream", std::bind( &stream_manager::handle_remove_stream_request, f_stream_manager, _1 ) );
        f_request_receiver->register_cmd_handler( "run-daq-cmd", std::bind( &daq_control::handle_run_command_request, f_daq_control, _1 ) );
        f_request_receiver->register_cmd_handler( "stop-run", std::bind( &daq_control::handle_stop_run_request, f_daq_control, _1 ) );
        f_request_receiver->register_cmd_handler( "start-run", std::bind( &daq_control::handle_start_run_request, f_daq_control, _1 ) );
        f_request_receiver->register_cmd_handler( "activate-daq", std::bind( &daq_control::handle_activate_daq_control, f_daq_control, _1 ) );
        f_request_receiver->register_cmd_handler( "reactivate-daq", std::bind( &daq_control::handle_reactivate_daq_control, f_daq_control, _1 ) );
        f_request_receiver->register_cmd_handler( "deactivate-daq", std::bind( &daq_control::handle_deactivate_daq_control, f_daq_control, _1 ) );
        f_request_receiver->register_cmd_handler( "quit-psyllid", std::bind( &run_server::handle_quit_server_request, this, _1 ) );

        std::condition_variable t_daq_control_ready_cv;
        std::mutex t_daq_control_ready_mutex;

        // start threads
        LPROG( plog, "Starting threads" );
        std::exception_ptr t_dc_ex_ptr;
        std::thread t_daq_control_thread( &daq_control::execute, f_daq_control.get(), std::ref(t_daq_control_ready_cv), std::ref(t_daq_control_ready_mutex) );
        // batch execution to do initial calls (AMQP consume hasn't started yet)
        std::thread t_executor_thread_initial( &batch_executor::execute, f_batch_executor.get(), std::ref(t_daq_control_ready_cv), std::ref(t_daq_control_ready_mutex), false );
        LDEBUG( plog, "Waiting for the batch executor to finish" );
        t_executor_thread_initial.join();
        LDEBUG( plog, "Initial batch executions complete" );

        if( ! is_canceled() )
        {
            // now execute the request receiver to start consuming
            //     and start the batch executor in infinite mode so that more command sets may be staged later
            std::thread t_executor_thread( &batch_executor::execute, f_batch_executor.get(), std::ref(t_daq_control_ready_cv), std::ref(t_daq_control_ready_mutex), true );
            std::thread t_receiver_thread( &request_receiver::execute, f_request_receiver.get(), std::ref(t_daq_control_ready_cv), std::ref(t_daq_control_ready_mutex) );

            t_lock.unlock();

            set_status( k_running );
            LPROG( plog, "Running..." );

            t_receiver_thread.join();
            LPROG( plog, "Receiver thread has ended" );
            // if make_connection is false, we need to actually call cancel:
            if ( ! f_request_receiver.get()->get_make_connection() )
            {
                LINFO( plog, "Request receiver not making connections, canceling run server" );
                scarab::signal_handler::cancel_all( RETURN_ERROR );
            }
            // and then wait for the controllers to finish up...
            t_executor_thread.join();
        }
        t_daq_control_thread.join();
        LPROG( plog, "DAQ control thread has ended" );

        t_sig_hand.remove_cancelable( this );

        if( t_msg_relay_thread.joinable() ) t_msg_relay_thread.join();
        LDEBUG( plog, "Message relay thread has ended" );

        set_status( k_done );

        LPROG( plog, "Threads stopped" );

        return;
    }

    void run_server::do_cancellation( int a_code )
    {
        LDEBUG( plog, "Canceling run server with code <" << a_code << ">" );
        f_return = a_code;
        message_relayer::get_instance()->slack_notice( "Psyllid is shutting down" );
        f_batch_executor->cancel( a_code );
        f_request_receiver->cancel( a_code );
        f_daq_control->cancel( a_code );
        message_relayer::get_instance()->cancel( a_code );
        //f_node_manager->cancel();
        return;
    }

    void run_server::quit_server()
    {
        LINFO( plog, "Shutting down the server" );
        cancel( f_status == k_error ? RETURN_ERROR : RETURN_SUCCESS );
        return;
    }


    dripline::reply_ptr_t run_server::handle_get_server_status_request( const dripline::request_ptr_t a_request )
    {
        /*
        param_node* t_server_node = new param_node();
        t_server_node->add( "status", new param_value( run_server::interpret_status( get_status() ) ) );

        f_component_mutex.lock();
        if( f_request_receiver != NULL )
        {
            param_node* t_rr_node = new param_node();
            t_rr_node->add( "status", new param_value( request_receiver::interpret_status( f_request_receiver->get_status() ) ) );
            t_server_node->add( "request-receiver", t_rr_node );
        }
        if( f_acq_request_db != NULL )
        {
            param_node* t_queue_node = new param_node();
            t_queue_node->add( "size", new param_value( (uint32_t)f_acq_request_db->queue_size() ) );
            t_queue_node->add( "is-active", new param_value( f_acq_request_db->queue_is_active() ) );
            t_server_node->add( "queue", t_queue_node );
        }
        if( f_server_worker != NULL )
        {
            param_node* t_sw_node = new param_node();
            t_sw_node->add( "status", new param_value( server_worker::interpret_status( f_server_worker->get_status() ) ) );
            t_sw_node->add( "digitizer-state", new param_value( server_worker::interpret_thread_state( f_server_worker->get_digitizer_state() ) ) );
            t_sw_node->add( "writer-state", new param_value( server_worker::interpret_thread_state( f_server_worker->get_writer_state() ) ) );
            t_server_node->add( "server-worker", t_sw_node );
        }
        f_component_mutex.unlock();

        a_reply_pkg.f_payload.add( "server", t_server_node );

        return a_reply_pkg.send_reply( dripline::dl_success(), "Server status request succeeded" );
        */
        return a_request->reply( dripline::dl_message_error_invalid_method(), "Server status request not yet supported" );
    }

    dripline::reply_ptr_t run_server::handle_quit_server_request( const dripline::request_ptr_t a_request )
    {
        dripline::reply_ptr_t t_return = a_request->reply( dripline::dl_success(), "Server-quit command processed" );
        quit_server();
        return t_return;
    }


    std::string run_server::interpret_status( status a_status )
    {
        switch( a_status )
        {
            case k_initialized:
                return std::string( "Initialized" );
                break;
            case k_starting:
                return std::string( "Starting" );
                break;
            case k_running:
                return std::string( "Running" );
                break;
            case k_done:
                return std::string( "Done" );
                break;
            case k_error:
                return std::string( "Error" );
                break;
            default:
                return std::string( "Unknown" );
        }
    }


} /* namespace mantis */
