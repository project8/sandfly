# Minimum cmake verison 3.12 required by scarab
cmake_minimum_required (VERSION 3.12)

# Define the project
cmake_policy( SET CMP0048 NEW ) # version in project()
project( Sandfly VERSION 0.5.2 )

list( APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )
include( sandfly )


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


#####################
# prepare for build #
#####################

pbuilder_prepare_project()


##############
# submodules #
##############

# Midge
pbuilder_add_submodule( Midge midge )
midge_process_options()

# Dripline
pbuilder_add_submodule( Dripline dripline-cpp )


###########
# sandfly #
###########

# build the sandfly libraries
add_subdirectory( library )


###############
# executables #
###############

if( Sandfly_ENABLE_EXECUTABLES )
    add_subdirectory( executables )
endif()

##################
# package config #
##################

configure_file( ${PROJECT_SOURCE_DIR}/SandflyConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/SandflyConfig.cmake @ONLY )

pbuilder_do_package_config()
