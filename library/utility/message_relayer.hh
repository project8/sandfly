/*
 * message_relayer.hh
 *
 *  Created on: Jun 28, 2017
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_MESSAGE_RELAYER_HH_
#define SANDFLY_MESSAGE_RELAYER_HH_

#include "relayer.hh"

#include "singleton.hh"

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
            virtual ~message_relayer();

        public:
            void slack_notice( const std::string& a_msg_text ) const;
            void slack_warn( const std::string& a_msg_text ) const;
            void slack_error( const std::string& a_msg_text ) const;
            void slack_critical( const std::string& a_msg_text ) const;

            mv_referrable( std::string, queue_name );
            mv_accessible( bool, use_relayer );

        protected:
            void send_to_slack( const std::string& a_msg_text, const std::string& a_rk_root ) const;
    };

} /* namespace sandfly */

#endif /* SANDFLY_MESSAGE_RELAYER_HH_ */
