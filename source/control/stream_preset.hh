/*
 * stream_preset.hh
 *
 *  Created on: Jan 27, 2016
 *      Author: nsoblath
 */

#ifndef SANDFLY_STREAM_PRESET_HH_
#define SANDFLY_STREAM_PRESET_HH_

#include "factory.hh"

#include <map>
#include <memory>
#include <set>
#include <string>

namespace scarab
{
    class param_node;
}

namespace sandfly
{
    class node_manager;

    class stream_preset
    {
        public:
            typedef std::map< std::string, std::string > nodes_t;
            typedef std::set< std::string > connections_t;

        public:
            stream_preset();
            stream_preset( const std::string& a_type );
            stream_preset( const stream_preset& a_orig );
            virtual ~stream_preset();

            stream_preset& operator=( const stream_preset& a_rhs );

            const nodes_t& get_nodes() const;
            const connections_t& get_connections() const;

        protected:
            void node( const std::string& a_type, const std::string& a_name );
            void connection( const std::string& a_conn );

        protected:
            std::string f_type;
            nodes_t f_nodes;
            connections_t f_connections;
    };


    class runtime_stream_preset : public stream_preset
    {
        public:
            runtime_stream_preset();
            runtime_stream_preset( const std::string& a_type );
            runtime_stream_preset( const runtime_stream_preset& a_orig );
            virtual ~runtime_stream_preset();

            runtime_stream_preset& operator=( const runtime_stream_preset& a_rhs );

        public:
            static bool add_preset( const scarab::param_node& a_preset_node );

        protected:
            struct rsp_creator
            {
                std::shared_ptr< runtime_stream_preset > f_preset_ptr;
                typedef scarab::registrar< stream_preset, runtime_stream_preset, const std::string& > registrar_t;
                std::shared_ptr< registrar_t > f_registrar_ptr;
                rsp_creator() : f_preset_ptr( new runtime_stream_preset() ), f_registrar_ptr( new registrar_t( "" ) ) {}
                rsp_creator( const std::string& a_type ) : f_preset_ptr( new runtime_stream_preset( a_type ) ), f_registrar_ptr( new registrar_t( a_type ) ) {}
            };
            typedef std::map< std::string, rsp_creator> runtime_presets;
            static runtime_presets s_runtime_presets;
            static std::mutex s_runtime_presets_mutex;

    };




    inline const stream_preset::nodes_t& stream_preset::get_nodes() const
    {
        return f_nodes;
    }

    inline const stream_preset::connections_t& stream_preset::get_connections() const
    {
        return f_connections;
    }


} /* namespace sandfly */



#define DECLARE_PRESET( preset_class ) \
	class preset_class : public stream_preset \
    { \
        public: \
    		preset_class( const std::string& a_type ); \
            virtual ~preset_class() {}; \
    };

#define REGISTER_PRESET( preset_class, preset_type ) \
        static ::scarab::registrar< ::psyllid::stream_preset, preset_class, const std::string& > s_stream_preset_##preset_class##_registrar( preset_type );


#endif /* CONTROL_STREAM_PRESET_HH_ */
