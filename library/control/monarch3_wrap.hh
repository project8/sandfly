/*
 * monarch3_wrap.hh
 *
 *  Created on: Feb 11, 2016
 *      Author: nsoblath
 *
 *  Monarch stages:
 *    - initialized
 *    - preparing
 *    - writing
 *    - finished
 *
 *  Thread safety
 *    Thread-safe operations:
 *      - Initializing an egg file
 *      - Accessing the Monarch header (can only be done by one thread at a time)
 *      - Writing data to a file (handled by HDF5's internal thread safety)
 *      - Finishing an egg file
 *
 *    Non-thread-safe operations:
 *      - Stream function calls are not thread-safe other than HDF5's internal thread safety.
 *        It is highly (highly highly) recommended that you only access a given stream from one thread.
 */

#ifndef SANDFLY_MONARCH3_WRAP_HH_
#define SANDFLY_MONARCH3_WRAP_HH_

#include "M3Monarch.hh"

#include "cancelable.hh"

#include <future>
#include <map>
#include <memory>
#include <mutex>

namespace sandfly
{

    enum class monarch_stage
    {
        initialized = 0,
        preparing = 1,
        writing = 2,
        finished = 3
    };
    uint32_t to_uint( monarch_stage a_stage );
    monarch_stage to_stage( uint32_t a_stage_uint );
    std::ostream& operator<<( std::ostream& a_os, monarch_stage a_stage );

    class monarch_wrapper;
    typedef std::shared_ptr< monarch_wrapper > monarch_wrap_ptr;

    class header_wrapper;
    typedef std::shared_ptr< header_wrapper > header_wrap_ptr;

    class stream_wrapper;
    typedef std::shared_ptr< stream_wrapper > stream_wrap_ptr;

    typedef std::chrono::time_point< std::chrono::steady_clock, std::chrono::nanoseconds > monarch_time_point_t;

    typedef std::unique_lock< std::mutex > unique_lock;


    //***************************
    // monarch_on_deck_manager
    //***************************

    /*!
     @class monarch_on_deck_manager
     @author N. S. Oblath

     @brief Handles asynchronous creation of on-deck monarch files and finishing of completed files.

     @details

     This class allows runs to be spread across multiple files with minimal time needed to switch between files.

     The on-deck file is created asynchronously so that, ideally, there's always a new file ready when the currently filling file runs out of space.
     Switching between files is then as simple as switching file and stream pointers (modululo a bunch of careful thread synchronization).

     Similarly, to help speed things along, file completion is also handled asynchronously by the same manager object.

     The "on-deck" monarch object is the new file waiting to be used.
     The "to-finish" monarch object is a recently finished file waiting to be closed.
    */
    class monarch_on_deck_manager : public scarab::cancelable
    {
        public:
            monarch_on_deck_manager( monarch_wrapper* a_monarch_wrap );
            ~monarch_on_deck_manager();

            const monarch3::Monarch3* od_ptr() const {return f_monarch_on_deck.get();}
            const monarch3::Monarch3* tf_ptr() const {return f_monarch_to_finish.get();}

            /// Return true if both f_monarch_on_deck and f_monarch_to_finish are empty
            bool pointers_empty() const;
            /// Return true if f_monarch_on_deck exists
            bool mod_exists() const;
            /// Return true if f_monarch_to_finish exists
            bool mtf_exists() const;

            /// Execute the thread loop: handle the asynchronous processing of the on-deck and to-finish monarch objects
            void execute();

            /// Create the on-deck monarch object if it doesn't exist already (synchronous)
            void create_on_deck();
            /// Clear the on-deck monarch object (synchronous)
            void clear_on_deck();
            /// Finish the to-finish monarch object (synchronous)
            void finish_to_finish();

            /// Notify the manager to process its monarch objects if needed (asynchronous)
            void notify();

            /// Give a monarch object to the on-deck manager with the intent that it be finished asynchronously
            void set_as_to_finish( std::shared_ptr< monarch3::Monarch3 >& a_monarch );
            /// Get the on-deck monarch object that has been created asynchronously
            void get_on_deck( std::shared_ptr< monarch3::Monarch3 >& a_monarch );

        private:
            void create_on_deck_nolock();
            void finish_to_finish_nolock();

            const monarch_wrapper* f_monarch_wrap;

            std::shared_ptr< monarch3::Monarch3 > f_monarch_on_deck;
            std::shared_ptr< monarch3::Monarch3 > f_monarch_to_finish;
            std::condition_variable f_od_condition;
            std::condition_variable f_od_continue_condition;
            std::mutex f_od_mutex;

    };


    //*******************
    // monarch_wrapper
    //*******************

    /*!
     @class monarch_wrapper
     @author N. S. Oblath

     @brief Wrapper class for a monarch3::M3Monarch object

     @details

     Provides the thread-safe, synchronized access to the Monarch object.  All thread safety is handled by the interface functions.

     Also owns a monarch_on_deck_manager object to handle asynchronous creation of on-deck files and finishing of completed files.
    */
    class monarch_wrapper : public scarab::cancelable
    {
        public:
            monarch_wrapper( const std::string& a_filename );
            ~monarch_wrapper();

            /// As it says, return the current value to, and then increment, the file count
            unsigned get_and_increment_file_count() const;

            /// Returns the header wrapped in a header_wrap_ptr to be filled at the beginning of file writing.
            header_wrap_ptr get_header();
            /// Returns the header wrapped in a header_wrap_ptr for read-only use anytime
            const header_wrap_ptr get_header() const;

            /// Returns the requested stream wrapped in a stream_wrap_ptr to be used to write records to the file.
            /// If the header had not been written, this writes the header to the file.
            stream_wrap_ptr get_stream( unsigned a_stream_no );

            /// Make the wrapper available for use; starts parallel on-deck thread
            void start_using();

            void execute_switch_loop();

            void trigger_switch();

            /// Ensure that the file is available to write to (via a stream)
            bool okay_to_write();

            /// Switch to a new file that continues the first file.
            /// The filename is automatically determine from the original filename by appending an integer count of the number of continuation files.
            /// If file contributions are being recorded, this is done automatically when the maximum file size is exceeded.
            void switch_to_new_file();

            /// Make the wrapper unavailable for use; stops the parallel on-deck thread
            void stop_using();

            /// Finish the given stream.  The stream object will be deleted.
            void finish_stream( unsigned a_stream_no );

            /// Finish the file
            void finish_file();

            monarch_time_point_t get_run_start_time() const;

            /// Override the stage value
            void set_stage( monarch_stage a_stage );

            /// Set the maximum file size used to determine when a new file is automatically started.
            void set_max_file_size( double a_size );

            /// If keeping track of file sizes for automatically creating new files, use this to inform the monarch_wrapper that a given number of bytes was written to the file.
            /// This should be called every time a record is written to the file.
            void record_file_contribution( double a_size );

        private:
            friend class monarch_on_deck_manager;

            void do_cancellation( int a_code );

            monarch_wrapper( const monarch_wrapper& ) = delete;
            monarch_wrapper& operator=( const monarch_wrapper& ) = delete;

            std::string f_orig_filename;
            std::string f_filename_base;
            std::string f_filename_ext;
            mutable unsigned f_file_count;

            double f_max_file_size_mb;
            std::atomic< double > f_file_size_est_mb;
            std::condition_variable f_wait_to_write;
            std::thread* f_switch_thread;
            std::atomic< bool > f_ok_to_write;
            std::atomic< bool > f_do_switch_flag;
            std::condition_variable f_do_switch_trig;

            std::shared_ptr< monarch3::Monarch3 > f_monarch;
            mutable std::mutex f_monarch_mutex;

            header_wrap_ptr f_header_wrap;

            std::map< unsigned, stream_wrap_ptr > f_stream_wraps;

            monarch_time_point_t f_run_start_time;

            monarch_stage f_stage;

            std::thread* f_od_thread;
            monarch_on_deck_manager f_monarch_od_manager;

    };


    //******************
    // header_wrapper
    //******************

    /*!
     @class header_wrapper
     @author N. S. Oblath

     @brief Wrapper class for a monarch3::M3Header object

     @details

     Provides the ability to write header information in a thread-safe synchronized way, and have read-only access to the header.

     Thread synchronization strategy:
       - Owns a mutex to control use of the header.
       - Provides a get_lock() function that returns a new locked unique_lock object associated with the header mutex.
       - While slower than the strategy used for by stream_wrapper, it is safer, and speed is not as critical for the header.
    */
    class header_wrapper
    {
        public:
            header_wrapper( monarch3::Monarch3& a_monarch );
            header_wrapper( header_wrapper&& a_orig );
            ~header_wrapper();

            header_wrapper& operator=( header_wrapper&& a_orig );

            /// Get a reference to the M3Header; Will throw sandfly::error if the header object is not valid.
            monarch3::M3Header& header();

            /// Get M3Header pointer
            monarch3::M3Header* ptr();
            /// Get the M3Header pointer, const version
            const monarch3::M3Header* ptr() const;

            /// Lock the header mutex and return a unique_lock object
            unique_lock get_lock() const;

        private:
            header_wrapper( const header_wrapper& ) = delete;
            header_wrapper& operator=( const header_wrapper& ) = delete;

            friend class monarch_wrapper;

            monarch3::M3Header* f_header;
            mutable std::mutex f_mutex;
    };


    //******************
    // stream_wrapper
    //******************

    /*!
     @class stream_wrapper
     @author N. S. Oblath

     @brief Wrapper class for a monarch3::M3Stream object

     @details

     Provides the ability to write records in a thread-safe synchronized way.

     Thread synchronization strategy:
       - Owns a mutex to control use of the stream.
       - Provides lock() and unlock() functions to manually lock and unlock the mutex.
       - Provides access to a reference to the mutex to allow its use in a lock.
       - Use of lock() and unlock() should be used for more speed-critical applications, with the understanding that the risks for unintentionally leaving the mutex locked is greater.
    */
    class stream_wrapper
    {
        public:
            stream_wrapper( monarch3::Monarch3&, unsigned a_stream_no, monarch_wrapper* a_monarch_wrapper );
            stream_wrapper( stream_wrapper&& a_orig );
            ~stream_wrapper();

            stream_wrapper& operator=( stream_wrapper&& a_orig );

            bool is_valid() const;

            /// Get the pointer to the stream record
            monarch3::M3Record* get_stream_record();
            /// Get the pointer to a particular channel record
            monarch3::M3Record* get_channel_record( unsigned a_chan_no );

            /// Write the record contents to the file
            bool write_record( monarch3::RecordIdType a_rec_id, monarch3::TimeType a_rec_time, const void* a_rec_block, uint64_t a_bytes, bool a_is_new_acq );

        private:
            stream_wrapper( const stream_wrapper& ) = delete;
            stream_wrapper& operator=( const stream_wrapper& ) = delete;

            friend class monarch_wrapper;

            monarch_wrapper* f_monarch_wrapper;

            monarch3::M3Stream* f_stream;
            bool f_is_valid;

            double f_record_size_mb;
    };


    //***************************
    // monarch_on_deck_manager
    //***************************

    inline bool monarch_on_deck_manager::pointers_empty() const
    {
        return ! f_monarch_on_deck && ! f_monarch_to_finish;
    }

    inline bool monarch_on_deck_manager::mod_exists() const
    {
        return f_monarch_on_deck.operator bool();
    }

    inline bool monarch_on_deck_manager::mtf_exists() const
    {
        return f_monarch_to_finish.operator bool();
    }

    inline void monarch_on_deck_manager::notify()
    {
        f_od_condition.notify_one();
        return;
    }

    inline void monarch_on_deck_manager::set_as_to_finish( std::shared_ptr< monarch3::Monarch3 >& a_monarch )
    {
        f_od_mutex.lock();
        f_monarch_to_finish.swap( a_monarch );
        f_od_mutex.unlock();
        return;
    }
    inline void monarch_on_deck_manager::get_on_deck( std::shared_ptr< monarch3::Monarch3 >& a_monarch )
    {
        f_od_mutex.lock();
        a_monarch.swap( f_monarch_on_deck );
        f_od_mutex.unlock();
        return;
    }

    inline void monarch_on_deck_manager::create_on_deck()
    {
        f_od_mutex.lock();
        if( ! f_monarch_on_deck ) create_on_deck_nolock();
        f_od_mutex.unlock();
        return;
    }

    inline void monarch_on_deck_manager::finish_to_finish_nolock()
    {
        f_monarch_to_finish->FinishWriting();
        f_monarch_to_finish.reset();
    }


    //*******************
    // monarch_wrapper
    //*******************

    inline const header_wrap_ptr monarch_wrapper::get_header() const
    {
        return f_header_wrap;
    }

    inline monarch_time_point_t monarch_wrapper::get_run_start_time() const
    {
        return f_run_start_time;
    }

    inline unsigned monarch_wrapper::get_and_increment_file_count() const
    {
        return f_file_count++;
    }

    inline void monarch_wrapper::set_max_file_size( double a_size )
    {
        f_max_file_size_mb = a_size;
        return;
    }

    inline void monarch_wrapper::do_cancellation( int a_code )
    {
        f_monarch_od_manager.cancel( a_code );
        return;
    }


    //******************
    // header_wrapper
    //******************

    inline monarch3::M3Header* header_wrapper::ptr()
    {
        return f_header;
    }

    inline const monarch3::M3Header* header_wrapper::ptr() const
    {
        return f_header;
    }

    inline unique_lock header_wrapper::get_lock() const
    {
        return unique_lock( f_mutex );
    }


    //******************
    // stream_wrapper
    //******************

    inline bool stream_wrapper::is_valid() const
    {
        return f_is_valid;
    }

    inline monarch3::M3Record* stream_wrapper::get_stream_record()
    {
        return f_stream->GetStreamRecord();
    }

    inline monarch3::M3Record* stream_wrapper::get_channel_record( unsigned a_chan_no )
    {
        return f_stream->GetChannelRecord( a_chan_no );
    }

} /* namespace sandfly */

#endif /* SANDFLY_MONARCH3_WRAP_HH_ */
