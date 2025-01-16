/*
 * message_relayer.cc
 *
 *  Created on: Jun 28, 2017
 *      Author: N.S. Oblath
 */

#include "message_relayer.hh"

#include "param.hh"

namespace sandfly
{

    message_relayer::message_relayer( const scarab::param_node& a_config, const scarab::authentication& a_auth ) :
            dripline::relayer( a_config, a_auth ),
            f_queue_name( a_config.get_value( "queue", "sandfly" ) ),
            f_use_relayer( false )
    {
    }

    message_relayer::~message_relayer()
    {
    }

    void message_relayer::slack_notice( const std::string& a_msg_text ) const
    {
        send_to_slack( a_msg_text, "status_message.notice." );
        return;
    }

    void  message_relayer::slack_warn( const std::string& a_msg_text ) const
    {
        send_to_slack( a_msg_text, "status_message.warning." );
        return;
    }

    void  message_relayer::slack_error( const std::string& a_msg_text ) const
    {
        send_to_slack( a_msg_text, "status_message.error." );
        return;
    }

    void  message_relayer::slack_critical( const std::string& a_msg_text ) const
    {
        send_to_slack( a_msg_text, "status_message.critical." );
        return;
    }

    void message_relayer::send_to_slack( const std::string& a_msg_text, const std::string& a_rk_root ) const
    {
        if( ! f_use_relayer ) return;
        scarab::param_ptr_t t_msg_ptr( new scarab::param_node() );
        scarab::param_node& t_msg = t_msg_ptr->as_node();
        t_msg.add( "message", scarab::param_value( a_msg_text ) );
        send_async( dripline::msg_alert::create( std::move(t_msg_ptr), a_rk_root + f_queue_name ) );
        return;
    }

} /* namespace sandfly */
