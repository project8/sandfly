# CMakeLists.txt for sandfly/executables
# Author: N.S. Oblath
# Created: Sept. 2, 2020
##########

# Why the setup with sandfly_exe_PROGRAMS set as empty here, then appended to?
# This follows the pattern of how other packages in the Project 8 codebase are setup, 
# and it will be useful if there are other executables in the future.
set( sandfly_exe_PROGRAMS )

# Sandfly executable
build_sandfly_executable()

list( APPEND sandfly_exe_PROGRAMS sandfly )


# Export
pbuilder_component_install_and_export( 
    COMPONENT Executables
    EXETARGETS ${sandfly_exe_PROGRAMS}
)
