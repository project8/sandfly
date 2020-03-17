/*
 * daq_control.hh
 *
 *  Created on: Jan 22, 2016
 *      Author: nsoblath
 */

#ifndef SANDFLY_DAQ_CONTROL_HH_
#define SANDFLY_DAQ_CONTROL_HH_

#include "control_access.hh"
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
    class message_relayer;

    /*!
     @class daq_control
     @author N. S. Oblath

     @brief Controls psyllid's status and forwards requests to the DAQ nodes.

     @details
     Handles all requests received via request_receiver that don't affect the stream setup directly.
     It switches between daq-states, starts and stops runs by un-pausing and pausing midge and forwards run-daq-commands.

     DAQ states:
     - Deactivated: initial state and state when Midge is not running.
     - Activating: an instruction to activate has been received, and the DAQ control is in the process of getting things going.
     - Activated: Midge is running; the DAQ is ready to go.
     - Deactivating: an instruction to deactivate has been received, and the DAQ control is in the process of stopping things.
     - Canceled: the DAQ control has received an instruction from on high to exit immediately, and is in the process of doing so.
     - Do Restart: Midge experienced a non-fatal error.  The DAQ should be restarted and things will be fine.
     - Done: the DAQ control has completed operation and is ready for Psyllid to exit.
     - Error: something went wrong; Psyllid should be restarted.

     Startup readiness: whether the DAQ control is ready for use after startup depends on whether
     it was told to activate on startup or not:
     - YES, activate on startup: DAQ control must be in the "activated" state to be ready
     - NO, wait to activate: DAQ control can be in the "deactivate," "activating,", or "activated" states to be ready

     Settings that can be applied in the "daq" section of the master config:
     - "duration" (integer): the duration of the next run in ms
     - "activate-at-startup" (boolean): whether or not the DAQ control is activated immediately on startup
     - "n-files" (integer): number of files that will be written in parallel
     - "max-file-size-mb" (float): maximum egg file size; writing will switch to a new file after that is reached
     */
    class daq_control : public scarab::cancelable, public control_access
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
            daq_control( const scarab::param_node& a_master_config, std::shared_ptr< stream_manager > a_mgr );
            virtual ~daq_control();

            /// Pre-execution initialization (call after setting the control_access pointer)
            void initialize();

            /// Run the DAQ control thread
            void execute( std::condition_variable& a_ready_condition_variable, std::mutex& a_ready_mutex );

            /// Start the DAQ into the activated state
            /// Can throw daq_control::status_error; daq_control will still be usable
            /// Can throw psyllid::error; daq_control will NOT be usable
            void activate();
            /// Restarts the DAQ into the activated state
            /// Can throw daq_control::status_error; daq_control will still be usable
            /// Can throw psyllid::error; daq_control will NOT be usable
            void reactivate();
            /// Returns the DAQ to the deactivated state
            /// Can throw daq_control::status_error; daq_control will still be usable
            /// Can throw psyllid::error; daq_control will NOT be usable
            void deactivate();

            bool is_ready_at_startup() const;

            /// Start a run
            /// Can throw daq_control::run_error or status_error; daq_control will still be usable
            /// Can throw psyllid::error; daq_control will NOT be usable
            /// Stop run with stop_run()
            void start_run();

            /// Stop a run
            /// Can throw daq_control::run_error or status_error; daq_control will still be usable
            /// Can throw psyllid::error; daq_control will NOT be usable
            void stop_run();

        public:
            void apply_config( const std::string& a_node_name, const scarab::param_node& a_config );
            void dump_config( const std::string& a_node_name, scarab::param_node& a_config );

            /// Instruct a node to run a command
            /// Throws psyllid::error if the command fails; returns false if the command is not recognized
            bool run_command( const std::string& a_node_name, const std::string& a_cmd, const scarab::param_node& a_args );

        public:
            dripline::reply_ptr_t handle_activate_daq_control( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_reactivate_daq_control( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_deactivate_daq_control( const dripline::request_ptr_t a_request );

            dripline::reply_ptr_t handle_start_run_request( const dripline::request_ptr_t a_request );

            dripline::reply_ptr_t handle_stop_run_request( const dripline::request_ptr_t a_request );

            dripline::reply_ptr_t handle_apply_config_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_dump_config_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_run_command_request( const dripline::request_ptr_t a_request );

            dripline::reply_ptr_t handle_set_filename_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_set_description_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_set_duration_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_set_use_monarch_request( const dripline::request_ptr_t a_request );

            dripline::reply_ptr_t handle_get_status_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_get_filename_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_get_description_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_get_duration_request( const dripline::request_ptr_t a_request );
            dripline::reply_ptr_t handle_get_use_monarch_request( const dripline::request_ptr_t a_request );

        private:
            void do_cancellation( int a_code );

            void do_run( unsigned a_duration );

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

            message_relayer* f_msg_relay;

        public:
            void set_filename( const std::string& a_filename, unsigned a_file_num = 0 );
            const std::string& get_filename( unsigned a_file_num = 0 );

            void set_description( const std::string& a_desc, unsigned a_file_num = 0 );
            const std::string& get_description( unsigned a_file_num = 0 );

            mv_accessible( unsigned, run_duration );

            mv_accessible( bool, use_monarch );

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

        private:
            std::atomic< status > f_status;


    };

    inline daq_control::status daq_control::get_status() const
    {
        return f_status.load();
    }

    inline void daq_control::set_status( status a_status )
    {
        f_status.store( a_status );
        return;
    }


} /* namespace sandfly */

#endif /* SANDFLY_DAQ_CONTROL_HH_ */
