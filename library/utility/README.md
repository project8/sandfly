# sandfly/source/utility

This directory contains utility classes and functions.

## sandfly Testing Helpers

Thin sandfly-specific layer on top of the [midge testing harness](../../midge/library/testing/README.md).
Read that document first — it covers all the core constraints (heap allocation, naming,
copy prohibition, shutdown, watchdog).

This header adds one sandfly-specific concern: testing node **bindings**.

### `config_round_trip`

```cpp
#include "sandfly_test_binding.hh"

my_node    t_node;
my_binding t_binding;

scarab::param_node t_config;
t_config.add( "spectrum-size", scarab::param_value( 1024u ) );

auto t_dumped = sandfly::testing::config_round_trip( &t_node, t_binding, t_config );

assert( t_node.get_spectrum_size() == 1024u );            // apply worked
assert( t_dumped["spectrum-size"]().as_uint() == 1024u ); // dump worked
```

`config_round_trip` calls `apply_config` followed by `dump_config` and returns the dumped
`param_node`. This catches the common bug of adding a key to one side but not the other.

### Node registry

`REGISTER_NODE_AND_BUILDER` in a node's `.cc` file is a static-initialization side effect.
A test executable that links the node's library gets the registry populated automatically.
No explicit registration call is needed in test code.

### include path

Add the sandfly `library/testing` directory to your project's include path:

```cmake
include_directories( BEFORE ${Sandfly_INCLUDE_DIR}/testing )
# -- or, if building in-tree --
include_directories( BEFORE ${PROJECT_SOURCE_DIR}/sandfly/library/testing )
```
