
MACRO ( GSOAP_GEN _DAEMON _HDRS _SRCS )

if ( HAVE_EXT_GSOAP )

	set ( ${_DAEMON}_SOAP_SRCS
		soap_${_DAEMON}C.cpp
		soap_${_DAEMON}Server.cpp )

	set ( ${_DAEMON}_SOAP_HDRS
		soap_${_DAEMON}H.h
		soap_${_DAEMON}Stub.h )

	#TODO update all the output targets so clean will 
	#remove all soap generated things.  
	add_custom_command(
		OUTPUT ${${_DAEMON}_SOAP_SRCS} ${${_DAEMON}_SOAP_HDRS} condor.xsd
		COMMAND ${SOAPCPP2}
		ARGS -I ../../libs/daemon_core -S -L -x -p soap_${_DAEMON} gsoap_${_DAEMON}.h
		COMMENT "Generating ${_DAEMON} soap files" )

	if (NOT WINDOWS)
		add_custom_target(
			gen_${_DAEMON}_soapfiles
			ALL
			DEPENDS ${${_DAEMON}_SOAP_SRCS} )
	endif()

	# now append the header and srcs to incoming vars
	list(FIND ${_SRCS} "soap_${_DAEMON}C.cpp" SOAP_ALREADY_GLOBED )
	
	if ( NOT SOAP_ALREADY_GLOBED )
		list(APPEND ${_SRCS} ${${_DAEMON}_SOAP_SRCS} )
		list(APPEND ${_HDRS} ${${_DAEMON}_SOAP_HDRS} )
	endif()
	

endif()

ENDMACRO ( GSOAP_GEN )