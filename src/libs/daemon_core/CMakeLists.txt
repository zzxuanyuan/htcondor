
file( GLOB DCRmvElements soap_daemon_core* )

condor_glob( DCHeaderFiles DCSourceFiles "${DCRmvElements}" )

#On windows we add the cpp file into the build 
if (GSOAP_SRC)
	list(APPEND DCSourceFiles ${GSOAP_SRC} )
endif()

#create the library
add_library(daemon_core STATIC ${DCHeaderFiles} ${DCSourceFiles})
