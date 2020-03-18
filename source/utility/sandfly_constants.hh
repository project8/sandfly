/*
 * sandfly_constants.hh
 *
 *  Created on: Jan 23, 2015
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_CONSTANTS_HH_
#define SANDFLY_CONSTANTS_HH_

#include "return_codes.hh"

namespace sandfly
{
    DEFINE_DL_RET_CODE_NOAPI( daq_error );
    DEFINE_DL_RET_CODE_NOAPI( daq_not_enabled );
    DEFINE_DL_RET_CODE_NOAPI( daq_running );
}

#endif /* SANDFLY_CONSTANTS_HH_ */
