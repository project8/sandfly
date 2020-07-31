/*
 * butterfly_house.cc
 *
 *  Created on: Feb 11, 2016
 *      Author: nsoblath
 */

#include "butterfly_house.hh"

#include "daq_control.hh"
#include "egg_writer.hh"

#include "logger.hh"
#include "param.hh"
#include "time.hh"

#include "sandfly_error.hh"

namespace sandfly
{
    LOGGER( plog, "butterfly_house" );


    butterfly_house::butterfly_house() :
            control_access(),
            f_max_file_size_mb( 500 ),
            f_file_infos(),
            f_mw_ptrs(),
            f_writers(),
            f_house_mutex()
    {
        LDEBUG( plog, "Butterfly house has been built" );
    }

    butterfly_house::~butterfly_house()
    {
    }


    void butterfly_house::prepare_files( const scarab::param_node& a_daq_config )
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );

        // default is 1 file
        f_file_infos.clear();

        if( a_daq_config.empty() )
        {
            f_file_infos.resize( 1 );
        }
        else
        {
            f_file_infos.resize( a_daq_config.get_value( "n-files", 1U ) );
            set_max_file_size_mb( a_daq_config.get_value( "max-file-size-mb", get_max_file_size_mb() ) );
        }

        for( file_infos_it fi_it = f_file_infos.begin(); fi_it != f_file_infos.end(); ++fi_it )
        {
            // creating default filenames; these can be changed when the run is started
            std::stringstream t_filename_sstr;
            unsigned t_file_num = fi_it - f_file_infos.begin();
            t_filename_sstr << "sandfly_out_" << t_file_num << ".egg";
            fi_it->f_filename = t_filename_sstr.str();
            fi_it->f_description = "";
            LPROG( plog, "Prepared file <" << t_file_num << ">; default filename is <" << fi_it->f_filename << ">" );
        }
        return;
    }

    void butterfly_house::start_files()
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );

        if( control_access::daq_control_expired() )
        {
            LERROR( plog, "Unable to get access to the DAQ control" );
            throw error() << "Butterfly house is unable to get access to the DAQ control";
        }
        unsigned t_run_duration = use_daq_control()->get_run_duration();

        LINFO( plog, "Starting egg3 files" );
        try
        {
            f_mw_ptrs.clear();
            f_mw_ptrs.resize( f_file_infos.size() );
            for( unsigned t_file_num = 0; t_file_num < f_file_infos.size(); ++t_file_num )
            {
                std::string t_filename( f_file_infos[ t_file_num ].f_filename );
                LDEBUG( plog, "Creating file <" << t_filename << ">" );
                f_mw_ptrs[ t_file_num ] = monarch_wrap_ptr( new monarch_wrapper( t_filename ) );
                f_mw_ptrs[ t_file_num ]->set_max_file_size( f_max_file_size_mb );

                header_wrap_ptr t_hwrap_ptr = f_mw_ptrs[ t_file_num ]->get_header();
                unique_lock t_header_lock( t_hwrap_ptr->get_lock() );
                t_hwrap_ptr->header().Description() = f_file_infos[ t_file_num ].f_description;

                time_t t_raw_time = time( nullptr );
                struct tm* t_processed_time = gmtime( &t_raw_time );
                char t_timestamp[ 512 ];
                strftime( t_timestamp, 512, scarab::date_time_format, t_processed_time );
                //LWARN( plog, "raw: " << t_raw_time << "   proc'd: " << t_processed_time->tm_hour << " " << t_processed_time->tm_min << " " << t_processed_time->tm_year << "   timestamp: " << t_timestamp );
                t_hwrap_ptr->header().Timestamp() = t_timestamp;

                t_hwrap_ptr->header().SetRunDuration( t_run_duration );

                // writer/stream setup
                LDEBUG( plog, "Setting up streams" );
                for( auto it_writer = f_writers.begin(); it_writer != f_writers.end(); ++it_writer )
                {
                    if( it_writer->second == t_file_num )
                    {
                        it_writer->first->prepare_to_write( f_mw_ptrs[ t_file_num ], t_hwrap_ptr );
                    }
                }

                // be sure to unlock here; the header mutex is locked again in monarch_wrapper::start_using()
                t_header_lock.unlock();

                f_mw_ptrs[ t_file_num ]->start_using();
            }
            LINFO( plog, "Done creating egg3 files" );
        }
        catch( std::exception& e )
        {
            throw;
        }

        return;
    }

    void butterfly_house::finish_files()
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );
        try
        {
            for( auto file_it = f_mw_ptrs.begin(); file_it != f_mw_ptrs.end(); ++file_it )
            {
                (*file_it)->cancel();
                (*file_it)->stop_using();
                (*file_it)->finish_file();
                file_it->reset();
            }
            f_mw_ptrs.clear();
        }
        catch( std::exception& e )
        {
            throw;
        }

        return;
    }

    void butterfly_house::register_writer( egg_writer* a_writer, unsigned a_file_num )
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );

        if( a_file_num >= f_file_infos.size() )
        {
            throw error() << "Currently configured number of files is <" << f_file_infos.size() << ">, but file <" << a_file_num << "> was requested by a writer; please reconfigure for the appropriate number of files or assign the writer to the appropriate file number.";
        }

        bool t_has_already = false;
        auto t_range = f_writers.equal_range( a_writer );
        for( auto t_it = t_range.first; t_it != t_range.second; ++t_it )
        {
            if( t_it->second == a_file_num )
            {
                t_has_already = true;
                break;
            }
        }
        if( t_has_already )
        {
            throw error() << "Egg writer at <" << a_writer << "> is already registered for file number " << a_file_num;
        }
        f_writers.insert( std::pair< egg_writer*, unsigned >( a_writer, a_file_num ) );
        return;
    }

    void butterfly_house::unregister_writer( egg_writer* a_writer )
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );
        auto t_range = f_writers.equal_range( a_writer );
        f_writers.erase( t_range.first, t_range.second );
        return;
    }

    void butterfly_house::set_filename( const std::string& a_filename, unsigned a_file_num )
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );
        if( a_file_num > f_file_infos.size() ) throw error() << "Currently configured number of files is <" << f_file_infos.size() << ">, but filename-set was for file <" << a_file_num << ">.";
        f_file_infos[ a_file_num ].f_filename = a_filename;
        return;
    }

    const std::string& butterfly_house::get_filename( unsigned a_file_num )
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );
        if( a_file_num > f_file_infos.size() ) throw error() << "Currently configured number of files is <" << f_file_infos.size() << ">, but filename-get was for file <" << a_file_num << ">.";
        return f_file_infos[ a_file_num ].f_filename;
    }

    void butterfly_house::set_description( const std::string& a_desc, unsigned a_file_num )
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );
        if( a_file_num > f_file_infos.size() ) throw error() << "Currently configured number of files is <" << f_file_infos.size() << ">, but description-set was for file <" << a_file_num << ">.";
        f_file_infos[ a_file_num ].f_description = a_desc;
        return;
    }

    const std::string& butterfly_house::get_description( unsigned a_file_num )
    {
        std::unique_lock< std::mutex > t_lock( f_house_mutex );
        if( a_file_num > f_file_infos.size() ) throw error() << "Currently configured number of files is <" << f_file_infos.size() << ">, but description-get was for file <" << a_file_num << ">.";
        return f_file_infos[ a_file_num ].f_description;
    }

} /* namespace sandfly */
