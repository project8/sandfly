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

    message_relayer::message_relayer( const scarab::param_node& a_config ) :
            dripline::relayer( a_config ),
            f_queue_name( a_config.get_value( "queue", "sandfly" ) ),
            f_use_relayer( false )
    {
    }

    message_relayer::~message_relayer()
    {
    }

    void message_relayer::slack_notice( const std::string& a_msg_text ) const
    {
        if( ! f_use_relayer ) return;
        scarab::param_ptr_t t_msg_ptr( new scarab::param_node() );
        scarab::param_node& t_msg = t_msg_ptr->as_node();
        t_msg.add( "message", scarab::param_value( a_msg_text ) );
        dripline::relayer::send_async( dripline::msg_alert::create( std::move(t_msg_ptr), "status_message.notice." + f_queue_name ) );
        return;
    }

    void  message_relayer::slack_warn( const std::string& a_msg_text ) const
    {
        if( ! f_use_relayer ) return;
        scarab::param_ptr_t t_msg_ptr( new scarab::param_node() );
        scarab::param_node& t_msg = t_msg_ptr->as_node();
        t_msg.add( "message", scarab::param_value( a_msg_text ) );
        send_async( dripline::msg_alert::create( std::move(t_msg_ptr), "status_message.warning." + f_queue_name ) );
        return;
    }

    void  message_relayer::slack_error( const std::string& a_msg_text ) const
    {
        if( ! f_use_relayer ) return;
        scarab::param_ptr_t t_msg_ptr( new scarab::param_node() );
        scarab::param_node& t_msg = t_msg_ptr->as_node();
        t_msg.add( "message", scarab::param_value( a_msg_text ) );
        send_async( dripline::msg_alert::create( std::move(t_msg_ptr), "status_message.error." + f_queue_name ) );
        return;
    }

    void  message_relayer::slack_critical( const std::string& a_msg_text ) const
    {
        if( ! f_use_relayer ) return;
        scarab::param_ptr_t t_msg_ptr( new scarab::param_node() );
        scarab::param_node& t_msg = t_msg_ptr->as_node();
        t_msg.add( "message", scarab::param_value( a_msg_text ) );
        send_async( dripline::msg_alert::create( std::move(t_msg_ptr), "status_message.critical." + f_queue_name ) );
        return;
    }


} /* namespace sandfly */
