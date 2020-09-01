/*
 * control_access.hh
 *
 *  Created on: Feb 23, 2016
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_CONTROL_ACCESS_HH_
#define SANDFLY_CONTROL_ACCESS_HH_

#include <memory>

namespace sandfly
{
    class run_control;

    /*!
     @class control_access
     @author N. S. Oblath

     @brief Gives other classes access to run_control.

     @details
     Used for example by butterfly_house to get the run name and description from run_control.
     */
    class control_access
    {
        public:
            typedef std::shared_ptr< run_control > dc_ptr_t;

        public:
            control_access();
            virtual ~control_access();

            static void set_run_control( std::weak_ptr< run_control > a_run_control );

        protected:
            static std::weak_ptr< run_control > f_run_control;

            dc_ptr_t use_run_control() {return control_access::f_run_control.lock();}

            bool run_control_expired() {return control_access::f_run_control.expired();}
    };

} /* namespace sandfly */

#endif /* SANDFLY_CONTROL_ACCESS_HH_ */
