###########
# control #
###########

set( headers
    batch_executor.hh
    butterfly_house.hh
    control_access.hh
    daq_control.hh
    egg_writer.hh
    monarch3_wrap.hh
    node_builder.hh
    persistent_store.hh
    request_receiver.hh
    run_server.hh
    server_config.hh
    stream_manager.hh
    stream_preset.hh
)

set( sources
    batch_executor.cc
    butterfly_house.cc
    control_access.cc
    daq_control.cc
    egg_writer.cc
    monarch3_wrap.cc
    node_builder.cc
    persistent_store.cc
    request_receiver.cc
    run_server.cc
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

midge_library( SandflyControl sources dependencies )
pbuilder_install_headers( ${headers} )


