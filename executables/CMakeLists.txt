# CMakeLists.txt for sandfly/executables
# Author: N.S. Oblath
# Created: Sept. 2, 2020
##########

set( sandfly_exe_SOURCES
    sandfly.cc
)

set( sandfly_exe_LIBS 
    SandflyControl
    SandflyUtility
)


set( sandfly_exe_PROGRAMS )
pbuilder_executables( 
    SOURCES ${sandfly_exe_SOURCES}
    TARGETS_VAR sandfly_exe_PROGRAMS
    PROJECT_LIBRARIES ${sandfly_exe_LIBS}
    #PUBLIC_EXTERNAL_LIBRARIES ${PUBLIC_EXT_LIBS}
    #PRIVATE_EXTERNAL_LIBRARIES ${PRIVATE_EXT_LIBS}
)

pbuilder_component_install_and_export( 
    COMPONENT Executables
    EXETARGETS ${sandfly_exe_PROGRAMS}
)
