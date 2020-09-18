/*
 * run_control.cc
 *
 *  Created on: Jan 22, 2016
 *      Author: nsoblath
 */

#include "run_control.hh"

#include "message_relayer.hh"
#include "node_builder.hh"
#include "request_receiver.hh"

#include "diptera.hh"
#include "midge_error.hh"

#include "logger.hh"
#include "signal_handler.hh"

#include "return_codes.hh"

#include <chrono>
#include <condition_variable>
#include <future>
#include <signal.h>
#include <thread>

using scarab::param_array;
using scarab::param_node;
using scarab::param_value;
using scarab::param_ptr_t;

using dripline::request_ptr_t;

using std::string;

namespace sandfly
{
    LOGGER( plog, "run_control" );

    run_control::run_control( const param_node& a_master_config, std::shared_ptr< stream_manager > a_mgr ) :
            scarab::cancelable(),
            control_access(),
            f_activation_condition(),
            f_daq_mutex(),
            f_node_manager( a_mgr ),
            f_daq_config(),
            f_midge_pkg(),
            f_node_bindings( nullptr ),
            f_run_stopper(),
            f_run_stop_mutex(),
            f_do_break_run( false ),
            f_run_return(),
            f_msg_relay( message_relayer::get_instance() ),
            f_run_duration( 1000 ),
            f_status( status::deactivated )
    {
        // DAQ config is optional; defaults will work just fine
        if( a_master_config.has( "daq" ) )
        {
            f_daq_config.merge( a_master_config["daq"].as_node() );
        }

        set_run_duration( f_daq_config.get_value( "duration", get_run_duration() ) );
    }

    run_control::~run_control()
    {
    }

    void run_control::initialize()
    {
        this->on_initialize();
        return;
    }

    void run_control::execute( std::condition_variable& a_ready_condition_variable, std::mutex& a_ready_mutex )
    {
        // if we're supposed to activate on startup, we'll call activate asynchronously
        std::future< void > t_activation_return;
        if( f_daq_config.get_value( "activate-at-startup", false ) )
        {
            LDEBUG( plog, "Will activate DAQ control asynchronously" );
            t_activation_return = std::async( std::launch::async,
                        [this]()
                        {
                            std::this_thread::sleep_for( std::chrono::milliseconds(250) );
                            LDEBUG( plog, "Activating DAQ control at startup" );
                            activate();
                        } );
        }

        // Errors caught during this loop are handled by setting the status to error, and continuing the loop,
        // which then goes to the error clause of the outer if/elseif logic
        while( ! is_canceled() )
        {
            status t_status = get_status();
            LDEBUG( plog, "run_control execute loop; status is <" << interpret_status( t_status ) << ">" );
            if( ( t_status == status::deactivated ) && ! is_canceled() )
            {
                while( t_status == status::deactivated )
                {
                    std::unique_lock< std::mutex > t_lock( f_daq_mutex );
                    LDEBUG( plog, "DAQ control waiting for activation signal; status is " << interpret_status( t_status ) );
                    f_activation_condition.wait_for( t_lock, std::chrono::seconds(1) );
                    t_status = get_status();
                }
            }
            else if( t_status == status::activating )
            {
                LPROG( plog, "DAQ control activating" );

                try
                {
                    if( f_node_manager->must_reset_midge() )
                    {
                        LDEBUG( plog, "Reseting midge" );
                        f_node_manager->reset_midge();
                    }
                }
                catch( error& e )
                {
                    LWARN( plog, "Exception caught while resetting midge: " << e.what() );
                    LWARN( plog, "Returning to the \"deactivated\" state and awaiting further instructions" );
                    f_msg_relay->slack_error( std::string("DAQ could not be activated (non-fatal error).\nDetails: ") + e.what() );
                    set_status( status::deactivated );
                    continue;
                }
                catch( fatal_error& e )
                {
                    LERROR( plog, "Exception caught while resetting midge: " << e.what() );
                    LERROR( plog, "Setting an error state and exiting the application" );
                    f_msg_relay->slack_error( std::string("DAQ could not be activated (fatal error).\nDetails: ") + e.what() );
                    set_status( status::error );
                    continue;
                }

                LDEBUG( plog, "Acquiring midge package" );
                f_midge_pkg = f_node_manager->get_midge();

                if( ! f_midge_pkg.have_lock() )
                {
                    LERROR( plog, "Could not get midge resource" );
                    f_msg_relay->slack_error( "Midge resource is locked; unable to activate the DAQ" );
                    set_status( status::error );
                    continue;
                }

                this->on_pre_midge_run();

                // set midge's running callback
                f_midge_pkg->set_running_callback(
                        [this, &a_ready_condition_variable, &a_ready_mutex]() {
                            set_status( status::activated );
                            std::lock_guard<std::mutex> ready_lock(a_ready_mutex);
                            a_ready_condition_variable.notify_all();
                            return;
                        }
                );

                f_node_bindings = f_node_manager->get_node_bindings();

                std::exception_ptr t_e_ptr;

                try
                {
                    std::string t_run_string( f_node_manager->get_node_run_str() );
                    LDEBUG( plog, "Starting midge with run string <" << t_run_string << ">" );
                    t_e_ptr = f_midge_pkg->run( t_run_string );
                    LINFO( plog, "DAQ control is shutting down after midge exited" );
                }
                catch( std::exception& e )
                {
                    LERROR( plog, "An exception was thrown while running midge: " << e.what() );
                    f_msg_relay->slack_error( std::string("An exception was thrown while running midge: ") + e.what() );
                    set_status( status::error );
                }

                LDEBUG( plog, "Midge has finished running" );

                this->on_post_midge_run();

                if( t_e_ptr )
                {
                    LDEBUG( plog, "An exception from midge is present; rethrowing" );
                    try
                    {
                        std::rethrow_exception( t_e_ptr );
                    }
                    catch( midge::error& e )
                    {
                        LERROR( plog, "A Midge error has been caught: " << e.what() );
                        f_msg_relay->slack_error( std::string("A Midge error has been caught: ") + e.what() );
                        set_status( status::error );
                    }
                    catch( midge::node_fatal_error& e )
                    {
                        LERROR( plog, "A fatal node error was thrown from midge: " << e.what() );
                        f_msg_relay->slack_error( std::string("A fatal node error was thrown from midge: ") + e.what() );
                        set_status( status::error );
                    }
                    catch( midge::node_nonfatal_error& e )
                    {
                        LWARN( plog, "A non-fatal node error was thrown from midge: " << e.what() );
                        f_msg_relay->slack_error( std::string("A non-fatal node error was thrown from midge.  ") +
                                "DAQ is still running (hopefully) but its state has been reset.\n" +
                                "Error details: " + e.what() );
                        set_status( status::do_restart );
                    }
                    catch( std::exception& e )
                    {
                        LERROR( plog, "An unknown exception was thrown from midge: " << e.what() );
                        f_msg_relay->slack_error( std::string("An unknown exception was thrown from midge: ") + e.what() );
                        set_status( status::error );
                    }
                    LDEBUG( plog, "Calling stop_run" );
                    stop_run();
                }

                f_node_manager->return_midge( std::move( f_midge_pkg ) );
                f_node_bindings = nullptr;

                if( get_status() == status::running )
                {
                    LERROR( plog, "Midge exited abnormally; error condition is unknown; canceling" );
                    scarab::signal_handler::cancel_all( RETURN_ERROR );
                    continue;
                }
                else if( get_status() == status::error )
                {
                    LERROR( plog, "Canceling due to an error" );
                    f_msg_relay->slack_error( "Sandfly is exiting due to an error.  Hopefully the details have already been reported." );
                    scarab::signal_handler::cancel_all( RETURN_ERROR );
                    continue;
                }
                else if( get_status() == status::do_restart )
                {
                    LDEBUG( plog, "Setting status to deactivated" );
                    set_status( status::deactivated );
                    LINFO( plog, "Commencing restart of the DAQ" );
                    f_msg_relay->slack_warn( "Commencing restart of the Sandfly DAQ nodes" );
                    std::this_thread::sleep_for( std::chrono::milliseconds(250) );
                    LDEBUG( plog, "Will activate DAQ control asynchronously" );
                    t_activation_return = std::async( std::launch::async,
                                [this]()
                                {
                                    std::this_thread::sleep_for( std::chrono::milliseconds(250) );
                                    LDEBUG( plog, "Restarting DAQ control" );
                                    activate();
                                } );
                    continue;
                }
            }
            else if( t_status == status::activated )
            {
                LERROR( plog, "DAQ control status is activated in the outer execution loop!" );
                f_msg_relay->slack_error( "DAQ control status is activated in the outer execution loop!" );
                set_status( status::error );
                continue;
            }
            else if( t_status == status::deactivating )
            {
                LDEBUG( plog, "DAQ control deactivating; status now set to \"deactivated\"" );
                set_status( status::deactivated );
            }
            else if( t_status == status::done )
            {
                LINFO( plog, "Exiting DAQ control" );
                this->on_done();
                f_node_bindings = nullptr;
                break;
            }
            else if( t_status == status::error )
            {
                LERROR( plog, "DAQ control is in an error state" );
                f_msg_relay->slack_error( "DAQ control is in an error state and will now exit" );
                this->on_error();
                f_node_bindings = nullptr;
                scarab::signal_handler::cancel_all( RETURN_ERROR );
                break;
            }
        }

        return;
    }

    void run_control::activate()
    {
        LDEBUG( plog, "Activating DAQ control" );

        if( is_canceled() )
        {
            throw error() << "DAQ control has been canceled";
        }

        if( get_status() != status::deactivated )
        {
            throw status_error() << "DAQ control is not in the deactivated state";
        }

        this->on_activate();

        LDEBUG( plog, "Setting status to activating" );
        set_status( status::activating );
        f_activation_condition.notify_one();

        return;
    }

    void run_control::reactivate()
    {
        deactivate();
        std::this_thread::sleep_for( std::chrono::seconds(1) );
        activate();
        return;
    }

    void run_control::deactivate()
    {
        LDEBUG( plog, "Deactivating DAQ" );

        if( is_canceled() )
        {
            throw error() << "DAQ control has been canceled";
        }

        if( get_status() != status::activated )
        {
            throw status_error() << "Invalid state for deactivating: <" + run_control::interpret_status( get_status() ) + ">; DAQ control must be in activated state";
        }

        set_status( status::deactivating );

        this->on_deactivate();

        if( f_midge_pkg.have_lock() )
        {
            LDEBUG( plog, "Canceling DAQ worker from DAQ control" );
            f_midge_pkg->cancel();
        }

        return;
    }

    bool run_control::is_ready_at_startup() const
    {
        return f_daq_config["activate-at-startup"]().as_bool() ? (f_status == status::activated) : (f_status == status::activated || f_status == status::deactivated);
    }

    void run_control::start_run()
    {
        LDEBUG( plog, "Preparing for run" );

        if( is_canceled() )
        {
            throw error() << "run_control has been canceled";
        }

        if( get_status() != status::activated )
        {
            throw status_error() << "DAQ control must be in the activated state to start a run; activate the DAQ and try again";
        }

        if( ! f_midge_pkg.have_lock() )
        {
            throw error() << "Do not have midge resource";
        }

        LDEBUG( plog, "Launching asynchronous do_run" );
        f_run_return = std::async( std::launch::async, &run_control::do_run, this, f_run_duration );
        //TODO: use run return?

        return;
    }

    void run_control::do_run( unsigned a_duration )
    {
        // a_duration is in ms

        LINFO( plog, "Run is commencing" );
        f_msg_relay->slack_notice( "Run is commencing" );

        this->on_pre_run();

        typedef std::chrono::steady_clock::duration duration_t;
        typedef std::chrono::steady_clock::time_point time_point_t;

        duration_t t_run_duration = std::chrono::duration_cast< duration_t >( std::chrono::milliseconds(a_duration) ); // a_duration converted from ms to duration_t
        duration_t t_sub_duration = std::chrono::duration_cast< duration_t >( std::chrono::milliseconds(500) ); // 500 ms sub-duration converted to duration_t
        LINFO( plog, "Run duration will be " << a_duration << " ms;  sub-durations of 500 ms will be used" );
        LDEBUG( plog, "In units of steady_clock::duration: run duration is " << t_run_duration.count() << " and sub_duration is " << t_sub_duration.count() );

        std::unique_lock< std::mutex > t_run_stop_lock( f_run_stop_mutex );
        f_do_break_run = false;

        LDEBUG( plog, "Unpausing midge" );
        set_status( status::running );
        if( f_midge_pkg.have_lock() ) f_midge_pkg->instruct( midge::instruction::resume );
        else
        {
            LERROR( plog, "Midge resource is not available" );
            return;
        }

        if( a_duration == 0 )
        {
            LDEBUG( plog, "Untimed run stopper in use" );
            // conditions that will break the loop:
            //   - last sub-duration was not stopped for a timeout (the other possibility is that f_run_stopper was notified by e.g. stop_run())
            //   - run_control has been canceled
            while( ! f_do_break_run && ! is_canceled() )
            {
                f_run_stopper.wait_for( t_run_stop_lock, t_sub_duration );
            }
        }
        else
        {
            LDEBUG( plog, "Timed run stopper in use; limit is " << a_duration << " ms" );
            // all but the last sub-duration last for t_sub_duration ms
            // conditions that will break the loop:
            //   - all sub-durations have been completed
            //   - last sub-duration was not stopped for a timeout (the other possibility is that f_run_stopper was notified by e.g. stop_run())
            //   - run_control has been canceled

            time_point_t t_run_start = std::chrono::steady_clock::now();
            time_point_t t_run_end = t_run_start + t_run_duration;

            while( std::chrono::steady_clock::now() < t_run_end && ! f_do_break_run && ! is_canceled() )
            {
                // we use wait_until so that we can break the run up with subdurations and not worry about whether a subduration was interrupted by a spurious wakeup
                f_run_stopper.wait_until( t_run_stop_lock, std::min( std::chrono::steady_clock::now() + t_sub_duration, t_run_end ) );
            }

        }

        // if we've reached here, we need to pause midge.
        // reasons for this include the timer has run out in a timed run, or the run has been manually stopped

        LDEBUG( plog, "Run stopper has been released" );

        if( f_midge_pkg.have_lock() ) f_midge_pkg->instruct( midge::instruction::pause );
        set_status( status::activated );

        LINFO( plog, "Run has stopped" );
        f_msg_relay->slack_notice( "Run has stopped" );

        if( f_do_break_run ) LINFO( plog, "Run was stopped manually" );
        if( is_canceled() ) LINFO( plog, "Run was cancelled" );

        this->on_post_run();

        return;
    }

    void run_control::stop_run()
    {
        LINFO( plog, "Run stop requested" );

        if( get_status() != status::running ) return;

        if( ! f_midge_pkg.have_lock() ) LWARN( plog, "Do not have midge resource" );

        std::unique_lock< std::mutex > t_run_stop_lock( f_run_stop_mutex );
        f_do_break_run = true;
        f_run_stopper.notify_all();

        return;
    }

    void run_control::do_cancellation( int a_code )
    {
        LDEBUG( plog, "Canceling DAQ control" );

        if( get_status() == status::running )
        {
            LDEBUG( plog, "Canceling run" );
            try
            {
                stop_run();
            }
            catch( error& e )
            {
                LERROR( plog, "Unable to complete stop-run: " << e.what() );
            }
        }

        LDEBUG( plog, "Canceling midge" );
        if( f_midge_pkg.have_lock() ) f_midge_pkg->cancel( a_code );

        set_status( status::canceled );

        return;
    }

    void run_control::apply_config( const std::string& a_node_name, const scarab::param_node& a_config )
    {
        if( f_node_bindings == nullptr )
        {
            throw error() << "Can't apply config to node <" << a_node_name << ">: node bindings aren't available";
        }

        active_node_bindings::iterator t_binding_it = f_node_bindings->find( a_node_name );
        if( t_binding_it == f_node_bindings->end() )
        {
            throw error() << "Can't apply config to node <" << a_node_name << ">: did not find node";
        }

        try
        {
            LDEBUG( plog, "Applying config to active node <" << a_node_name << ">: " << a_config );
            t_binding_it->second.first->apply_config( t_binding_it->second.second, a_config );
        }
        catch( std::exception& e )
        {
            throw error() << "Can't apply config to node <" << a_node_name << ">: " << e.what();
        }
        return;
    }

    void run_control::dump_config( const std::string& a_node_name, scarab::param_node& a_config )
    {
        if( f_node_bindings == nullptr )
        {
            throw error() << "Can't dump config from node <" << a_node_name << ">: node bindings aren't available";
        }

        if( f_node_bindings == nullptr )
        {
            throw error() << "Can't dump config from node <" << a_node_name << ">: node bindings aren't available";
        }

        active_node_bindings::iterator t_binding_it = f_node_bindings->find( a_node_name );
        if( t_binding_it == f_node_bindings->end() )
        {
            throw error() << "Can't dump config from node <" << a_node_name << ">: did not find node";
        }

        try
        {
            LDEBUG( plog, "Dumping config from active node <" << a_node_name << ">" );
            t_binding_it->second.first->dump_config( t_binding_it->second.second, a_config );
        }
        catch( std::exception& e )
        {
            throw error() << "Can't dump config from node <" << a_node_name << ">: " << e.what();
        }
        return;
    }

    bool run_control::run_command( const std::string& a_node_name, const std::string& a_cmd, const scarab::param_node& a_args )
    {
        if( f_node_bindings == nullptr )
        {
            throw error() << "Can't run command <" << a_cmd << "> on node <" << a_node_name << ">: node bindings aren't available";
        }

        if( f_node_bindings == nullptr )
        {
            throw error() << "Can't run command <" << a_cmd << "> on node <" << a_node_name << ">: node bindings aren't available";
        }

        active_node_bindings::iterator t_binding_it = f_node_bindings->find( a_node_name );
        if( t_binding_it == f_node_bindings->end() )
        {
            throw error() << "Can't run command <" << a_cmd << "> on node <" << a_node_name << ">: did not find node";
        }

        try
        {
            LDEBUG( plog, "Running command <" << a_cmd << "> on active node <" << a_node_name << ">" );
            return t_binding_it->second.first->run_command( t_binding_it->second.second, a_cmd, a_args );
        }
        catch( std::exception& e )
        {
            throw error() << "Can't run command <" << a_cmd << "> on node <" << a_node_name << ">: " << e.what();
        }
    }


    dripline::reply_ptr_t run_control::handle_activate_run_control( const dripline::request_ptr_t a_request )
    {
        try
        {
            activate();
            return a_request->reply( dripline::dl_success(), "DAQ control activated" );
        }
        catch( error& e )
        {
            return a_request->reply( dripline::dl_service_error(), string( "Unable to activate DAQ control: " ) + e.what() );
        }
    }

    dripline::reply_ptr_t run_control::handle_reactivate_run_control( const dripline::request_ptr_t a_request )
    {
        try
        {
            reactivate();
            return a_request->reply( dripline::dl_success(), "DAQ control reactivated" );
        }
        catch( error& e )
        {
            return a_request->reply( dripline::dl_service_error(), string( "Unable to reactivate DAQ control: " ) + e.what() );
        }
    }

    dripline::reply_ptr_t run_control::handle_deactivate_run_control( const dripline::request_ptr_t a_request )
    {
        try
        {
            deactivate();
            return a_request->reply( dripline::dl_success(), "DAQ control deactivated" );
        }
        catch( error& e )
        {
            return a_request->reply( dripline::dl_service_error(), string( "Unable to deactivate DAQ control: " ) + e.what() );
        }
    }

    dripline::reply_ptr_t run_control::handle_start_run_request( const dripline::request_ptr_t a_request )
    {
        try
        {
            start_run();
            return a_request->reply( dripline::dl_success(), "Run started" );
        }
        catch( std::exception& e )
        {
            LWARN( plog, "there was an error starting a run" );
            return a_request->reply( dripline::dl_service_error(), string( "Unable to start run: " ) + e.what() );
        }
    }

    dripline::reply_ptr_t run_control::handle_stop_run_request( const dripline::request_ptr_t a_request )
    {
        try
        {
            stop_run();
            return a_request->reply( dripline::dl_success(), "Run stopped" );
        }
        catch( error& e )
        {
            return a_request->reply( dripline::dl_service_error(), string( "Unable to stop run: " ) + e.what() );
        }
    }

    dripline::reply_ptr_t run_control::handle_apply_config_request( const dripline::request_ptr_t a_request )
    {
        if( a_request->parsed_specifier().size() < 2 )
        {
            return a_request->reply( dripline::dl_service_error_invalid_method(), "Specifier is improperly formatted: active-config.[stream].[node] or node-config.[stream].[node].[parameter]" );
        }

        //size_t t_rks_size = a_request->parsed_rks().size();

        std::string t_target_stream = a_request->parsed_specifier().front();
        a_request->parsed_specifier().pop_front();

        std::string t_target_node = t_target_stream + "_" + a_request->parsed_specifier().front();
        a_request->parsed_specifier().pop_front();

        param_ptr_t t_payload_ptr( new param_node() );
        param_node& t_payload = t_payload_ptr->as_node();

        if( a_request->parsed_specifier().empty() )
        {
            // payload should be a map of all parameters to be set
            LDEBUG( plog, "Performing config for multiple values in active node <" << t_target_node << ">" );

            if( ! a_request->payload().is_node() || a_request->payload().as_node().empty() )
            {
                return a_request->reply( dripline::dl_service_error_bad_payload(), "Unable to perform active-config request: payload is empty" );
            }

            try
            {
                apply_config( t_target_node, a_request->payload().as_node() );
                t_payload.merge( a_request->payload().as_node() );
            }
            catch( std::exception& e )
            {
                return a_request->reply( dripline::dl_service_error(), std::string("Unable to perform node-config request: ") + e.what() );
            }
        }
        else
        {
            // payload should be values array with a single entry for the particular parameter to be set
            LDEBUG( plog, "Performing node config for a single value in active node <" << t_target_node << ">" );

            if( ! a_request->payload().is_node() || ! a_request->payload().as_node().has( "values" ) )
            {
                return a_request->reply( dripline::dl_service_error_bad_payload(), "Unable to perform active-config (single value): values array is missing" );
            }
            scarab::param_array t_values_array;
            if ( a_request->payload().as_node().has("values") ) {
                t_values_array.append( a_request->payload()["values"].as_array() );
            }
            if( t_values_array.empty() || ! t_values_array[0].is_value() )
            {
                return a_request->reply( dripline::dl_service_error_bad_payload(), "Unable to perform active-config (single value): \"values\" is not an array, or the array is empty, or the first element in the array is not a value", std::move(t_payload_ptr) );
            }

            scarab::param_node t_param_to_set;
            t_param_to_set.add( a_request->parsed_specifier().front(), scarab::param_value( t_values_array[0].as_value() ) );

            try
            {
                apply_config( t_target_node, t_param_to_set );
                t_payload.merge( t_param_to_set );
            }
            catch( std::exception& e )
            {
                return a_request->reply( dripline::dl_service_error(), std::string("Unable to perform active-config request (single value): ") + e.what(), std::move(t_payload_ptr) );
            }
        }

        LDEBUG( plog, "Node-config was successful" );
        return a_request->reply( dripline::dl_success(), "Performed node-config", std::move(t_payload_ptr) );
    }

    dripline::reply_ptr_t run_control::handle_dump_config_request( const dripline::request_ptr_t a_request )
    {
        if( a_request->parsed_specifier().size() < 2 )
        {
            return a_request->reply( dripline::dl_service_error_invalid_specifier(), "Specifier is improperly formatted: active-config.[stream].[node] or active-config.[stream].[node].[parameter]" );
        }

        //size_t t_rks_size = a_request->parsed_rks().size();

        std::string t_target_stream = a_request->parsed_specifier().front();
        a_request->parsed_specifier().pop_front();

        std::string t_target_node = t_target_stream + "_" + a_request->parsed_specifier().front();
        a_request->parsed_specifier().pop_front();

        param_ptr_t t_payload_ptr( new param_node() );
        param_node& t_payload = t_payload_ptr->as_node();

        if( a_request->parsed_specifier().empty() )
        {
            // getting full node configuration
            LDEBUG( plog, "Getting node config for active node <" << t_target_node << ">" );

            try
            {
                dump_config( t_target_node, t_payload );
            }
            catch( std::exception& e )
            {
                return a_request->reply( dripline::dl_service_error(), std::string("Unable to perform get-active-config request: ") + e.what(), std::move(t_payload_ptr) );
            }
        }
        else
        {
            // getting value for a single parameter
            LDEBUG( plog, "Getting value for a single parameter in active node <" << t_target_node << ">" );

            std::string t_param_to_get = a_request->parsed_specifier().front();

            try
            {
                scarab::param_node t_param_dump;
                dump_config( t_target_node, t_param_dump );
                if( ! t_param_dump.has( t_param_to_get ) )
                {
                    return a_request->reply( dripline::dl_service_error_invalid_key(), "Unable to get active-node parameter: cannot find parameter <" + t_param_to_get + ">" );
                }
                t_payload.add( t_param_to_get, t_param_dump[t_param_to_get]() );
            }
            catch( std::exception& e )
            {
                return a_request->reply( dripline::dl_service_error(), std::string("Unable to get active-node parameter (single value): ") + e.what(), std::move(t_payload_ptr) );
            }
        }

        LDEBUG( plog, "Get-active-node-config was successful" );
        return a_request->reply( dripline::dl_success(), "Performed get-active-node-config", std::move(t_payload_ptr) );
    }

    dripline::reply_ptr_t run_control::handle_run_command_request( const dripline::request_ptr_t a_request )
    {
        if( a_request->parsed_specifier().size() < 2 )
        {
            return a_request->reply( dripline::dl_service_error_invalid_specifier(), "RKS is improperly formatted: run-command.[stream].[node].[command]" );
        }

        //size_t t_rks_size = a_request->parsed_rks().size();

        std::string t_target_stream = a_request->parsed_specifier().front();
        a_request->parsed_specifier().pop_front();

        std::string t_target_node = t_target_stream + "_" + a_request->parsed_specifier().front();
        a_request->parsed_specifier().pop_front();

        scarab::param_node t_args_node;
        if( a_request->payload().is_node() ) t_args_node = a_request->payload().as_node();

        std::string t_command( a_request->parsed_specifier().front() );
        a_request->parsed_specifier().pop_front();

        LDEBUG( plog, "Performing run-command <" << t_command << "> for active node <" << t_target_node << ">; args:\n" << t_args_node );

        param_ptr_t t_payload_ptr( new param_node() );
        param_node& t_payload = t_payload_ptr->as_node();

        bool t_return = false;
        try
        {
            t_return = run_command( t_target_node, t_command, t_args_node );
            t_payload.merge( t_args_node );
            t_payload.add( "command", t_command );
        }
        catch( std::exception& e )
        {
            return a_request->reply( dripline::dl_service_error(), std::string("Unable to perform run-command request: ") + e.what(), std::move(t_payload_ptr) );
        }

        if( t_return )
        {
            LDEBUG( plog, "Active run-command execution was successful" );
            return a_request->reply( dripline::dl_success(), "Performed active run-command execution", std::move(t_payload_ptr) );
        }
        else
        {
            LWARN( plog, "Active run-command execution failed" );
            return a_request->reply( dripline::dl_service_error_invalid_method(), "Command was not recognized", std::move(t_payload_ptr) );
        }
    }

    dripline::reply_ptr_t run_control::handle_set_duration_request( const dripline::request_ptr_t a_request )
    {
        try
        {
            unsigned t_new_duration = a_request->payload()["values"][0]().as_uint();
            if( t_new_duration == 0 )
            {
                throw error() << "Invalid duration: " << t_new_duration;
            }
            f_run_duration =  t_new_duration;

            LDEBUG( plog, "Duration set to <" << f_run_duration << "> ms" );
            return a_request->reply( dripline::dl_success(), "Duration set" );
        }
        catch( std::exception& e )
        {
            return a_request->reply( dripline::dl_service_error(), string( "Unable to set duration: " ) + e.what() );
        }
    }

    dripline::reply_ptr_t run_control::handle_get_status_request( const dripline::request_ptr_t a_request )
    {
        param_node t_server_node;
        t_server_node.add( "status", param_value( interpret_status( get_status() ) ) );
        t_server_node.add( "status-value", param_value( status_to_uint( get_status() ) ) );

        // TODO: add status of nodes

        param_ptr_t t_payload_ptr( new param_node() );
        t_payload_ptr->as_node().add( "server", t_server_node );

        return a_request->reply( dripline::dl_success(), "DAQ status request succeeded", std::move(t_payload_ptr) );

    }

    dripline::reply_ptr_t run_control::handle_get_duration_request( const dripline::request_ptr_t a_request )
    {
        param_array t_values_array;
        t_values_array.push_back( param_value( f_run_duration ) );

        param_ptr_t t_payload_ptr( new param_node() );
        t_payload_ptr->as_node().add( "values", t_values_array );

        return a_request->reply( dripline::dl_success(), "Duration request completed", std::move(t_payload_ptr) );
    }

    void run_control::register_handlers( std::shared_ptr< request_receiver > a_receiver_ptr )
    {
        using namespace std::placeholders;

        // set the run request handler
        a_receiver_ptr->set_run_handler( std::bind( &run_control::handle_start_run_request, this, _1 ) );

        // add get request handlers
        a_receiver_ptr->register_get_handler( "active-config", std::bind( &run_control::handle_dump_config_request, this, _1 ) );
        a_receiver_ptr->register_get_handler( "daq-status", std::bind( &run_control::handle_get_status_request, this, _1 ) );
        a_receiver_ptr->register_get_handler( "duration", std::bind( &run_control::handle_get_duration_request, this, _1 ) );

        // add set request handlers
        a_receiver_ptr->register_set_handler( "active-config", std::bind( &run_control::handle_apply_config_request, this, _1 ) );
        a_receiver_ptr->register_set_handler( "duration", std::bind( &run_control::handle_set_duration_request, this, _1 ) );

        // add cmd request handlers
        a_receiver_ptr->register_cmd_handler( "run-daq-cmd", std::bind( &run_control::handle_run_command_request, this, _1 ) );
        a_receiver_ptr->register_cmd_handler( "stop-run", std::bind( &run_control::handle_stop_run_request, this, _1 ) );
        a_receiver_ptr->register_cmd_handler( "start-run", std::bind( &run_control::handle_start_run_request, this, _1 ) );
        a_receiver_ptr->register_cmd_handler( "activate-daq", std::bind( &run_control::handle_activate_run_control, this, _1 ) );
        a_receiver_ptr->register_cmd_handler( "reactivate-daq", std::bind( &run_control::handle_reactivate_run_control, this, _1 ) );
        a_receiver_ptr->register_cmd_handler( "deactivate-daq", std::bind( &run_control::handle_deactivate_run_control, this, _1 ) );

        this->derived_register_handlers( a_receiver_ptr );

        return;
    }

    uint32_t run_control::status_to_uint( status a_status )
    {
        return static_cast< uint32_t >( a_status );
    }
    run_control::status run_control::uint_to_status( uint32_t a_value )
    {
        return static_cast< status >( a_value );
    }
    std::string run_control::interpret_status( status a_status )
    {
        switch( a_status )
        {
            case status::deactivated:
                return std::string( "Deactivated" );
                break;
            case status::activating:
                return std::string( "Activating" );
                break;
            case status::activated:
                return std::string( "Activated" );
                break;
            case status::running:
                return std::string( "Running" );
                break;
            case status::deactivating:
                return std::string( "Deactivating" );
                break;
            case status::canceled:
                return std::string( "Canceled" );
                break;
            case status::done:
                return std::string( "Done" );
                break;
            case status::do_restart:
                return std::string( "Do Restart" );
                break;
            case status::error:
                return std::string( "Error" );
                break;
            default:
                return std::string( "Unknown" );
        }
    }


} /* namespace sandfly */
