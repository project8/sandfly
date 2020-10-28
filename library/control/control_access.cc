/*
 * control_access.cc
 *
 *  Created on: Feb 23, 2016
 *      Author: nsoblath
 */

#include "control_access.hh"

#include "run_control.hh"

#include "logger.hh"

namespace sandfly
{
    LOGGER( plog, "control_access" );

    std::weak_ptr< run_control > control_access::f_run_control = std::weak_ptr< run_control >();

    control_access::control_access()
    {
    }

    control_access::~control_access()
    {
    }

    void control_access::set_run_control( std::weak_ptr< run_control > a_run_control )
    {
        f_run_control = a_run_control;
        LDEBUG( plog, "DAQ control access set; is valid? " << ! f_run_control.expired() );
    }

} /* namespace sandfly */
