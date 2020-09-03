# sandfly.cmake
# Macros for building a project using Sandfly
# Author: N.S. Oblath

# Main directory for Sandfly
set( SANDFLY_DIR ${CMAKE_CURRENT_LIST_DIR}/.. )
MESSAGE( STATUS "sandfly dir: ${SANDFLY_DIR}" )

macro (sandfly_build_executables)
	add_subdirectory( ${SANDFLY_DIR}/executables )
endmacro ()
