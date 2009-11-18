
file( GLOB DCRmvElements *.t.* )

condor_glob(DCHeaderFiles DCSourceFiles "${DCRmvElements}")

#create the library
add_library(daemon_client) STATIC ${DCHeaderFiles} ${DCSourceFiles})
