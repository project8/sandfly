/*
 * server_config.hh
 *
 *  Created on: Nov 4, 2013
 *      Author: N.S. Oblath
 */

#ifndef SANDFLY_SERVER_CONFIG_HH_
#define SANDFLY_SERVER_CONFIG_HH_

#include "param.hh"

namespace scarab
{
    class main_app;
}

namespace sandfly
{
    /*!
     @class server_config
     @author N. S. Oblath

     @brief Contains default server configuration

     @details
     Contains default configurations for:
     - broker
     - queue
     - auth-file
     - slack-queue
     - activate-at-startup
     - n-files
     - duration
     - max-file-size-mb

     These default configurations, together with the configurations from the command line and the config-file, are passed to scarab::configurator by the sandfly executable.
     The configurator combines them and extracts the final sandfly configuration which is then passed to the run_server during initialization.
     */
    class server_config : public scarab::param_node
    {
        public:
            server_config();
            virtual ~server_config();
    };

    /// Add basic AMQP options to an app object
    void add_sandfly_options( scarab::main_app& an_app );

} /* namespace sandfly */

#endif /* SANDFLY_SERVER_CONFIG_HH_ */
