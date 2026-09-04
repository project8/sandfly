#ifndef SANDFLY_TEST_BINDING_HH_
#define SANDFLY_TEST_BINDING_HH_

#include "node_builder.hh"
#include "param.hh"

namespace sandfly
{
    namespace testing
    {
        /*!
         @brief Apply a config to a node via its binding, dump it back, and return the result.

         @details
         Exercises both do_apply_config and do_dump_config in a single call. The round-trip
         catches the common bug of adding a key to apply_config but forgetting dump_config
         (or vice versa): if a key is applied but not dumped, the returned param_node will
         not contain it; if a key is dumped but not applied, the node's value will still be
         the constructor default.

         Usage:
         @code
         my_node t_node;
         my_binding t_binding;

         scarab::param_node t_config;
         t_config.add( "spectrum-size", scarab::param_value( 1024u ) );

         auto t_dumped = sandfly::testing::config_round_trip( &t_node, t_binding, t_config );

         // assert the node got the value
         assert( t_node.get_spectrum_size() == 1024u );
         // assert it also shows up in the dump
         assert( t_dumped["spectrum-size"]().as_uint() == 1024u );
         @endcode

         @param a_node     the node to configure (modified in place)
         @param a_binding  the binding whose do_apply_config / do_dump_config are called
         @param a_config   configuration to apply
         @return           the dumped configuration after applying a_config
        */
        template< class x_node, class x_binding >
        scarab::param_node config_round_trip( x_node* a_node,
                                              const x_binding& a_binding,
                                              const scarab::param_node& a_config )
        {
            a_binding.apply_config( a_node, a_config );
            scarab::param_node t_dumped;
            a_binding.dump_config( a_node, t_dumped );
            return t_dumped;
        }

    } // namespace testing
} // namespace sandfly

#endif /* SANDFLY_TEST_BINDING_HH_ */
