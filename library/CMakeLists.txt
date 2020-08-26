# CMakeLists.txt for sandfly/library

pbuilder_use_sm_library( Midge Midge )

add_subdirectory( utility )
add_subdirectory( control )

pbuilder_component_install_and_export(
    COMPONENT Library
    LIBTARGETS SandflyUtility SandflyControl
)
