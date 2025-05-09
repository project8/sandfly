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
            void send_notice( const std::string& a_msg_text ) const ;
            void send_warn( const std::string& a_msg_text ) const ;
            void send_error( const std::string& a_msg_text ) const ;
            void send_critical( const std::string& a_msg_text ) const ;

            void send_notice( scarab::param_ptr_t&& a_payload ) const ;
            void send_warn( scarab::param_ptr_t&& a_payload ) const ;
            void send_error( scarab::param_ptr_t&& a_payload ) const ;
            void send_critical( scarab::param_ptr_t&& a_payload ) const ;

            mv_referrable( std::string, queue_name );
            mv_accessible( bool, use_relayer );
    };


} /* namespace sandfly */

#endif /* SANDFLY_MESSAGE_RELAYER_HH_ */
