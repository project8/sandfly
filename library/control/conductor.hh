/*
 * conductor.hh
 *
 *  Created on: May 6, 2015
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_CONDUCTOR_HH_
#define SANDFLY_CONDUCTOR_HH_

//#include "mt_version.hh"

#include "hub.hh"

#include "param.hh"

#include "cancelable.hh"

#include <atomic>
#include <mutex>

namespace scarab
{
    class authentication;
    class version_semantic;
}

namespace sandfly
{
    class batch_executor;
    class message_relayer;
    class run_control;
    class request_receiver;
    class stream_manager;

    /*!
     @class conductor
     @author N.S. Oblath

     @brief Sets up all control classes.  Registers request handlers.  Coordinates threads.

     @details
     The conductor is responsible for coordinating all actions in sandfly.

     Control classes managed by the conductor include batch_executor, run_control, request_receiver, and stream_manager.
     A derived run_control type can be used in place of run_control either by providing the type as a template 
     argument to conductor.execute(), or by first setting it with conductor.set_rc_creator().

     A conductor instance should be created by the executable. 
     The executable calls conductor.execute() and waits for it's return.
     If use of a message_relayer is desired, it should be supplied to conductor.execute().  This argument is optional.

     In execute(), conductor creates new instances of run_control, stream_manager and request_receiver.
     It also adds set, get and cmd request handlers by registering handlers with the request_receiver.
     Then it calls run_control.execute and request_receiver.execute in 2 separate threads.
     conductor.execute() only returns when all threads are joined.

     */
    class conductor : public scarab::cancelable
    {
        public:
            conductor();
            virtual ~conductor();

            /// Start the conductor (using the existing run_control type)
            void execute( const scarab::param_node& a_config, const scarab::authentication& a_auth, std::shared_ptr< message_relayer > a_relayer = nullptr );
            /// Start the conductor using the provided run_control type
            template< typename rc_type >
            void execute( const scarab::param_node& a_config, const scarab::authentication& a_auth, std::shared_ptr< message_relayer > a_relayer = nullptr );

            /// Asynchronous function to tell the conductor to quit
            void quit_server();

            int get_return() const;

            dripline::reply_ptr_t handle_get_server_status_request( const dripline::request_ptr_t a_request );

            dripline::reply_ptr_t handle_stop_all_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_quit_server_request( const dripline::request_ptr_t a_request );

            template< typename rc_type = run_control >
            void set_rc_creator();

        private:
            virtual void do_cancellation( int a_code );

            int f_return;

            std::function< std::shared_ptr< run_control >(const scarab::param_node&, std::shared_ptr< stream_manager >, std::shared_ptr< message_relayer >) > f_rc_creator;

            // component pointers for asynchronous access
            std::shared_ptr< request_receiver > f_request_receiver;
            std::shared_ptr< batch_executor > f_batch_executor;
            //std::shared_ptr< daq_worker > f_daq_worker;
            std::shared_ptr< run_control > f_run_control;
            std::shared_ptr< stream_manager > f_stream_manager;
            std::shared_ptr< message_relayer > f_message_relayer;

            std::mutex f_component_mutex;

        public:
            enum status
            {
                k_initialized = 0,
                k_starting = 1,
                k_running = 5,
                k_done = 10,
                k_error = 100
            };

            static std::string interpret_status( status a_status );

            status get_status() const;
            void set_status( status a_status );

        private:
            std::atomic< status > f_status;

    };

    template< typename rc_type >
    void conductor::execute( const scarab::param_node& a_config, const scarab::authentication& a_auth, std::shared_ptr< message_relayer > a_relayer )
    {
        set_rc_creator< rc_type >();
        execute( a_config, a_auth, a_relayer );
        return;
    }

    template< typename rc_type >
    void conductor::set_rc_creator()
    {
        f_rc_creator = []( const scarab::param_node& a_config, std::shared_ptr< stream_manager > a_mgr, std::shared_ptr< message_relayer > a_rly )->std::shared_ptr< run_control >{ 
            return std::make_shared< rc_type >( a_config, a_mgr, a_rly ); 
        };
        return;
    }

    inline int conductor::get_return() const
    {
        return f_return;
    }

    inline conductor::status conductor::get_status() const
    {
        return f_status.load();
    }

    inline void conductor::set_status( status a_status )
    {
        f_status.store( a_status );
        return;
    }

} /* namespace sandfly */

#endif /* SANDFLY_CONDUCTOR_HH_ */
