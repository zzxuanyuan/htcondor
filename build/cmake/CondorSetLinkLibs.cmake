
MACRO (CONDOR_SET_LINK_LIBS _CNDR_TARGET _LINK_LIBS)

#message(STATUS "DEBUG: LIBS=${_LINK_LIBS}")

set( ${_CNDR_TARGET}LinkDeps ${_LINK_LIBS} )

if ( NOT WINDOWS )
	set ( ${_CNDR_TARGET}LinkDeps -Wl,--start-group ${${_CNDR_TARGET}LinkDeps} -Wl,--end-group )
else()
	set( ${_CNDR_TARGET}LinkDeps "${${_CNDR_TARGET}LinkDeps};${CONDOR_WIN_LIBS}" )
endif()

if ( NOT ${_CNDR_TARGET} MATCHES "condor" )
	target_link_libraries( condor_${_CNDR_TARGET} ${${_CNDR_TARGET}LinkDeps} )
else()
	target_link_libraries( ${_CNDR_TARGET} ${${_CNDR_TARGET}LinkDeps} )
endif()

ENDMACRO (CONDOR_SET_LINK_LIBS)