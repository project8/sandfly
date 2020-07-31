/*
 * sandfly_constants.cc
 *
 *  Created on: Mar 17, 2020
 *      Author: N.S. Oblath
 */

#include "sandfly_constants.hh"

namespace sandfly
{
    IMPLEMENT_DL_RET_CODE( daq_error, 1100, "Generic DAQ error" );
    IMPLEMENT_DL_RET_CODE( daq_not_enabled, 1101, "DAQ system is not enabled" );
    IMPLEMENT_DL_RET_CODE( daq_running, 1102, "DAQ system is running" );
}
