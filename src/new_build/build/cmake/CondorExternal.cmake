# condor_external - will search for an external based on it's input
#                   params, and set a series of VARS for further the build.
#
#
# IN: 
#     _PACKAGE        - Name of external package
#     _NAMES          - "Names of libraries.lib/.a/.so to search for"
#     _ON_OFF         - Indicates if we should perform the search (ON/OFF)
#
# OUT:
#     WITH_${UP_PACKAGE}     - Indicated that the option is enabled
#     ${UP_PACKAGE}_FOUND    - Indicates the package has been found
#                            If the package is found the link and include
#                            deps will be appended to.
#

MACRO (CONDOR_EXTERNAL _PACKAGE _ON_OFF _NAMES ) 

	string( TOUPPER "${_PACKAGE}" UP_PACKAGE )
	option(WITH_${UP_PACKAGE} "Enable or disable package" ${_ON_OFF})	
	
	# if the option is enabled _ON_OFF,
	if (WITH_${UP_PACKAGE})

		message(STATUS "condor_external searching(${UW_EXTERNALS_DIR}) for ${_PACKAGE} libs (${_NAMES})")

		find_library( ${UP_PACKAGE}_SEARCH NAMES ${_NAMES} )

        ##############
		if (${UP_PACKAGE}_SEARCH STREQUAL "${UP_PACKAGE}_SEARCH-NOTFOUND" )
			
			if(${STRICT})
				message(FATAL_ERROR "condor_external (${_PACKAGE})... *not* found, to disable check set WITH_${UP_PACKAGE} to OFF")
			else()
				message(STATUS "condor_external (${_PACKAGE})... *not* found, to disable check set WITH_${UP_PACKAGE} to OFF")
			endif()						
			
		else()
		    set (${UP_PACKAGE}_FOUND ${${UP_PACKAGE}_SEARCH} )
			set(HAVE_EXT_${UP_PACKAGE} ON)
			set(HAVE_${UP_PACKAGE} ON)
			message(STATUS "condor_external (${_PACKAGE})... found (${${_PACKAGE}_FOUND})")

		endif()
		##############

	else()
		message(STATUS "condor_external skipping search WITH_${UP_PACKAGE} = OFF")
	endif()

ENDMACRO (CONDOR_EXTERNAL)