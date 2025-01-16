
#include "run_control.hh"
#include "request_receiver.hh"
#include "dripline_constants.hh"

#include "sandfly_return_codes.hh"
#include "sandfly_error.hh"

#include "authentication.hh"
#include "logger.hh"
#include "signal_handler.hh"

#include <cstddef>
#include <signal.h>
#include <sstream>

using std::string;

using scarab::param_node;
using scarab::param_ptr_t;

using dripline::request_ptr_t;


namespace sandfly
{

    LOGGER( plog, "request_receiver" );

    request_receiver::request_receiver( const param_node& a_config, const scarab::authentication& a_auth ) :
            hub( a_config["dripline_mesh"].as_node(), a_auth ),
            control_access(),
            f_set_conditions( a_config["set-conditions"].as_node() ),
            f_status( k_initialized )
    {
    }

    request_receiver::~request_receiver()
    {
    }

    void request_receiver::execute( std::condition_variable& a_run_control_ready_cv, std::mutex& a_run_control_ready_mutex )
    {
        set_status( k_starting );

        if( run_control_expired() )
        {
            LERROR( plog, "Unable to get access to the DAQ control" );
            scarab::signal_handler::cancel_all( RETURN_ERROR );
            return;
        }
        dc_ptr_t t_run_control_ptr = use_run_control();

        // start the service
        if( ! start() && f_make_connection )
        {
            LERROR( plog, "Unable to start the dripline service" );
            scarab::signal_handler::cancel_all( RETURN_ERROR );
            return;
        }

        while ( ! t_run_control_ptr->is_ready_at_startup() && ! cancelable::is_canceled() )
        {
            std::unique_lock< std::mutex > t_run_control_lock( a_run_control_ready_mutex );
            a_run_control_ready_cv.wait_for( t_run_control_lock, std::chrono::seconds(1) );
        }

        if ( f_make_connection && ! cancelable::is_canceled() ) {
            LINFO( plog, "Waiting for incoming messages" );

            set_status( k_listening );

            while( ! cancelable::is_canceled() )
            {
                // blocking call to wait for incoming message
                if( ! listen() ) 
                {
                    LERROR( plog, "An error occurred while listening for messages" );
                    set_status( k_error );
                    scarab::signal_handler::cancel_all( RETURN_ERROR );
                }
            }
        }

        LINFO( plog, "No longer waiting for messages" );

        if( ! stop() )
        {
            LERROR( plog, "An error occurred while stopping the request receiver" );
            set_status( k_error );
        }

        if( get_status() != k_error && get_status() != k_canceled ) set_status( k_done );
        LDEBUG( plog, "Request receiver is done" );

        return;
    }

    void request_receiver::do_cancellation( int )
    {
        LDEBUG( plog, "Canceling request receiver" );
        if( get_status() != k_error ) set_status( k_canceled );
        return;
    }

    std::string request_receiver::interpret_status( status a_status )
    {
        switch( a_status )
        {
            case k_initialized:
                return std::string( "Initialized" );
                break;
            case k_starting:
                return std::string( "Starting" );
                break;
            case k_listening:
                return std::string( "Listening" );
                break;
            case k_canceled:
                return std::string( "Canceled" );
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

    dripline::reply_ptr_t request_receiver::__do_handle_set_condition_request( const dripline::request_ptr_t a_request )
    {
        std::string t_condition = a_request->payload()["values"][0]().as_string();
        if ( f_set_conditions.has( t_condition ) )
        {
            std::string t_rks = f_set_conditions[t_condition]().as_string();
            dripline::request_ptr_t t_request = dripline::msg_request::create( param_ptr_t(new param_node()), dripline::op_t::cmd, a_request->routing_key(), t_rks );
            //t_request->specifier = t_rks; //, dripline::routing_key_specifier( t_rks ) );

            dripline::reply_ptr_t t_reply_ptr = submit_request_message( t_request );
            return a_request->reply( t_reply_ptr->get_return_code(), t_reply_ptr->return_message(), param_ptr_t(new param_node(t_reply_ptr->payload().as_node())) );
        }
        return a_request->reply( dl_sandfly_error(), "set condition not configured" );
    }

}
