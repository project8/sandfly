/*
 * sandfly_return_codes.cc
 *
 *  Created on: Mar 17, 2020
 *      Author: N.S. Oblath
 */

#include "sandfly_return_codes.hh"

namespace sandfly
{
    IMPLEMENT_DL_RET_CODE( sandfly_error, 1100, "Generic Sandfly system error" );
    IMPLEMENT_DL_RET_CODE( sandfly_not_enabled, 1101, "Sandfly system is not enabled" );
    IMPLEMENT_DL_RET_CODE( sandfly_running, 1102, "Sandfly system is running" );
}
