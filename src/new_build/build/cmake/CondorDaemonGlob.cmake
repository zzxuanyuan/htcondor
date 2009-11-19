MACRO (CONDOR_DAEMON_GLOB _CNDR_TARGET _REMOVE_ELEMENTS _GEN_GSOAP )

	set(${_CNDR_TARGET}SOAP ${_GEN_GSOAP})

	condor_glob( ${_CNDR_TARGET}HDRS ${_CNDR_TARGET}SRCS "${_REMOVE_ELEMENTS}" )

    if ( ${_CNDR_TARGET}SOAP AND GSOAP_FOUND )		
		gsoap_gen( ${_CNDR_TARGET} ${_CNDR_TARGET}HDRS ${_CNDR_TARGET}SRCS )
	endif()

	#Add the executable target.
	add_executable(condor_${_CNDR_TARGET} ${${_CNDR_TARGET}HDRS} ${${_CNDR_TARGET}SRCS} )

	# update the dependencies based on options
	if ( ${_CNDR_TARGET}SOAP AND ${GSOAP_FOUND})
		add_dependencies(condor_${_CNDR_TARGET} gen_${_CNDR_TARGET}_soapfiles)
	endif()

	install (TARGETS condor_${_CNDR_TARGET}
			 RUNTIME DESTINATION sbin )

ENDMACRO (CONDOR_DAEMON_GLOB)