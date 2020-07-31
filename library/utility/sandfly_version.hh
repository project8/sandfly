/*
 * sandfly_version.hh
 *
 *  Created on: Mar 20, 2013
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_VERSION_HH_
#define SANDFLY_VERSION_HH_

#include "dripline_version.hh"

namespace sandfly
{
    class version : public scarab::version_semantic
    {
        public:
            version();
            ~version();
    };

} // namespace sandfly

#endif /* SANDFLY_VERSION_HH_ */
