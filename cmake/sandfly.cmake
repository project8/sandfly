# sandfly.cmake
# Macros for building a project using Sandfly
# Author: N.S. Oblath

# Main directory for Sandfly
set( SANDFLY_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/.. )
message( STATUS "sandfly dir: ${SANDFLY_SOURCE_DIR}" )

list( APPEND CMAKE_MODULE_PATH ${SANDFLY_SOURCE_DIR}/midge/scarab/cmake )
include( PackageBuilder )

list( APPEND CMAKE_MODULE_PATH ${SANDFLY_SOURCE_DIR}/midge/cmake )
include( MidgeUtility )


function( build_sandfly_executable )
	# Builds a the sandfly executable
	#
	# User is responsible for including the resulting executable in a target export.
    #
    # Parameters
	#     SANDFLY_SUBMODULE_NAME: libraries to be linked against; only used if sandfly is a submodule of a parent project (the typical use case)
	#     ALT_NAME: name for the executable, only needed if something other than "sandfly" is desired
	#     ALT_SOURCES: source file for the executable, only needed if something other than "sandfly.cc" is desired
	#     PROJECT_LIBRARIES: if building from a parent project, this should be the set of targets in the parent project to link against this executable
	#     EXTERNAL_LIBRARIES: if there are external libraries to link against this article, they should be specified here
	#

	set( OPTIONS )
	set( ONEVALUEARGS SANDFLY_SUBMODULE_NAME ALT_NAME )
	set( MULTIVALUEARGS ALT_SOURCES PROJECT_LIBRARIES EXTERNAL_LIBRARIES )
	cmake_parse_arguments( EXE "${OPTIONS}" "${ONEVALUEARGS}" "${MULTIVALUEARGS}" ${ARGN} )

	if( NOT EXE_ALT_NAME )
		set( EXE_ALT_NAME "sandfly" )
	endif()

	message( STATUS "Building main sandfly executable as <${ALT_EXE_NAME}> from project <${CMAKE_PROJECT_NAME}>" )

	if( NOT EXE_ALT_SOURCES )
		set( EXE_ALT_SOURCES
			sandfly.cc
		)
	endif()

	if( CMAKE_PROJECT_NAME STREQUAL "Sandfly" )
		list( APPEND EXE_PROJECT_LIBRARIES SandflyControl SandflyUtility )
		message( STATUS "Building Sandfly executable(s) as part of Sandfly" )
	else()
		if( NOT EXE_SANDFLY_SUBMODULE_NAME )
			message( FATAL_ERROR "Sandfly project name was not specified when building Sandfly executables.  Check the call to sandfly_build_executables()" )
		endif()
		pbuilder_use_sm_library( SandflyControl ${EXE_SANDFLY_SUBMODULE_NAME} )
		pbuilder_use_sm_library( SandflyUtility ${EXE_SANDFLY_SUBMODULE_NAME} )
		message( STATUS "Building Sandfly executable(s) as part of ${CMAKE_PROJECT_NAME}" )
	endif()

	set( sandfly_exe_PROGRAMS )
	pbuilder_executable( 
		SOURCES ${EXE_ALT_SOURCES}
		EXECUTABLE ${EXE_ALT_NAME}
		PROJECT_LIBRARIES ${EXE_PROJECT_LIBRARIES}
		PUBLIC_EXTERNAL_LIBRARIES ${EXE_EXTERNAL_LIBRARIES}
		#PRIVATE_EXTERNAL_LIBRARIES ${PRIVATE_EXT_LIBS}
	)

endfunction()
