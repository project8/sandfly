###########
# utility #
###########

include_directories( BEFORE
    ${PROJECT_SOURCE_DIR}/library/utility
    ${PROJECT_SOURCE_DIR}/library/testing
)

set( headers
    locked_resource.hh
    message_relayer.hh
    sandfly_return_codes.hh
    sandfly_error.hh
    sandfly_version.hh
)

set( testing_headers
    sandfly_test_binding.hh
)
set( sources
    message_relayer.cc
    sandfly_return_codes.cc
    sandfly_error.cc
)

configure_file( sandfly_version.cc.in ${CMAKE_CURRENT_BINARY_DIR}/sandfly_version.cc )
set( sources
    ${sources}
    ${CMAKE_CURRENT_BINARY_DIR}/sandfly_version.cc
)


###########
# library #
###########

pbuilder_library(
    TARGET SandflyUtility
    SOURCES ${sources}
)

pbuilder_install_headers( ${headers} )
pbuilder_install_headers( ${testing_headers} )

