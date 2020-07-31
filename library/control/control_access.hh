/*
 * control_access.hh
 *
 *  Created on: Feb 23, 2016
 *      Author: nsoblath
 */

#ifndef SANDFLY_CONTROL_ACCESS_HH_
#define SANDFLY_CONTROL_ACCESS_HH_

#include <memory>

namespace sandfly
{
    class daq_control;

    /*!
     @class control_access
     @author N. S. Oblath

     @brief Gives other classes access to daq_control.

     @details
     Used for example by butterfly_house to get the run name and description from daq_control.
     */
    class control_access
    {
        public:
            typedef std::shared_ptr< daq_control > dc_ptr_t;

        public:
            control_access();
            virtual ~control_access();

            static void set_daq_control( std::weak_ptr< daq_control > a_daq_control );

        protected:
            static std::weak_ptr< daq_control > f_daq_control;

            dc_ptr_t use_daq_control() {return control_access::f_daq_control.lock();}

            bool daq_control_expired() {return control_access::f_daq_control.expired();}
    };

} /* namespace sandfly */

#endif /* SANDFLY_CONTROL_ACCESS_HH_ */
