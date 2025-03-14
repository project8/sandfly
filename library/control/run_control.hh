/*
 * run_control.hh
 *
 *  Created on: Jan 22, 2016
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_RUN_CONTROL_HH_
#define SANDFLY_RUN_CONTROL_HH_

#include "control_access.hh"
#include "message_relayer.hh"
#include "stream_manager.hh" // for midge_package
#include "sandfly_error.hh"

#include "cancelable.hh"
#include "member_variables.hh"

#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace sandfly
{
    class request_receiver;

    /*!
     @class run_control
     @author N.S. Oblath

     @brief Controls Sandfly's status and forwards requests to the processing nodes.

     @details
     Handles all requests received via request_receiver that don't affect the stream setup directly.
     It switches between states, and starts and stops runs by un-pausing and pausing midge.

     States:
     - Deactivated: initial state and state when Midge is not running.
     - Activating: an instruction to activate has been received, and the run control is in the process of getting things going.
     - Activated: Midge is running; the processing nodes are ready to go.
     - Deactivating: an instruction to deactivate has been received, and the run control is in the process of stopping things.
     - Canceled: the run control has received an instruction from on high to exit immediately, and is in the process of doing so.
     - Do Restart: Midge experienced a non-fatal error.  The run should be restarted and things will be fine.
     - Done: the run control has completed operation and is ready for Sandfly to exit.
     - Error: something went wrong; Sandfly should be restarted.

     Startup readiness: whether the run control is ready for use after startup depends on whether
     it was told to activate on startup or not:
     - YES, activate on startup: run control must be in the "activated" state to be ready
     - NO, wait to activate: run control can be in the "deactivate," "activating,", or "activated" states to be ready

     Settings that can be applied in the "daq" section of the global config:
     - "duration" (integer): the duration of the next run in ms
     - "activate-at-startup" (boolean): whether or not the DAQ control is activated immediately on startup

     Developer notes:
     - Even though run_control's constructor has a default argument for the message_relayer, if you derive a class from 
       run_control, the constructor should still have all three arguments.  This allows conductor to propertly create 
       the specified run control type with all three arguments (even if the relayer isn't being used).
     */
    class run_control : public scarab::cancelable, public control_access
    {
        public:
            class run_error : public error
            {
                public:
                    run_error() = default;
                    virtual ~run_error() = default;
            };
            class status_error : public error
            {
                public:
                    status_error() = default;
                    virtual ~status_error() = default;
            };

        public:
            run_control( const scarab::param_node& a_config, std::shared_ptr< stream_manager > a_mgr, std::shared_ptr< message_relayer > a_msg_relay = std::make_shared< null_relayer >() );
            virtual ~run_control() = default;

            /// Pre-execution initialization (call after setting the control_access pointer)
            void initialize();

            /// Run the DAQ control thread
            void execute( std::condition_variable& a_ready_condition_variable, std::mutex& a_ready_mutex );

            /// Start the DAQ into the activated state
            /// Can throw run_control::status_error; run_control will still be usable
            /// Can throw sandfly::error; run_control will NOT be usable
            void activate();
            /// Restarts the DAQ into the activated state
            /// Can throw run_control::status_error; run_control will still be usable
            /// Can throw sandfly::error; run_control will NOT be usable
            void reactivate();
            /// Returns the DAQ to the deactivated state
            /// Can throw run_control::status_error; run_control will still be usable
            /// Can throw sandfly::error; run_control will NOT be usable
            void deactivate();

            bool is_ready_at_startup() const;

            /// Start a run
            /// Can throw run_control::run_error or status_error; run_control will still be usable
            /// Can throw sandfly::error; run_control will NOT be usable
            /// Stop run with stop_run()
            void start_run();

            /// Stop a run
            /// Can throw run_control::run_error or status_error; run_control will still be usable
            /// Can throw sandfly::error; run_control will NOT be usable
            void stop_run();

        protected:
            /// Handle called to perform initialization
            virtual void on_initialize() {}
            /// Handle called just before the midge package is run
            virtual void on_pre_midge_run() {}
            /// Handle called just after the midge package finishes running (both for successful and unsuccessful exit conditions)
            virtual void on_post_midge_run() {}
            /// Handle called when control is complete and exiting normally
            virtual void on_done() {}
            /// Handle called when an error has been encountered
            virtual void on_error() {}
            /// Handle called when performing asynchronous activation (just before notifying the condition variable)
            virtual void on_activate() {}
            /// Handle called when deactivating (just before canceling midge)
            virtual void on_deactivate() {}
            /// Handle called before run takes place (i.e. control is unpaused)
            virtual void on_pre_run() {}
            /// Handle called after run finishes (i.e. control is paused)
            virtual void on_post_run() {}


        public:
            void apply_config( const std::string& a_node_name, const scarab::param_node& a_config );
            void dump_config( const std::string& a_node_name, scarab::param_node& a_config );

            /// Instruct a node to run a command
            /// Throws sandfly::error if the command fails; returns false if the command is not recognized
            bool run_command( const std::string& a_node_name, const std::string& a_cmd, const scarab::param_node& a_args );

        public:
            virtual dripline::reply_ptr_t handle_activate_run_control( const dripline::request_ptr_t a_request );
            virtual dripline::reply_ptr_t handle_reactivate_run_control( const dripline::request_ptr_t a_request );
            virtual dripline::reply_ptr_t handle_deactivate_run_control( const dripline::request_ptr_t a_request );

            virtual dripline::reply_ptr_t handle_start_run_request( const dripline::request_ptr_t a_request );

            virtual dripline::reply_ptr_t handle_stop_run_request( const dripline::request_ptr_t a_request );

            virtual dripline::reply_ptr_t handle_apply_config_request( const dripline::request_ptr_t a_request );
            virtual dripline::reply_ptr_t handle_dump_config_request( const dripline::request_ptr_t a_request );
            virtual dripline::reply_ptr_t handle_run_command_request( const dripline::request_ptr_t a_request );

            virtual dripline::reply_ptr_t handle_set_duration_request( const dripline::request_ptr_t a_request );

            virtual dripline::reply_ptr_t handle_get_status_request( const dripline::request_ptr_t a_request );
            virtual dripline::reply_ptr_t handle_get_duration_request( const dripline::request_ptr_t a_request );

            void register_handlers( std::shared_ptr< request_receiver > a_receiver_ptr );

            const message_relayer& relayer() const;

        protected:
            void do_cancellation( int a_code );

            void do_run( unsigned a_duration );

            virtual void derived_register_handlers( std::shared_ptr< request_receiver > ) {}

            std::condition_variable f_activation_condition;
            std::mutex f_daq_mutex;

            std::shared_ptr< stream_manager > f_node_manager;

            scarab::param_node  f_daq_config;

            midge_package f_midge_pkg;
            active_node_bindings* f_node_bindings;

            std::condition_variable f_run_stopper; // ends the run after a given amount of time
            std::mutex f_run_stop_mutex; // mutex used by the run_stopper
            bool f_do_break_run; // bool to confirm that the run should stop; protected by f_run_stop_mutex

            std::future< void > f_run_return;

            std::shared_ptr< message_relayer > f_msg_relay;

        public:
            mv_accessible( unsigned, run_duration );

        public:
            enum class status:uint32_t
            {
                deactivated = 0,
                activating = 2,
                activated = 4,
                running = 5,
                deactivating = 6,
                canceled = 8,
                do_restart = 9,
                done = 10,
                error = 200
            };

            static uint32_t status_to_uint( status a_status );
            static status uint_to_status( uint32_t a_value );
            static std::string interpret_status( status a_status );

            status get_status() const;
            void set_status( status a_status );

        protected:
            std::atomic< status > f_status;


    };

    inline run_control::status run_control::get_status() const
    {
        return f_status.load();
    }

    inline void run_control::set_status( status a_status )
    {
        f_status.store( a_status );
        return;
    }

    inline const message_relayer& run_control::relayer() const
    {
        return *f_msg_relay;
    }


} /* namespace sandfly */

#endif /* SANDFLY_RUN_CONTROL_HH_ */
