/*
 * constants.hh
 *
 *  Created on: Jan 23, 2015
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_CONSTANTS_HH_
#define SANDFLY_CONSTANTS_HH_

#include "return_codes.hh"

// New return codes have to be in the dripline namespace, per instructions in return_codes.hh
namespace dripline
{
    DEFINE_DL_RET_CODE( daq_error, 1100 );
#undef RC_LIST
#define RC_LIST NEW_RC_LIST( daq_error )
    DEFINE_DL_RET_CODE( daq_not_enabled, 1101 );
#undef RC_LIST
#define RC_LIST NEW_RC_LIST( daq_not_enabled )
    DEFINE_DL_RET_CODE( daq_running, 1102 );
#undef RC_LIST
#define RC_LIST NEW_RC_LIST( daq_running )

}

#endif /* SANDFLY_CONSTANTS_HH_ */
