/*
 * batch_executor.cc
 *
 * Created on: April 12, 2018
 *     Author: laroque
 */

//sandfly includes
#include "batch_executor.hh"
#include "run_control.hh"
#include "sandfly_return_codes.hh"
#include "request_receiver.hh"

//non-sandfly P8 includes
#include "dripline_constants.hh"
#include "dripline_exceptions.hh"

#include "logger.hh"
#include "signal_handler.hh"

//external includes
#include <chrono>
#include <signal.h>
#include <thread>

namespace sandfly
{

    LOGGER( plog, "batch_executor" );

    batch_executor::batch_executor() :
            control_access(),
            scarab::cancelable(),
            f_batch_commands(),
            f_request_receiver(),
            f_action_queue(),
            f_condition_actions()
    {
    }

    batch_executor::batch_executor( const scarab::param_node& a_config, std::shared_ptr< request_receiver > a_request_receiver ) :
            control_access(),
            scarab::cancelable(),
            f_batch_commands( a_config[ "batch-commands" ].as_node() ),
            f_request_receiver( a_request_receiver ),
            f_action_queue(),
            f_condition_actions()
    {
        if ( a_config.has( "on-startup" ) )
        {
            LINFO( plog, "have an initial action array" );
            add_to_queue( a_config["on-startup"].as_array() );
        }
        else
        {
            LINFO( plog, "no initial batch actions" );
        }

        // register batch commands
        using namespace std::placeholders;
        for ( scarab::param_node::iterator command_it = f_batch_commands.begin(); command_it != f_batch_commands.end(); ++command_it )
        {
            a_request_receiver->register_cmd_handler( command_it.name(), std::bind( &batch_executor::do_batch_cmd_request, this, command_it.name(), _1 ) );
        }
    }

    batch_executor::~batch_executor()
    {
    }

    void batch_executor::clear_queue()
    {
        action_info t_action;
        while ( f_action_queue.try_pop( t_action ) )
        {
        }
    }

    void batch_executor::add_to_queue( const scarab::param_node& an_action )
    {
        f_action_queue.push( parse_action( an_action ) );
    }

    void batch_executor::add_to_queue( const scarab::param_array& actions_array )
    {
        for( scarab::param_array::const_iterator action_it = actions_array.begin();
              action_it!=actions_array.end();
              ++action_it )
        {
            LDEBUG( plog, "adding an item: " << action_it->as_node() )
            add_to_queue( action_it->as_node() );
        }
    }

    void batch_executor::add_to_queue( const std::string& a_batch_command_name )
    {
        if ( f_batch_commands.has( a_batch_command_name ) )
        {
            add_to_queue( f_batch_commands[a_batch_command_name].as_array() );
        }
        else
        {
            LWARN( plog, "no configured batch action for: '" << a_batch_command_name << "' ignoring" );
        }
    }

    void batch_executor::replace_queue( const scarab::param_node& an_action )
    {
        clear_queue();
        add_to_queue( an_action );
    }

    void batch_executor::replace_queue( const scarab::param_array& actions_array )
    {
        clear_queue();
        add_to_queue( actions_array );
    }

    void batch_executor::replace_queue( const std::string& a_batch_command_name )
    {
        clear_queue();
        add_to_queue( a_batch_command_name );
    }

    // this method should be bound in the request receiver to be called with a command name, the request_ptr_t is not used
    dripline::reply_ptr_t batch_executor::do_batch_cmd_request( const std::string& a_command, const dripline::request_ptr_t a_request )
    {
        try
        {
            add_to_queue( a_command );
        }
        catch( dripline::dripline_error& e )
        {
            return a_request->reply( dl_sandfly_error(), std::string("Error processing command: ") + e.what() );
        }
        return a_request->reply( dripline::dl_success(), "" );
    }

    // this method should be bound in the request receiver to be called with a command name, the request_ptr_t is not used
    dripline::reply_ptr_t batch_executor::do_replace_actions_request( const std::string& a_command, const dripline::request_ptr_t a_request )
    {
        try
        {
            //TODO do we have a good way to interrupt an ongoing action here?
            replace_queue( a_command );
        }
        catch( dripline::dripline_error& e )
        {
            return a_request->reply( dl_sandfly_error(), std::string("Error processing command: ") + e.what() );
        }
        return a_request->reply( dripline::dl_success(), "" );
    }

    /* considering yaml that looks like:
    batch-actions:
        - type: cmd
          sleep-for: 500 # [ms], optional element to specify the sleep after issuing the cmd, before proceeding to the next.
          key: start-run
          payload:
            duration: 200
            filenames: '["/tmp/foo_t.yaml", "/tmp/foo_f.yaml"]'
    */
    void batch_executor::execute( std::condition_variable& a_run_control_ready_cv, std::mutex& a_run_control_ready_mutex, bool a_run_forever )
    {
        if( run_control_expired() )
        {
            LERROR( plog, "Unable to get access to the DAQ control" );
            scarab::signal_handler::cancel_all( RETURN_ERROR );
            return;
        }
        dc_ptr_t t_run_control_ptr = use_run_control();

        while ( ! t_run_control_ptr->is_ready_at_startup() && ! is_canceled() )
        {
            std::unique_lock< std::mutex > t_run_control_lock( a_run_control_ready_mutex );
            a_run_control_ready_cv.wait_for( t_run_control_lock, std::chrono::seconds(1) );
        }

        LINFO( plog, "Batch executor is starting to execute actions" );

        try
        {
            while ( ( a_run_forever || f_action_queue.size() ) && ! is_canceled() )
            {
                if ( f_action_queue.size() )
                {
                    do_an_action();
                }
            }
        }
        catch( error& e )
        {
            LERROR( plog, "Caught a sandfly error; stopping batch execution\n" << e.what() );
            scarab::signal_handler::exit( RETURN_ERROR );
        }
        catch( fatal_error& e )
        {
            LERROR( plog, "Caught a sandfly fatal_error: stopping batch execution\n" << e.what() );
            scarab::signal_handler::exit( RETURN_ERROR );
        }
        catch( dripline::dripline_error& e )
        {
            LERROR( plog, "Caught a dripline error; stopping batch execution\n" << e.what() );
            scarab::signal_handler::exit( RETURN_ERROR );
        }
        catch( std::exception& e )
        {
            LERROR( plog, "Caught an error; stopping batch execution\n" << e.what() )
            scarab::signal_handler::exit( RETURN_ERROR );
        }

        LINFO( plog, "Batch executor has completed action execution" );

        return;
    }

    void batch_executor::do_an_action()
    {
        action_info t_action;
        if ( !f_action_queue.try_pop( t_action ) )
        {
            LDEBUG( plog, "there are no actions in the queue" );
            return;
        }

        LINFO( plog, "Running action:\n" << *t_action.f_request_ptr );

        dripline::reply_ptr_t t_request_reply = f_request_receiver->submit_request_message( t_action.f_request_ptr );
        if ( ! t_request_reply )
        {
            LWARN( plog, "failed submitting action request" );
            throw error() << "error while submitting command";
        }

        // wait until daq status is no longer "running"
        if ( t_action.f_is_custom_action )
        {
            run_control::status t_status = run_control::uint_to_status( t_request_reply->payload()["server"]["status-value"]().as_uint() );
            while ( t_status == run_control::status::running )
            {
                t_request_reply = f_request_receiver->submit_request_message( t_action.f_request_ptr );
                t_status = run_control::uint_to_status( t_request_reply->payload()["server"]["status-value"]().as_uint() );
                std::this_thread::sleep_for( std::chrono::milliseconds( t_action.f_sleep_duration_ms ) );
            }
        }
        else
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( t_action.f_sleep_duration_ms ) );
        }
        if ( t_request_reply->get_return_code() >= 100 )
        {
            LWARN( plog, "batch action received an error-level return code; exiting" );
            throw error() << "error completing batch action, received code [" <<
                                   t_request_reply->get_return_code() << "]: \"" <<
                                   t_request_reply->return_message() << "\"";
        }
    }

    action_info batch_executor::parse_action( const scarab::param_node& a_action )
    {
        if ( ! a_action["payload"].is_node() )
        {
            LERROR( plog, "payload must be a param_node" );
            throw error() << "batch action payload must be a node";
        }

        action_info t_action_info;
        std::string t_routing_key, t_specifier;
        dripline::op_t t_msg_op;
        
        try
        {
            t_routing_key = a_action["key" ]().as_string();
            t_action_info.f_sleep_duration_ms = std::stoi( a_action.get_value( "sleep-for", "500" ) );
            t_action_info.f_is_custom_action = false;
        }
        catch( scarab::error& e )
        {
            LERROR( plog, "error parsing action param_node, check keys and value types: " << e.what() );
            throw;
        }

        try
        {
            t_msg_op = dripline::to_op_t( a_action["type"]().as_string() );
        }
        catch( dripline::dripline_error& )
        {
            LDEBUG( plog, "got a dripline error parsing request type" );
            if ( a_action["type"]().as_string() == "wait-for" && t_routing_key == "daq-status" )
            {
                LDEBUG( plog, "action is poll on run status" );
                t_msg_op = dripline::op_t::get;
                t_action_info.f_is_custom_action = true;
            }
            else throw;
        }

        if( a_action.has( "specifier" ) ) t_specifier = a_action["specifier"]().as_string();

        LDEBUG( plog, "building request object" );

        scarab::param_ptr_t t_payload_ptr( new scarab::param_node( a_action["payload"].as_node() ) );

        // put it together into a request
        t_action_info.f_request_ptr = dripline::msg_request::create(
                                            std::move(t_payload_ptr),
                                            t_msg_op,
                                            t_routing_key );// reply-to is empty because no reply for batch requests
        t_action_info.f_request_ptr->parsed_specifier().parse( t_specifier );

        LDEBUG( plog, "Adding action:\n" << *t_action_info.f_request_ptr );

        return t_action_info;
    }

} /* namespace sandfly */
