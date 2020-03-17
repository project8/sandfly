#include <iostream>

#include "sandfly_error.hh"

namespace sandfly
{

    error::error() :
            base_exception()
    {
    }
    error::error( const error& p_copy ) :
            base_exception( p_copy )
    {
    }
    error& error::operator=( const error& p_copy )
    {
        base_exception::operator=( p_copy );
        return *this;
    }
    error::~error() noexcept
    {
    }

} /* namespace sandfly */

