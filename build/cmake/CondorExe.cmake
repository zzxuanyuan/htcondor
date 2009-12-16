MACRO (CONDOR_EXE _CNDR_TARGET _SRCS _INSTALL_LOC _LINK_LIBS )


	if ( NOT ${_CNDR_TARGET} MATCHES "condor" )
		set (local_${_CNDR_TARGET} condor_${_CNDR_TARGET})
	else()
		set (local_${_CNDR_TARGET} ${_CNDR_TARGET})
	endif()

	add_executable( ${local_${_CNDR_TARGET}} ${_SRCS})

	condor_set_link_libs( ${local_${_CNDR_TARGET}} "${_LINK_LIBS}")
	
	install (TARGETS ${local_${_CNDR_TARGET}}
			 RUNTIME DESTINATION ${_INSTALL_LOC} )

ENDMACRO (CONDOR_EXE)