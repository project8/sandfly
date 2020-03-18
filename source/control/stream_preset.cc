/*
 * stream_preset.cc
 *
 *  Created on: Jan 27, 2016
 *      Author: nsoblath
 */

#include "stream_preset.hh"

#include "sandfly_error.hh"

#include "logger.hh"
#include "param.hh"

namespace sandfly
{
    LOGGER( plog, "stream_preset" );

    //*****************
    // stream_preset
    //*****************

    stream_preset::stream_preset() :
            f_type( "unknown" ),
            f_nodes(),
            f_connections()
    {
    }

    stream_preset::stream_preset( const std::string& a_type ) :
            f_type( a_type ),
            f_nodes(),
            f_connections()
    {
    }

    stream_preset::stream_preset( const stream_preset& a_orig ) :
            f_type( a_orig.f_type ),
            f_nodes( a_orig.f_nodes ),
            f_connections( a_orig.f_connections )
    {
    }

    stream_preset::~stream_preset()
    {
    }

    stream_preset& stream_preset::operator=( const stream_preset& a_rhs )
    {
        f_type = a_rhs.f_type;
        f_nodes = a_rhs.f_nodes;
        f_connections = a_rhs.f_connections;
        return *this;
    }

    void stream_preset::node( const std::string& a_type, const std::string& a_name )
    {
        if( f_nodes.find( a_name ) != f_nodes.end() )
        {
            throw error() << "Invalid preset: node is already present: <" + a_name + "> of type <" + a_type + ">";
        }
        f_nodes.insert( nodes_t::value_type( a_name, a_type ) );
        return;
    }

    void stream_preset::connection( const std::string& a_conn )
    {
        if( f_connections.find( a_conn ) != f_connections.end() )
        {
            throw error() << "Invalid preset; connection is already present: <" + a_conn + ">";
        }
        f_connections.insert( a_conn );
    }


    //*************************
    // runtime_stream_preset
    //*************************

    runtime_stream_preset::runtime_presets runtime_stream_preset::s_runtime_presets;
    std::mutex runtime_stream_preset::s_runtime_presets_mutex;

    runtime_stream_preset::runtime_stream_preset() :
            stream_preset()
    {
    }

    runtime_stream_preset::runtime_stream_preset( const std::string& a_type ) :
            stream_preset( a_type )
    {
        //std::unique_lock< std::mutex > t_lock( s_runtime_presets_mutex );
        runtime_presets::const_iterator t_preset_it = s_runtime_presets.find( a_type );
        if( t_preset_it != s_runtime_presets.end() )
        {
            *this = *t_preset_it->second.f_preset_ptr.get();
            //f_nodes = s_runtime_presets.at( a_type ).f_preset.f_nodes;
            //f_connections = s_runtime_presets.at( a_type ).f_preset.f_connections;
        }
    }

    runtime_stream_preset::runtime_stream_preset( const runtime_stream_preset& a_orig ) :
            stream_preset( a_orig )
    {
    }

    runtime_stream_preset::~runtime_stream_preset()
    {
    }

    runtime_stream_preset& runtime_stream_preset::operator=( const runtime_stream_preset& a_rhs )
    {
        stream_preset::operator=( a_rhs );
        return *this;
    }

    bool runtime_stream_preset::add_preset( const scarab::param_node& a_preset_node )
    {
        if( ! a_preset_node.has( "type" ) )
        {
            LERROR( plog, "Preset must have a type.  Preset config:\n" << a_preset_node );
            return false;
        }
        std::string t_preset_type = a_preset_node["type"]().as_string();

        LDEBUG( plog, "Adding preset of type <" << t_preset_type << ">" );

        if( ! a_preset_node.has( "nodes" ) )
        {
            LERROR( plog, "No \"nodes\" configuration was present for preset <" << t_preset_type << ">" );
            return false;
        }
        const scarab::param_array& t_nodes_array = a_preset_node["nodes"].as_array();

        std::unique_lock< std::mutex > t_lock( s_runtime_presets_mutex );

        auto t_rp_pair = s_runtime_presets.insert( runtime_presets::value_type( t_preset_type, rsp_creator( t_preset_type ) ) );
        if( ! t_rp_pair.second )
        {
            LERROR( plog, "Unable to add new runtime preset <" << t_preset_type << ">" );
            return false;
        }
        runtime_presets::iterator t_new_rsp_creator = t_rp_pair.first;

        std::string t_type;
        for( scarab::param_array::const_iterator t_nodes_it = t_nodes_array.begin(); t_nodes_it != t_nodes_array.end(); ++t_nodes_it )
        {
            if( ! t_nodes_it->is_node() )
            {
                LERROR( plog, "Invalid node specification in preset <" << t_preset_type << ">" );
                scarab::factory< stream_preset, runtime_stream_preset, const std::string& >::get_instance()->remove_class( t_preset_type );
                s_runtime_presets.erase( t_new_rsp_creator );
                return false;
            }

            t_type = t_nodes_it->as_node().get_value( "type", "" );
            if( t_type.empty() )
            {
                LERROR( plog, "No type given for one of the nodes in preset <" << t_preset_type << ">" );
                scarab::factory< stream_preset, runtime_stream_preset, const std::string& >::get_instance()->remove_class( t_preset_type );
                s_runtime_presets.erase( t_new_rsp_creator );
                return false;
            }

            LDEBUG( plog, "Adding node <" << t_type << ":" << t_nodes_it->as_node().get_value( "name", t_type ) << "> to preset <" << t_preset_type << ">" );
            t_new_rsp_creator->second.f_preset_ptr->node( t_type, t_nodes_it->as_node().get_value( "name", t_type ) );
        }

        if( ! a_preset_node.has( "connections" ) )
        {
            LDEBUG( plog, "Preset <" << t_preset_type << "> is being setup with no connections" );
        }
        else
        {
            const scarab::param_array& t_conn_array = a_preset_node["connections"].as_array();
            for( scarab::param_array::const_iterator t_conn_it = t_conn_array.begin(); t_conn_it != t_conn_array.end(); ++t_conn_it )
            {
                if( ! t_conn_it->is_value() )
                {
                    LERROR( plog, "Invalid connection specification in preset <" << t_preset_type << ">" );
                    scarab::factory< stream_preset, runtime_stream_preset, const std::string& >::get_instance()->remove_class( t_preset_type );
                    s_runtime_presets.erase( t_new_rsp_creator );
                    return false;
                }

                LDEBUG( plog, "Adding connection <" << t_conn_it->as_value().as_string() << "> to preset <" << t_preset_type << ">");
                t_new_rsp_creator->second.f_preset_ptr->connection( t_conn_it->as_value().as_string() );
            }
        }

        LINFO( plog, "Preset <" << t_preset_type << "> is now available" );

        return true;
    }

} /* namespace sandfly */
