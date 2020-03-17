#ifndef SANDFLY_ERROR_HH_
#define SANDFLY_ERROR_HH_

#include "base_exception.hh"

namespace sandfly
{

    class error : public scarab::base_exception< error >
    {
        public:
            error();
            error( const error& p_copy );
            error& operator=( const error& p_copy );
            virtual ~error() noexcept;
    };

} /* namespace sandfly */

#endif /* SANDFLY_ERROR_HH_ */
