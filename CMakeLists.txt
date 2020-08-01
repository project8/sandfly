# Minimum cmake verison 3.1 required for the variable CMAKE_CXX_STANDARD
cmake_minimum_required (VERSION 3.1)

# Define the project
cmake_policy( SET CMP0048 NEW ) # version in project()
project( Sandfly VERSION 0.3.0 )

list( APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/midge/scarab/cmake )
include( PackageBuilder )

list( APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/midge/cmake )
include( MidgeUtility )

pbuilder_prepare_project()


#########
# flags #
#########

set( default__fpa_flag FALSE )
if( UNIX AND NOT APPLE )
    set( default__fpa_flag TRUE )
endif( UNIX AND NOT APPLE )

set_option( USE_CPP11 TRUE )

option( Sandfly_ENABLE_ITERATOR_TIMING "Flag to enable iterator time profiling" FALSE )

# add an option to perform iterator time profiling
if( Sandfly_ENABLE_ITERATOR_TIMING )
    add_definitions( -DENABLE_ITERATOR_TIMING )
else( Sandfly_ENABLE_ITERATOR_TIMING )
    remove_definitions( -DENABLE_ITERATOR_TIMING )
endif( Sandfly_ENABLE_ITERATOR_TIMING )


################
# dependencies #
################

# Boost
# Boost (1.48 required for container; scarab minimum is 1.46)
find_package( Boost 1.48.0 REQUIRED )

# Midge
pbuilder_add_submodule( Midge midge )
midge_process_options()

# Dripline
add_definitions( -DDRIPLINE_AUTH_FILE=~/.project8_authentications.json )
pbuilder_add_submodule( Dripline dripline-cpp )

# Monarch
pbuilder_add_submodule( Monarch monarch )
# we need the definitions for HAS_ATTR_IFC and HAS_GRP_IFC from Monarch to use the proper class interfaces in M3Header and M3Stream
set_property( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} APPEND PROPERTY COMPILE_DEFINITIONS ${Monarch_COMPILE_DEFINITIONS} )


###########
# sandfly #
###########


# build this project
add_subdirectory( library )

#add_subdirectory( source/applications )
#add_subdirectory( source/test )
