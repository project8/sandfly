# CMakeLists.txt for sandfly/library

pbuilder_use_sm_library( Midge Midge )

add_subdirectory( source/utility )
add_subdirectory( source/control )

pbuilder_component_install_and_export(
    COMPONENT Library
    LIBTARGETS SandflyUtility SandflyControl
)
