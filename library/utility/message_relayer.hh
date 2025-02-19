/*
 * message_relayer.hh
 *
 *  Created on: Jun 28, 2017
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_MESSAGE_RELAYER_HH_
#define SANDFLY_MESSAGE_RELAYER_HH_

#include "relayer.hh"


namespace scarab
{
    class param_node;
}

namespace sandfly
{

    class message_relayer : public dripline::relayer
    {
        public:
            message_relayer( const scarab::param_node& a_config, const scarab::authentication& a_auth );
            message_relayer( const message_relayer& ) = delete;
            message_relayer( message_relayer&& ) = default;
            virtual ~message_relayer() = default;

            message_relayer& operator=( const message_relayer& ) = delete;
            message_relayer& operator=( message_relayer&& ) = default;

        public:
            virtual void send_notice( const std::string& a_msg_text ) const = 0;
            virtual void send_warn( const std::string& a_msg_text ) const = 0;
            virtual void send_error( const std::string& a_msg_text ) const = 0;
            virtual void send_critical( const std::string& a_msg_text ) const = 0;

            virtual void send_notice( scarab::param_ptr_t&& a_payload ) const = 0;
            virtual void send_warn( scarab::param_ptr_t&& a_payload ) const = 0;
            virtual void send_error( scarab::param_ptr_t&& a_payload ) const = 0;
            virtual void send_critical( scarab::param_ptr_t&& a_payload ) const = 0;

            mv_referrable( std::string, queue_name );
            mv_accessible( bool, use_relayer );
    };

    /**
     * @class null_relayer
     * @brief Concrete message_relayer class that does nothing -- no relaying messages, no connecting to a DL broker, etc
     */
    class null_relayer : public message_relayer
    {
        public:
            null_relayer();
            null_relayer( const null_relayer& ) = delete;
            null_relayer( null_relayer&& ) = default;
            virtual ~null_relayer() = default;

            null_relayer& operator=( const null_relayer& ) = delete;
            null_relayer& operator=( null_relayer&& ) = default;

        public:
            void send_notice( const std::string& ) const override;
            void send_warn( const std::string& ) const override;
            void send_error( const std::string& ) const override;
            void send_critical( const std::string& ) const override;

            void send_notice( scarab::param_ptr_t&& ) const override;
            void send_warn( scarab::param_ptr_t&& ) const override;
            void send_error( scarab::param_ptr_t&& ) const override;
            void send_critical( scarab::param_ptr_t&& ) const override;
        };

} /* namespace sandfly */

#endif /* SANDFLY_MESSAGE_RELAYER_HH_ */
