###########
# control #
###########

include_directories( BEFORE
    ${PROJECT_SOURCE_DIR}/library/utility
    ${PROJECT_SOURCE_DIR}/library/control
)

set( headers
    batch_executor.hh
    conductor.hh
    control_access.hh
    node_builder.hh
    request_receiver.hh
    run_control.hh
    server_config.hh
    stream_manager.hh
    stream_preset.hh
)

set( sources
    batch_executor.cc
    conductor.cc
    control_access.cc
    node_builder.cc
    request_receiver.cc
    run_control.cc
    server_config.cc
    stream_manager.cc
    stream_preset.cc
)

set( dependencies
    SandflyUtility
)


###########
# library #
###########

pbuilder_library(
    TARGET SandflyControl
    SOURCES ${sources}
    PROJECT_LIBRARIES ${dependencies}
)

pbuilder_install_headers( ${headers} )


