/*
 * node_builder.cc
 *
 *  Created on: Feb 18, 2016
 *      Author: nsoblath
 */

#include "node_builder.hh"

namespace sandfly
{
    //****************
    // node_binding
    //****************

    node_binding::node_binding()
    {}

    node_binding::~node_binding()
    {}

    node_binding& node_binding::operator=( const node_binding& )
    {
        return *this;
    }


    //****************
    // node_builder
    //****************

    node_builder::node_builder( node_binding* a_binding ) :
            node_binding(),
            f_binding( a_binding ),
            f_config(),
            f_name()
    {
    }

    node_builder::~node_builder()
    {
        delete f_binding;
    }

    node_builder& node_builder::operator=( const node_builder& a_rhs )
    {
        delete f_binding;
        f_binding = a_rhs.f_binding->clone();
        f_config = a_rhs.f_config;
        f_name = a_rhs.f_name;
        this->node_binding::operator=( a_rhs );
        return *this;
    }



/*

    node_builder::node_builder() :
            f_name(),
            f_config()
    {
    }

    node_builder::node_builder( const node_builder& a_orig ) :
            f_name( a_orig.f_name ),
            f_config( a_orig.f_config )
    {
    }

    node_builder::~node_builder()
    {
    }

    node_builder& node_builder::operator=( const node_builder& a_rhs )
    {
        f_name = a_rhs.f_name;
        f_config = a_rhs.f_config;
        return *this;
    }

    void node_builder::configure( const scarab::param_node& a_config )
    {
        f_config.merge( a_config );
        return;
    }

    void node_builder::replace_config( const scarab::param_node& a_config )
    {
        f_config.clear();
        f_config.merge( a_config );
        return;
    }

    void node_builder::dump_config( scarab::param_node& a_config )
    {
        a_config.clear();
        a_config.merge( f_config );
        return;
    }
    */

} /* namespace sandfly */
