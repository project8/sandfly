#ifndef SANDFLY_REQUEST_RECEIVER_HH_
#define SANDFLY_REQUEST_RECEIVER_HH_

#include "control_access.hh"

// dripline
#include "hub.hh"
#include "message.hh"

#include "param.hh"
#include "cancelable.hh"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>

namespace scarab
{
    class authentication;
}

namespace sandfly
{
    class run_server;
    class run_control;
    /*!
     @class request_receiver
     @author N. S. Oblath

     @brief Receives request from a amqp broker.

     @details
     A request_receiver instance is initialized by run_server.
     request_receiver holds maps for set, get, cmd and run requests.
     When a request is received the handle_function registered with this request gets called.
     The registration of requests and functions is done in dripline::hub.
     */
    class request_receiver : public dripline::hub, public control_access
    {
        public:
            request_receiver( const scarab::param_node& a_config, const scarab::authentication& a_auth );
            virtual ~request_receiver();

            void execute( std::condition_variable& a_run_control_ready_cv, std::mutex& a_run_control_ready_mutex );

            mv_referrable_const( scarab::param_node, set_conditions );
        private:
            virtual void do_cancellation( int a_code );

        public:
            enum status
            {
                k_initialized = 0,
                k_starting = 1,
                k_listening = 5,
                k_canceled = 9,
                k_done = 10,
                k_error = 100
            };

            static std::string interpret_status( status a_status );

            status get_status() const;
            void set_status( status a_status );

        private:
            std::atomic< status > f_status;

            virtual dripline::reply_ptr_t __do_handle_set_condition_request( const dripline::request_ptr_t a_request );

    };

    inline request_receiver::status request_receiver::get_status() const
    {
        return f_status.load();
    }

    inline void request_receiver::set_status( status a_status )
    {
        f_status.store( a_status );
        return;
    }

}

#endif
