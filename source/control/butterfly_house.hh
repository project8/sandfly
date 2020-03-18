/*
 * butterfly_house.hh
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
 *      - All butterfly_house function calls
 *      - Initializing an egg file
 *      - Accessing the Monarch header (can only be done by one thread at a time)
 *      - Writing data to a file (handled by HDF5's internal thread safety)
 *      - Finishing an egg file
 *
 *    Non-thread-safe operations:
 *      - Stream function calls are not thread-safe other than HDF5's internal thread safety.
 *        It is highly (highly highly) recommended that you only access a given stream from one thread.
 */

#ifndef SANDFLY_BUTTERFLY_HOUSE_HH_
#define SANDFLY_BUTTERFLY_HOUSE_HH_

#include "control_access.hh"
#include "monarch3_wrap.hh"

#include "member_variables.hh"
#include "singleton.hh"

namespace scarab
{
    class param_node;
}

namespace sandfly
{
    class egg_writer;

    /*!
     @class butterfly_house
     @author N. S. Oblath

     @brief Responsible for starting files. Holds pointer to monarch.

     @details
     Holds one monarch pointer per file.
     Registers the writer and creates, prepares, starts and finishes egg files via monarch3_wrapper.
     butterfly_house gets the file size from the config file and the filename, run duration and description from daq_control.
     It adds this information to the file header.
     */
    class butterfly_house : public scarab::singleton< butterfly_house >, public control_access
    {
        public:
            mv_accessible( double, max_file_size_mb );

        public:
            void register_file( unsigned a_file_num, const std::string& a_filename, const std::string& a_description, unsigned a_duration_ms );

            void prepare_files( const scarab::param_node& a_files_config );

            void start_files();

            void finish_files();

            void register_writer( egg_writer* a_writer, unsigned a_file_num );

            void unregister_writer( egg_writer* a_writer );

            void set_filename( const std::string& a_filename, unsigned a_file_num = 0 );
            const std::string& get_filename( unsigned a_file_num );

            void set_description( const std::string& a_desc, unsigned a_file_num = 0 );
            const std::string& get_description( unsigned a_file_num );

        private:
            struct file_info
            {
                std::string f_filename;
                std::string f_description;
            };
            typedef std::vector< file_info > file_infos_t;
            typedef file_infos_t::const_iterator file_infos_cit;
            typedef file_infos_t::iterator file_infos_it;
            file_infos_t f_file_infos;

            std::vector< monarch_wrap_ptr > f_mw_ptrs;
            std::multimap< egg_writer*, unsigned > f_writers;

            mutable std::mutex f_house_mutex;

        private:
            friend class scarab::singleton< butterfly_house >;
            friend class scarab::destroyer< butterfly_house >;

            butterfly_house();
            virtual ~butterfly_house();

    };

} /* namespace sandfly */

#endif /* SANDFLY_BUTTERFLY_HOUSE_HH_ */
