/*
 * sandfly.cc
 *
 *  Created on: Feb 1, 2016
 *      Author: N.S. Oblath
 */

#include "conductor.hh"
#include "sandfly_error.hh"
#include "sandfly_version.hh"
#include "server_config.hh"

#include "application.hh"
#include "logger.hh"

using namespace sandfly;

using std::string;

LOGGER( slog, "sandfly" );

int main( int argc, char** argv )
{
    LINFO( slog, "Welcome to Sandfly\n\n" <<

            "\t\t███████╗ █████╗ ███╗   ██╗██████╗ ███████╗██╗  ██╗   ██╗\n" <<
            "\t\t██╔════╝██╔══██╗████╗  ██║██╔══██╗██╔════╝██║  ╚██╗ ██╔╝\n" <<
            "\t\t███████╗███████║██╔██╗ ██║██║  ██║█████╗  ██║   ╚████╔╝ \n" <<
            "\t\t╚════██║██╔══██║██║╚██╗██║██║  ██║██╔══╝  ██║    ╚██╔╝  \n" <<
            "\t\t███████║██║  ██║██║ ╚████║██████╔╝██║     ███████╗██║   \n" <<
            "\t\t╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═════╝ ╚═╝     ╚══════╝╚═╝   \n\n");
                                                        
    try
    {
        // The application
        scarab::main_app the_main;
        conductor the_conductor;

        // Default configuration
        the_main.default_config() = server_config();

        // The main execution callback
        the_main.callback( [&](){ the_conductor.execute( the_main.primary_config() ); } );

        // Command line options
        add_sandfly_options( the_main );

        // Package version
        the_main.set_version( std::make_shared< sandfly::version >() );

        // Parse CL options and run the application
        CLI11_PARSE( the_main, argc, argv );

        return the_conductor.get_return();
    }
    catch( scarab::error& e )
    {
        LERROR( slog, "configuration error: " << e.what() );
        return RETURN_ERROR;
    }
    catch( sandfly::error& e )
    {
        LERROR( slog, "psyllid error: " << e.what() );
        return RETURN_ERROR;
    }
    catch( std::exception& e )
    {
        LERROR( slog, "std::exception caught: " << e.what() );
        return RETURN_ERROR;
    }
    catch( ... )
    {
        LERROR( slog, "unknown exception caught" );
        return RETURN_ERROR;
    }

    return RETURN_ERROR;
}
