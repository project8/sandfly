/*
 * server_config.cc
 *
 *  Created on: Nov 4, 2013
 *      Author: nsoblath
 */

#include "server_config.hh"
#include "logger.hh"
#include "sandfly_error.hh"

//scarab
#include "path.hh"

#include<string>

using std::string;

using scarab::param_array;
using scarab::param_node;
using scarab::param_value;

namespace sandfly
{

    LOGGER( plog, "server_config" );

    server_config::server_config()
    {
        // default server configuration

        param_node t_amqp_node = param_node();
        t_amqp_node.add( "broker", "localhost" );
        t_amqp_node.add( "queue", "sandfly" );
        t_amqp_node.add( "slack-queue", "slack_interface" );
        //add logic for default auth file if it exists
        scarab::path t_auth_default_path = scarab::expand_path( "~/.project8_authentications.json" );
        if ( boost::filesystem::exists( t_auth_default_path ) )
        {
            LDEBUG( plog, "default auth file found, setting that as initial value" << t_auth_default_path.string() );
            t_amqp_node.add( "auth-file", t_auth_default_path.string() );
        }
        else
        {
            LDEBUG( plog, "default auth file <" << t_auth_default_path.string() << "> does not exist, not setting" );
        }

        // other available values
        // - auth-file
        // - requests-exchange
        // - alerts-exchange
        // - listen-timeout-ms
        // - broker-port
        add( "amqp", t_amqp_node );

        add( "post-to-slack", false );

        param_node t_daq_node;
        t_daq_node.add( "activate-at-startup", false );
        t_daq_node.add( "n-files", 1U );
        t_daq_node.add( "duration", 1000U );
        t_daq_node.add( "max-file-size-mb", 500.0 );
        add( "daq", t_daq_node );

        param_node t_batch_commands;
        param_array t_stop_array;
        param_node t_stop_action;
        t_stop_action.add( "type", "cmd" );
        t_stop_action.add( "rks", "stop-run" );
        t_stop_action.add( "payload", param_node() );
        t_stop_array.push_back( t_stop_action );
        t_batch_commands.add( "hard-abort", t_stop_array );
        add( "batch-commands",  t_batch_commands );

        param_node t_set_conditions;
        t_set_conditions.add( "10", "hard-abort" );
        t_set_conditions.add( "12", "hard-abort" );
        add( "set-conditions", t_set_conditions );

        /*
        // this devices node can be used for multiple streams
        param_node* t_dev_node = new param_node();
        t_dev_node->add( "n-channels", new param_value( 1U ) );
        t_dev_node->add( "bit-depth", new param_value( 8U ) );
        t_dev_node->add( "data-type-size", new param_value( 1U ) );
        t_dev_node->add( "sample-size", new param_value( 2U ) );
        t_dev_node->add( "record-size", new param_value( 4096U ) );
        t_dev_node->add( "acq-rate", new param_value( 100U ) );
        t_dev_node->add( "v-offset", new param_value( 0.0 ) );
        t_dev_node->add( "v-range", new param_value( 0.5 ) );

        param_node* t_streams_node = new param_node();

        param_node* t_stream0_node = new param_node();
        t_stream0_node->add( "preset", new param_value( "str-1ch") );
        t_stream0_node->add( "device", t_dev_node );

        t_streams_node->add( "stream0", t_stream0_node );

        add( "streams", t_streams_node );
        */
    }

    server_config::~server_config()
    {
    }

} /* namespace sandfly */
