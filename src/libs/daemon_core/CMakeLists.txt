
file( GLOB DCRmvElements soap_daemon_core* )

condor_glob( DCHeaderFiles DCSourceFiles "${DCRmvElements}" )

#create the library
add_library(daemon_core STATIC ${DCHeaderFiles} ${DCSourceFiles})
