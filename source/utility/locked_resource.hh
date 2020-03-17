/*
 * locked_resource.hh
 *
 *  Created on: Feb 2, 2016
 *      Author: nsoblath
 */

#ifndef SANDFLY_LOCKED_RESOURCE_HH_
#define SANDFLY_LOCKED_RESOURCE_HH_

#include <atomic>
#include <memory>
#include <mutex>

namespace sandfly
{

    template< class x_resource, class x_parent, class x_mutex = std::mutex, class x_lock = std::unique_lock< x_mutex > >
    class locked_resource
    {
        public:
            typedef std::shared_ptr< x_resource > resource_ptr_t;
            typedef x_resource resource_t;

            locked_resource();
            locked_resource( locked_resource&& a_orig );
            virtual ~locked_resource();

            locked_resource& operator=( locked_resource&& a_rhs );

            resource_t* operator->() const;

            bool have_lock() const;

            void unlock();

        private:
            friend x_parent;
            locked_resource( resource_ptr_t a_resource, x_mutex& a_mutex );

            locked_resource( const locked_resource& ) = delete;
            locked_resource& operator=( const locked_resource& ) = delete;

            resource_ptr_t f_resource;
            x_lock f_lock;
            std::atomic< bool > f_have_lock;
    };

    template< class x_resource, class x_parent, class x_mutex, class x_lock >
    locked_resource< x_resource, x_parent, x_mutex, x_lock >::locked_resource()  :
            f_resource(),
            f_lock(),
            f_have_lock( false )
    {}

    template< class x_resource, class x_parent, class x_mutex, class x_lock >
    locked_resource< x_resource, x_parent, x_mutex, x_lock >::locked_resource( locked_resource&& a_orig ) :
            f_resource( std::move( a_orig.f_resource ) ),
            f_lock( std::move( a_orig.f_lock ) ),
            f_have_lock( f_lock.owns_lock() )
    {
        a_orig.f_resource.reset();
        a_orig.f_lock.release();
        a_orig.f_have_lock.store( false );
    }

    template< class x_resource, class x_parent, class x_mutex, class x_lock >
    locked_resource< x_resource, x_parent, x_mutex, x_lock >::locked_resource( resource_ptr_t a_resource, x_mutex& a_mutex ) :
        f_resource( a_resource ),
        f_lock( a_mutex ),
        f_have_lock( f_lock.owns_lock() )
    {}

    template< class x_resource, class x_parent, class x_mutex, class x_lock >
    locked_resource< x_resource, x_parent, x_mutex, x_lock >::~locked_resource()
    {}

    template< class x_resource, class x_parent, class x_mutex, class x_lock >
    locked_resource< x_resource, x_parent, x_mutex, x_lock >& locked_resource< x_resource, x_parent, x_mutex, x_lock >::operator=( locked_resource&& a_rhs )
    {
        f_resource = std::move( a_rhs.f_resource );
        a_rhs.f_resource.reset();
        f_lock = std::move( a_rhs.f_lock );
        a_rhs.f_lock.release();
        f_have_lock.store( a_rhs.f_have_lock.load() );
        a_rhs.f_have_lock.store( false );
        return *this;
    }

    template< class x_resource, class x_parent, class x_mutex, class x_lock >
    typename locked_resource< x_resource, x_parent, x_mutex, x_lock >::resource_t* locked_resource< x_resource, x_parent, x_mutex, x_lock >::operator->() const
    {
        return f_resource.get();
    }

    template< class x_resource, class x_parent, class x_mutex, class x_lock >
    bool locked_resource< x_resource, x_parent, x_mutex, x_lock >::have_lock() const
    {
        return f_have_lock.load();
    }

    template< class x_resource, class x_parent, class x_mutex, class x_lock >
    void locked_resource< x_resource, x_parent, x_mutex, x_lock >::unlock()
    {
        f_resource.reset();
        f_lock.unlock();
        f_have_lock.store( false );
        return;
    }


} /* namespace sandfly */


#endif /* SANDFLY_LOCKED_RESOURCE_HH_ */
