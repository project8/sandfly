/*
 * control_access.cc
 *
 *  Created on: Feb 23, 2016
 *      Author: nsoblath
 */

#include "control_access.hh"

#include "daq_control.hh"

#include "logger.hh"

namespace sandfly
{
    LOGGER( plog, "control_access" );

    std::weak_ptr< daq_control > control_access::f_daq_control = std::weak_ptr< daq_control >();

    control_access::control_access()
    {
    }

    control_access::~control_access()
    {
    }

    void control_access::set_daq_control( std::weak_ptr< daq_control > a_daq_control )
    {
        f_daq_control = a_daq_control;
        LDEBUG( plog, "DAQ control access set; is valid? " << ! f_daq_control.expired() );
    }

} /* namespace sandfly */
