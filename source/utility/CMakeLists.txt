###########
# utility #
###########

set( headers
    locked_resource.hh
    message_relayer.hh
    sandfly_constants.hh
    sandfly_error.hh
    sandfly_version.hh
)
set( sources
    message_relayer.cc
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

midge_library( SandlfyUtility sources dependencies )
pbuilder_install_headers( ${headers} )

