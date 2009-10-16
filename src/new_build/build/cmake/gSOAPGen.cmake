
MACRO ( GSOAP_GEN _DAEMON _HDRS _SRCS )

if ( HAVE_EXT_GSOAP )

	set ( ${_DAEMON}_SOAP_SRCS
		soap_${_DAEMON}C.cpp
		soap_${_DAEMON}Stub.cpp
		soap_${_DAEMON}Server.cpp )
		#gsoap_${_DAEMON}.cpp )

	set ( ${_DAEMON}_SOAP_HDRS
		gsoap_${_DAEMON}.h
		soap${_DAEMON}H.h
		soap_${_DAEMON}Stub.h )

	add_custom_command(
		OUTPUT ${${_DAEMON}_SOAP_SRCS}
		COMMAND soapcpp2
		ARGS -I ../../libs/daemon_core -S -L -x -p soap_${_DAEMON} gsoap_${_DAEMON}.h
		COMMENT "Generating ${_DAEMON} soap files" )

	add_custom_target(
		gen_${_DAEMON}_soapfiles
		ALL
		DEPENDS ${${_DAEMON}_SOAP_SRCS} )

	# now append the header and srcs to incoming vars
	set (_SRCS ${_SRCS} ${${_DAEMON}_SOAP_SRCS} )
	set (_HDRS ${_HDRS} ${${_DAEMON}_SOAP_HDRS} )

endif()

ENDMACRO ( GSOAP_GEN )