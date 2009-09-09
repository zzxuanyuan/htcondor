#set the local target 
set(TargetName daemon_client)

file( GLOB DCRmvElements *.t.* )

condor_glob(DCHeaderFiles DCSourceFiles "${DCRmvElements}")

#create the library
add_library(${TargetName} STATIC ${DCHeaderFiles} ${DCSourceFiles})
