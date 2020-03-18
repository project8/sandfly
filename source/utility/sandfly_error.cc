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


    fatal_error::fatal_error() :
            base_exception()
    {
    }
    fatal_error::fatal_error( const fatal_error& p_copy ) :
            base_exception( p_copy )
    {
    }
    fatal_error& fatal_error::operator=( const fatal_error& p_copy )
    {
        base_exception::operator=( p_copy );
        return *this;
    }
    fatal_error::~fatal_error() noexcept
    {
    }

} /* namespace sandfly */

