#set the local target 
set(TargetName daemon_core)

file( GLOB DCRmvElements soap_daemon_core* )

condor_glob( DCHeaderFiles DCSourceFiles "${DCRmvElements}" )

#create the library
add_library(${TargetName} STATIC ${DCHeaderFiles} ${DCSourceFiles})
