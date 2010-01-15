MACRO (FIND_MULTIPLE _NAMES _VAR_FOUND)

	## Normally find_library will exist on 1st match, for some windows libs 
	## there is one target and multiple libs
	if (CONDOR_CMAKE_DEBUG)
		message(STATUS "SEARCHING ... (${_NAMES})")
	endif()

	foreach ( loop_var ${_NAMES} )
	
		find_library( ${_VAR_FOUND}_SEARCH_${loop_var} NAMES ${loop_var} )

			if (NOT ${${_VAR_FOUND}_SEARCH_${loop_var}} STREQUAL "${_VAR_FOUND}_SEARCH_${loop_var}-NOTFOUND" )

				if (${_VAR_FOUND})
					list(APPEND ${_VAR_FOUND} ${${_VAR_FOUND}_SEARCH_${loop_var}} )
				else()
					set (${_VAR_FOUND} ${${_VAR_FOUND}_SEARCH_${loop_var}} )
				endif()
			endif()

	endforeach(loop_var)

	if (${_VAR_FOUND})
		if (CONDOR_CMAKE_DEBUG)
			message(STATUS "FOUND ... (${${_VAR_FOUND}})")
		endif()
	else()
		message(FATAL_ERROR "Could not find libs(${_NAMES})")
	endif()

ENDMACRO (FIND_MULTIPLE)