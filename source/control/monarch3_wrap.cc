/*
 * monarch3_wrap.cc
 *
 *  Created on: Feb 11, 2016
 *      Author: nsoblath
 */

#include "butterfly_house.hh"
#include "monarch3_wrap.hh"

#include "sandfly_constants.hh"
#include "sandfly_error.hh"

#include "M3Exception.hh"

#include "logger.hh"
#include "signal_handler.hh"

#include <boost/filesystem.hpp>

#include <future>
#include <signal.h>


namespace sandfly
{
    LOGGER( plog, "monarch3_wrap" );


    uint32_t to_uint( monarch_stage a_stage )
    {
        return static_cast< uint32_t >( a_stage );
    }
    monarch_stage to_op_t( uint32_t a_stage_uint )
    {
        return static_cast< monarch_stage >( a_stage_uint );
    }
    std::ostream& operator<<( std::ostream& a_os, monarch_stage a_stage )
    {
        return a_os << to_uint( a_stage );
    }


    //***************************
    // monarch_on_deck_manager
    //***************************

    monarch_on_deck_manager::monarch_on_deck_manager( monarch_wrapper* a_monarch_wrap ) :
            scarab::cancelable(),
            f_monarch_wrap( a_monarch_wrap ),
            f_monarch_on_deck(),
            f_od_condition(),
            f_od_continue_condition(),
            f_od_mutex()
    {}

    monarch_on_deck_manager::~monarch_on_deck_manager()
    {}

    void monarch_on_deck_manager::execute()
    {
        LINFO( plog, "Monarch-on-deck manager for file <" << f_monarch_wrap->get_header()->header().Filename() << "> is starting up" );

        while( ! is_canceled() && f_monarch_wrap->f_stage != monarch_stage::finished )
        {
            {
                unique_lock t_od_lock( f_od_mutex );

                // wait on the condition variable
                f_od_condition.wait_for( t_od_lock, std::chrono::milliseconds( 500 ) );

                // this particular setup of the while on t_do_wait and the if-elseif-else structure
                // is used so that any of the actions can be taken in any order without going through waiting on the condition.
                bool t_do_wait = false;
                try
                {
                    while( ! t_do_wait )
                    {
                        if( f_monarch_to_finish )
                        {
                            // then we need to finish a file
                            // finish the old file
                            LDEBUG( plog, "Finishing pre-existing to-finish file" );
                            finish_to_finish_nolock();
                        }
                        else if( ! f_monarch_on_deck )
                        {
                            // then we need to make a new on-deck monarch
                            unique_lock t_header_lock( f_monarch_wrap->get_header()->get_lock() );
                            create_on_deck_nolock();
                        }
                        else
                        {
                            t_do_wait = true;
                        }
                    } // end while( ! to_do_wait )
                }
                catch( std::exception& e )
                {
                    LERROR( plog, "Exception caught in monarch-on-deck manager: " << e.what() );
                    scarab::signal_handler::cancel_all( RETURN_ERROR );
                }
            }
        } // end while( ! is_canceled() && f_monarch_wrap->f_stage != monarch_stage::finished )

        LINFO( plog, "Monarch-on-deck manager is stopping" );

        return;
    }

    void monarch_on_deck_manager::create_on_deck_nolock()
    {
        try
        {
            LDEBUG( plog, "Creating a new on-deck monarch" );

            // create the new filename
            std::stringstream t_count_stream;
            t_count_stream << f_monarch_wrap->get_and_increment_file_count();
            std::string t_new_filename = f_monarch_wrap->f_filename_base + '_' + t_count_stream.str() + f_monarch_wrap->f_filename_ext;
            LDEBUG( plog, "On-deck filename: <" << t_new_filename << ">" );

            // open the new file
            std::shared_ptr< monarch3::Monarch3 > t_new_monarch;
            try
            {
                t_new_monarch.reset( monarch3::Monarch3::OpenForWriting( t_new_filename ) );
                LTRACE( plog, "New file is open" );
            }
            catch( monarch3::M3Exception& e )
            {
                throw error() << "Unable to open the file <" << t_new_filename << "\n" <<
                        "Reason: " << e.what();
            }

            // copy info into the new header
            const header_wrap_ptr t_old_header_ptr = f_monarch_wrap->get_header();
            monarch3::M3Header* t_new_header = t_new_monarch->GetHeader();
            t_new_header->CopyBasicInfo( t_old_header_ptr->header() );
            t_new_header->Filename() = t_new_filename;
            t_new_header->Description() = t_old_header_ptr->ptr()->Description() + "\nContinuation of file " + t_old_header_ptr->ptr()->Filename();

            // for each stream, create new stream in new file
            std::vector< monarch3::M3StreamHeader >* t_old_stream_headers = &t_old_header_ptr->ptr()->GetStreamHeaders();
            std::vector< unsigned > t_chan_vec;
            for( unsigned i_stream = 0; i_stream != t_old_stream_headers->size(); ++i_stream )
            {
                //t_stream_it->second->lock();
                t_chan_vec.clear();
                monarch3::M3StreamHeader* t_old_stream_header = &t_old_stream_headers->operator[]( i_stream );
                unsigned n_channels = t_old_stream_header->GetNChannels();
                if( n_channels > 1 )
                {
                    t_new_header->AddStream( t_old_stream_header->Source(), n_channels, t_old_stream_header->GetChannelFormat(),
                            t_old_stream_header->GetAcquisitionRate(), t_old_stream_header->GetRecordSize(), t_old_stream_header->GetSampleSize(),
                            t_old_stream_header->GetDataTypeSize(), t_old_stream_header->GetDataFormat(),
                            t_old_stream_header->GetBitDepth(), t_old_stream_header->GetBitAlignment(),
                            &t_chan_vec );
                }
                else
                {
                    t_new_header->AddStream( t_old_stream_header->Source(),
                            t_old_stream_header->GetAcquisitionRate(), t_old_stream_header->GetRecordSize(), t_old_stream_header->GetSampleSize(),
                            t_old_stream_header->GetDataTypeSize(), t_old_stream_header->GetDataFormat(),
                            t_old_stream_header->GetBitDepth(), t_old_stream_header->GetBitAlignment(),
                            &t_chan_vec );
                }
                std::vector< monarch3::M3ChannelHeader >& t_old_chan_headers = t_old_header_ptr->ptr()->GetChannelHeaders();
                std::vector< monarch3::M3ChannelHeader >& t_new_chan_headers = t_new_header->GetChannelHeaders();
                for( unsigned i_chan = 0; i_chan < n_channels; ++i_chan )
                {
                    t_new_chan_headers[ i_chan ].SetVoltageOffset( t_old_chan_headers[ i_chan ].GetVoltageOffset() );
                    t_new_chan_headers[ i_chan ].SetVoltageRange( t_old_chan_headers[ i_chan ].GetVoltageRange() );
                    t_new_chan_headers[ i_chan ].SetDACGain( t_old_chan_headers[ i_chan ].GetDACGain() );
                    t_new_chan_headers[ i_chan ].SetFrequencyMin( t_old_chan_headers[ i_chan ].GetFrequencyMin() );
                    t_new_chan_headers[ i_chan ].SetFrequencyRange( t_old_chan_headers[ i_chan ].GetFrequencyRange() );
                }
            }

            // write the new header
            LTRACE( plog, "Writing new header" );
            t_new_monarch->WriteHeader();

            // switch out the monarch pointers, f_monarch_on_deck <--> t_new_monarch
            f_monarch_on_deck.swap( t_new_monarch );
        }
        catch(...)
        {
            throw;
        }
        return;
    }

    void monarch_on_deck_manager::clear_on_deck()
    {
        f_od_mutex.lock();
        if( f_monarch_on_deck )
        {
            std::string t_filename( f_monarch_on_deck->GetHeader()->Filename() );
            try
            {
                LDEBUG( plog, "Closing on-deck file <" << t_filename << ">" );
                f_monarch_on_deck.reset();
            }
            catch( monarch3::M3Exception& e )
            {
                LWARN( plog, "File could not be closed properly: " << e.what() );
            }
            try
            {
                LDEBUG( plog, "On-deck file was written out to <" << t_filename << ">; now removing the file" );
                boost::filesystem::remove( t_filename );
            }
            catch( boost::filesystem::filesystem_error& e )
            {
                LWARN( plog, "File could not be removed: <" << t_filename << ">\n" << e.what() );
            }
        }
        f_od_mutex.unlock();
        return;
    }

    void monarch_on_deck_manager::finish_to_finish()
    {
        f_od_mutex.lock();

        if( f_monarch_to_finish )
        {
            LDEBUG( plog, "Finishing to-finish file" );
            finish_to_finish_nolock();
        }
        f_od_mutex.unlock();
        return;
    }


    //*******************
    // monarch_wrapper
    //*******************

    monarch_wrapper::monarch_wrapper( const std::string& a_filename ) :
            f_orig_filename( a_filename ),
            f_filename_base(),
            f_filename_ext(),
            f_file_count( 1 ),
            f_max_file_size_mb( 0. ),
            f_file_size_est_mb( 0. ),
            f_wait_to_write(),
            f_switch_thread( nullptr ),
            f_ok_to_write( true ),
            f_do_switch_flag( false ),
            f_do_switch_trig(),
            f_monarch(),
            f_monarch_mutex(),
            f_header_wrap(),
            f_stream_wraps(),
            f_run_start_time( std::chrono::steady_clock::now() ),
            f_stage( monarch_stage::initialized ),
            f_od_thread( nullptr ),
            f_monarch_od_manager( this )
    {
        std::string::size_type t_ext_pos = a_filename.find_last_of( '.' );
        if( t_ext_pos == std::string::npos )
        {
            f_filename_base = f_orig_filename;
        }
        else
        {
            f_filename_base = f_orig_filename.substr( 0, t_ext_pos );
            f_filename_ext = f_orig_filename.substr( t_ext_pos );
        }
        LDEBUG( plog, "Monarch wrapper created with filename base <" << f_filename_base << "> and extension <" << f_filename_ext << ">" );

        unique_lock t_monarch_lock( f_monarch_mutex );
        try
        {
            f_monarch.reset( monarch3::Monarch3::OpenForWriting( f_orig_filename ) );
        }
        catch( monarch3::M3Exception& e )
        {
            throw error() << "Unable to open the file <" << f_orig_filename << "\n" <<
                    "Reason: " << e.what();
        }
    }

    monarch_wrapper::~monarch_wrapper()
    {
        f_monarch_mutex.lock();

        set_stage( monarch_stage::finished );

        if( f_od_thread != nullptr )
        {
            if( f_od_thread->joinable() )
            {
                LWARN( plog, "Trying to destroy monarch_wrapper; waiting for on-deck thread to join" );
                f_od_thread->join();
            }
            delete f_od_thread;
        }

        if( f_switch_thread != nullptr )
        {
            if( f_switch_thread->joinable() )
            {
                LWARN( plog, "Trying to destroy monarch_wrapper; waiting for switch thread to join" );
                f_switch_thread->join();
            }
            delete f_switch_thread;
        }

        try
        {
            if( f_monarch )
            {
                f_monarch->FinishWriting();
                f_monarch.reset();
            }

        }
        catch( monarch3::M3Exception& e )
        {
            LERROR( plog, "Unable to write file on monarch_wrapper deletion: " << e.what() );
        }

        f_wait_to_write.notify_all();

        f_monarch_mutex.unlock();

    }

    header_wrap_ptr monarch_wrapper::get_header()
    {
        unique_lock t_monarch_lock( f_monarch_mutex );
        if( f_stage == monarch_stage::initialized )
        {
            try
            {
                set_stage( monarch_stage::preparing );
                header_wrap_ptr t_header_ptr( new header_wrapper( *f_monarch.get() ) );
                f_header_wrap = t_header_ptr;
            }
            catch( error& e )
            {
                throw;
            }
        }
        if( f_stage != monarch_stage::preparing ) throw error() << "Invalid monarch stage for getting the (writeable) header: " << f_stage << "; use the const get_header() instead";

        return f_header_wrap;
    }

    stream_wrap_ptr monarch_wrapper::get_stream( unsigned a_stream_no )
    {
        LDEBUG( plog, "Stream <" << a_stream_no << "> is being retrieved from the Monarch object" );

        unique_lock t_monarch_lock( f_monarch_mutex );

        if( f_stage != monarch_stage::writing ) throw error() << "Invalid monarch stage for getting a stream: " << f_stage;

        if( f_stream_wraps.find( a_stream_no ) == f_stream_wraps.end() )
        {
            try
            {
                stream_wrap_ptr t_stream_ptr( new stream_wrapper( *f_monarch.get(), a_stream_no, this ) );
                f_stream_wraps[ a_stream_no ] = t_stream_ptr;
            }
            catch( error& e )
            {
                throw;
            }
        }
        return f_stream_wraps[ a_stream_no ];
    }

    void monarch_wrapper::start_using()
    {
        if( f_stage != monarch_stage::preparing ) throw error() << "Invalid monarch stage for start-using: " << f_stage;

        if( ! f_monarch_od_manager.pointers_empty() )
        {
            throw error() << "One or more of the Monarch pointers is not empty";
        }

        if( f_od_thread != nullptr )
        {
            throw error() << "On-deck thread already exists";
        }

        unique_lock t_monarch_lock( f_monarch_mutex );

        unique_lock t_header_lock( f_header_wrap->get_lock() );

        LDEBUG( plog, "Writing the header for file <" << f_header_wrap->header().Filename() );
        try
        {
            f_monarch->WriteHeader();
            LDEBUG( plog, "Header written for file <" << f_header_wrap->header().Filename() << ">" );
        }
        catch( monarch3::M3Exception& e )
        {
            throw error() << e.what();
        }
        set_stage( monarch_stage::writing );

        t_header_lock.unlock();


        // prepare file-switching components

        f_do_switch_flag = false;

        LDEBUG( plog, "Starting the switch thread for file <" << f_header_wrap->header().Filename() << ">" );
        f_switch_thread = new std::thread( &monarch_wrapper::execute_switch_loop, this );

        // start the on-deck thread and assign it to the member variable for safe keeping
        LDEBUG( plog, "Starting the on-deck thread for file <" << f_header_wrap->header().Filename() << ">" );
        f_od_thread = new std::thread( &monarch_on_deck_manager::execute, &f_monarch_od_manager );

        // let the thread start up
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

        // release it to create the on-deck file
        f_monarch_od_manager.notify();

        return;
    }

    void monarch_wrapper::execute_switch_loop()
    {
        LINFO( plog, "Monarch's execute-switch-loop for file <" << f_header_wrap->header().Filename() << "> is starting up" );

        while( ! is_canceled() && f_stage != monarch_stage::finished )
        {
            unique_lock t_lock( f_monarch_mutex );

            // wait on the condition variable
            while( ! f_do_switch_flag.load() && ! is_canceled() )
            {
                f_do_switch_trig.wait_for( t_lock, std::chrono::milliseconds( 500 ) );
            }
            if( is_canceled() ) break;
            //if( f_stage == monarch_stage::finished) break;

            // f_monarch_mutex is locked at this point

            f_do_switch_flag = false;

            LDEBUG( plog, "Switching egg files" );
            try
            {
                switch_to_new_file();
            }
            catch( std::exception& e )
            {
                LERROR( plog, "Caught exception while switching to new file: " << e.what() );
                scarab::signal_handler::cancel_all( RETURN_ERROR );
            }

            f_ok_to_write = true;
            f_wait_to_write.notify_all();

        } // end while( ! f_monarch_od_manager.is_canceled() && f_monarch_wrap->f_stage != monarch_stage::finished )

        LINFO( plog, "Monarch's execute-switch-loop for file <" << f_header_wrap->header().Filename() << "> is stopping" );

        return;
    }

    void monarch_wrapper::trigger_switch()
    {
        if( f_do_switch_flag.load() ) return;
        f_do_switch_flag = true;
        f_ok_to_write = false;
        f_do_switch_trig.notify_one();
        return;
    }

    void monarch_wrapper::stop_using()
    {
        if( f_od_thread == nullptr )
        {
            LWARN( plog, "Monarch wrapper was not in use" );
            return;
        }

        f_monarch_od_manager.notify();

        f_od_thread->join();
        delete f_od_thread;
        f_od_thread = nullptr;

        f_switch_thread->join();
        delete f_switch_thread;
        f_switch_thread = nullptr;

        f_monarch_od_manager.clear_on_deck();

        return;
    }

    void monarch_wrapper::finish_stream( unsigned a_stream_no )
    {
        unique_lock t_monarch_lock( f_monarch_mutex );
        if( f_stage != monarch_stage::writing )
        {
            throw error() << "Monarch must be in the writing stage to finish a stream";
        }

        auto t_stream_it = f_stream_wraps.find( a_stream_no );
        if( t_stream_it == f_stream_wraps.end() )
        {
            throw error() << "Stream number <" << a_stream_no << "> was not found";
        }
        LDEBUG( plog, "Finishing stream <" << a_stream_no << ">" );
        f_stream_wraps.erase( t_stream_it );

        return;
    }

    void monarch_wrapper::finish_file()
    {
        unique_lock t_monarch_lock( f_monarch_mutex );

        std::string t_filename( f_monarch->GetHeader()->Filename() );

        f_monarch_od_manager.finish_to_finish();

        if( f_stage == monarch_stage::preparing )
        {
            f_header_wrap.reset();
            try
            {
                f_monarch->WriteHeader();
                LDEBUG( plog, "Header written for file <" << t_filename << ">" );
            }
            catch( monarch3::M3Exception& e )
            {
                throw error() << e.what();
            }
        }
        else if( f_stage == monarch_stage::writing )
        {
            t_monarch_lock.unlock(); // finish_stream() locks f_monarch_mutex, so it needs to be unlocked before calls on that can be processed
            // we expect that streams are being cancelled from their respective writers
            // here we wait for them to all be cancelled
            for( unsigned i_attempt = 0; i_attempt < 10 && ! f_stream_wraps.empty(); ++i_attempt)
            {
                // give midge time to finish the streams before finishing the files
                std::this_thread::sleep_for( std::chrono::milliseconds(500));
            }
            if( ! f_stream_wraps.empty() )
            {
                // perhaps we got here without midge being cancelled.
                // let's call a global cancel and then wait for the streams to cancel again
                LERROR( plog, "Streams were not cancelled as expected (via their writers). Attempting a global cancellation." );
                scarab::signal_handler::cancel_all( RETURN_ERROR );
                for( unsigned i_attempt = 0; i_attempt < 10 && ! f_stream_wraps.empty(); ++i_attempt)
                {
                    // give midge time to finish the streams before finishing the files
                    std::this_thread::sleep_for( std::chrono::milliseconds(500));
                }
                // check again that the streams are now empty
                if( ! f_stream_wraps.empty() )
                {
                    // get out if the streams did not cancel because we can't finish the file correctly in that case
                    throw error() << "Streams did not all finish after wait period and global cancellation";
                }
            }
            // re-lock so that the lock condition is the same once we exit this block
            t_monarch_lock.lock();
        }
        LINFO( plog, "Finished writing file <" << t_filename << ">" );
        set_stage( monarch_stage::finished );
        f_monarch->FinishWriting();
        f_monarch.reset();
        f_file_size_est_mb = 0.;
        return;
    }

    void monarch_wrapper::switch_to_new_file()
    {
        if( f_stage != monarch_stage::writing ) throw error() << "Invalid monarch stage for starting a new file: " << f_stage;

        try
        {
            LDEBUG( plog, "Switching to new file; locking header mutex, and monarch mutex" );
            unique_lock t_header_lock( f_header_wrap->get_lock() );
            //unique_lock t_monarch_lock( f_monarch_mutex ); // monarch mutex is already locked in the loop in execute_switch_loop

            // if the to-finish monarch is full for some reason, empty it
            LTRACE( plog, "Synchronous call to finish to-finish" );
            f_monarch_od_manager.finish_to_finish();

            // if the on-deck monarch doesn't exist, create it
            LTRACE( plog, "Synchronous call to create on-deck" );
            f_monarch_od_manager.create_on_deck();

            LTRACE( plog, "Switching file pointers" );

            // move the old file to the to_finish pointer
            f_monarch_od_manager.set_as_to_finish( f_monarch );

            // move the on_deck pointer to the current pointer
            f_monarch_od_manager.get_on_deck( f_monarch );

            f_file_size_est_mb = 0.;

            LTRACE( plog, "Switching header pointer" );

            // swap out the header pointer
            f_header_wrap->f_header = f_monarch->GetHeader();
            t_header_lock.unlock();

            LTRACE( plog, "Switching stream pointers" );

            // swap out the stream pointers
            for( std::map< unsigned, stream_wrap_ptr >::iterator t_stream_it = f_stream_wraps.begin(); t_stream_it != f_stream_wraps.end(); ++t_stream_it )
            {
                t_stream_it->second->f_stream = f_monarch->GetStream( t_stream_it->first );
                if( t_stream_it->second->f_stream == nullptr )
                {
                    throw error() << "Stream <" << t_stream_it->first << "> was invalid";
                }
            }

            LDEBUG( plog, "Switch to new file is complete: <" << f_header_wrap->ptr()->Filename() << ">" );

            //f_file_switch_started = false;

            // notify the on-deck thread to process the to-finish and on-deck monarchs
            f_monarch_od_manager.notify();

        }
        catch( std::exception& e )
        {
            throw;
        }
        return;
    }

    void monarch_wrapper::set_stage( monarch_stage a_stage )
    {
        LDEBUG( plog, "Setting monarch stage to <" << a_stage << ">" );
        f_stage = a_stage;
        return;
    }

    void monarch_wrapper::record_file_contribution( double a_size )
    {
        double t_file_size_est_mb = f_file_size_est_mb.load() + a_size;
        f_file_size_est_mb = t_file_size_est_mb;
        LTRACE( plog, "File contribution: " << a_size << " MB;  Estimated file size is now " << t_file_size_est_mb << " MB;  limit is " << f_max_file_size_mb << " MB" );
        if( t_file_size_est_mb >= f_max_file_size_mb )
        {
            LDEBUG( plog, "Max file size exceeded (" << t_file_size_est_mb << " MB >= " << f_max_file_size_mb << " MB)" );
            trigger_switch();
        }
        return;
    }

    inline bool monarch_wrapper::okay_to_write()
    {
        LTRACE( plog, "Checking ok to write" );
        if( f_ok_to_write.load() ) return f_monarch.operator bool();
        std::mutex t_wait_mutex;
        unique_lock t_wait_lock( t_wait_mutex );
        while( ! f_ok_to_write.load() )
        {
            f_wait_to_write.wait_for( t_wait_lock, std::chrono::milliseconds( 100 ) );
        }
        return f_monarch.operator bool();
    }


    //******************
    // header_wrapper
    //******************

    header_wrapper::header_wrapper( monarch3::Monarch3& a_monarch ) :
        f_header( a_monarch.GetHeader() ),
        f_mutex()
    {
        if( ! f_header )
        {
            throw error() << "Unable to get monarch header";
        }
    }

    header_wrapper::header_wrapper( header_wrapper&& a_orig ) :
            f_header( a_orig.f_header ),
            f_mutex()//,
            //f_lock( f_mutex )
    {
        a_orig.f_header = nullptr;
    }

    header_wrapper::~header_wrapper()
    {
    }

    header_wrapper& header_wrapper::operator=( header_wrapper&& a_orig )
    {
        f_header = a_orig.f_header;
        a_orig.f_header = nullptr;
        return *this;
    }

    monarch3::M3Header& header_wrapper::header()
    {
        if( f_header == nullptr ) throw error() << "Unable to write to header; the owning Monarch object must have moved beyond the preparation stage";
        return *f_header;
    }


    //******************
    // stream_wrapper
    //******************

    stream_wrapper::stream_wrapper( monarch3::Monarch3& a_monarch, unsigned a_stream_no, monarch_wrapper* a_monarch_wrapper ) :
            f_monarch_wrapper( a_monarch_wrapper ),
            f_stream( a_monarch.GetStream( a_stream_no ) ),
            f_is_valid( true ),
            f_record_size_mb( 1.e-6 * (double)f_stream->GetStreamRecordNBytes() )
    {
        if( f_stream == nullptr )
        {
            throw error() << "Invalid stream number requested: " << a_stream_no;
        }
    }

    stream_wrapper::stream_wrapper( stream_wrapper&& a_orig ) :
            f_monarch_wrapper( a_orig.f_monarch_wrapper ),
            f_stream( a_orig.f_stream ),
            f_is_valid( a_orig.f_is_valid ),
            f_record_size_mb( a_orig.f_record_size_mb )
    {
        a_orig.f_stream = nullptr;
        a_orig.f_is_valid = false;
    }

    stream_wrapper::~stream_wrapper()
    {}

    stream_wrapper& stream_wrapper::operator=( stream_wrapper&& a_orig )
    {
        f_monarch_wrapper = a_orig.f_monarch_wrapper;
        f_stream = a_orig.f_stream;
        a_orig.f_stream = nullptr;
        a_orig.f_is_valid = false;
        f_record_size_mb = a_orig.f_record_size_mb;
        return *this;
    }

    /// Write the record contents to the file
    bool stream_wrapper::write_record( monarch3::RecordIdType a_rec_id, monarch3::TimeType a_rec_time, const void* a_rec_block, uint64_t a_bytes, bool a_is_new_acq )
    {
        LTRACE( plog, "Writing record <" << a_rec_id << ">" );
        if( ! f_monarch_wrapper->okay_to_write() )
        {
            LERROR( plog, "Unable to write to monarch file" );
            //f_mutex.unlock();
            return false;
        }
        get_stream_record()->SetRecordId( a_rec_id );
        get_stream_record()->SetTime( a_rec_time );
        ::memcpy( get_stream_record()->GetData(), a_rec_block, a_bytes );
        bool t_return = f_stream->WriteRecord( a_is_new_acq );
        f_monarch_wrapper->record_file_contribution( f_record_size_mb );
        return t_return;
    }

} /* namespace sandfly */
