/*
 * sandfly_return_codes.hh
 *
 *  Created on: Jan 23, 2015
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_RETURN_CODES_HH_
#define SANDFLY_RETURN_CODES_HH_

#include "return_codes.hh"

namespace sandfly
{
    DEFINE_DL_RET_CODE_NOAPI( sandfly_error );
    DEFINE_DL_RET_CODE_NOAPI( sandfly_not_enabled );
    DEFINE_DL_RET_CODE_NOAPI( sandfly_running );
}

#endif /* SANDFLY_RETURN_CODES_HH_ */
