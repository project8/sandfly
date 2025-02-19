/*
 * message_relayer.cc
 *
 *  Created on: Jun 28, 2017
 *      Author: N.S. Oblath
 */

#include "message_relayer.hh"

#include "authentication.hh"
#include "param.hh"
#include "param_helpers_impl.hh"

using scarab::param_node;
using_param_args_and_kwargs;

namespace sandfly
{

    message_relayer::message_relayer( const param_node& a_config, const scarab::authentication& a_auth ) :
            dripline::relayer( a_config, a_auth ),
            f_queue_name( a_config.get_value( "queue", "sandfly" ) ),
            f_use_relayer( a_config.get_value( "use-relayer", false ) )
    {}

    
    null_relayer::null_relayer() :
            message_relayer( param_node( "dripline_mesh"_a=param_node() ), scarab::authentication() )
    {}

    void null_relayer::send_notice( const std::string& a_msg_text ) const
    {
        return;
    }

    void null_relayer::send_warn( const std::string& a_msg_text ) const
    {
        return;
    }

    void null_relayer::send_error( const std::string& a_msg_text ) const
    {
        return;
    }

    void null_relayer::send_critical( const std::string& a_msg_text ) const
    {
        return;
    }

} /* namespace sandfly */
