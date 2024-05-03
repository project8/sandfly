#ifndef SANDFLY_ERROR_HH_
#define SANDFLY_ERROR_HH_

#include "base_exception.hh"

namespace sandfly
{

    class error : public scarab::typed_exception< error >
    {
        public:
            error();
            error( const error& p_copy );
            error& operator=( const error& p_copy );
            virtual ~error() noexcept;
    };

    class fatal_error : public scarab::typed_exception< fatal_error >
    {
        public:
            fatal_error();
            fatal_error( const fatal_error& p_copy );
            fatal_error& operator=( const fatal_error& p_copy );
            virtual ~fatal_error() noexcept;
    };

} /* namespace sandfly */

#endif /* SANDFLY_ERROR_HH_ */
