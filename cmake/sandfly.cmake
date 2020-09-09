# sandfly.cmake
# Macros for building a project using Sandfly
# Author: N.S. Oblath

# Main directory for Sandfly
set( SANDFLY_DIR ${CMAKE_CURRENT_LIST_DIR}/.. )
message( STATUS "sandfly dir: ${SANDFLY_DIR}" )

list( APPEND CMAKE_MODULE_PATH ${SANDFLY_DIR}/midge/scarab/cmake )
include( PackageBuilder )

macro (sandfly_build_executables)
	add_subdirectory( ${SANDFLY_DIR}/executables )
endmacro ()
