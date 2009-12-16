
file( GLOB DCRmvElements soap_daemon_core* )

condor_glob( DCHeaderFiles DCSourceFiles "${DCRmvElements};soap_core.cpp;mimetypes.cpp" )

#On windows we add the cpp file into the build 
if (GSOAP_SRC)
	set_source_files_properties(${GSOAP_SRC} PROPERTIES GENERATED TRUE)
	list(APPEND DCSourceFiles ${GSOAP_SRC} )
endif()

#create the library
add_library(daemon_core STATIC ${DCHeaderFiles} ${DCSourceFiles})
