MACRO (CONDOR_DAEMON_GLOB _CNDR_TARGET _REMOVE_ELEMENTS _GEN_GSOAP )

	set(${_CNDR_TARGET}SOAP ${_GEN_GSOAP})
	set(${_CNDR_TARGET}_REMOVE_ELEMENTS ${_REMOVE_ELEMENTS}) 
    
    if (NOT ${_CNDR_TARGET}SOAP OR NOT HAVE_EXT_GSOAP )
		file ( GLOB ${_CNDR_TARGET}_REMOVE_ELEMENTS *soap* ${_REMOVE_ELEMENTS} )
    endif()
    
    #message(STATUS "DEBUG: ${${_CNDR_TARGET}SOAP} ${HAVE_EXT_GSOAP} ${${_CNDR_TARGET}_REMOVE_ELEMENTS}")
    
	condor_glob( ${_CNDR_TARGET}HDRS ${_CNDR_TARGET}SRCS "${${_CNDR_TARGET}_REMOVE_ELEMENTS}" )

    if ( ${_CNDR_TARGET}SOAP AND HAVE_EXT_GSOAP )		
		gsoap_gen( ${_CNDR_TARGET} ${_CNDR_TARGET}HDRS ${_CNDR_TARGET}SRCS )
	endif()

	#Add the executable target.
	add_executable(condor_${_CNDR_TARGET} ${${_CNDR_TARGET}HDRS} ${${_CNDR_TARGET}SRCS} )

	# update the dependencies based on options
	#if ( ${_CNDR_TARGET}SOAP AND HAVE_EXT_GSOAP )
	#	add_dependencies(condor_${_CNDR_TARGET} gen_${_CNDR_TARGET}_soapfiles)
	#endif()

	install (TARGETS condor_${_CNDR_TARGET}
			 RUNTIME DESTINATION sbin )

ENDMACRO (CONDOR_DAEMON_GLOB)