
#file( GLOB PrivsepRmvSrcs  )

condor_glob(PrivsepHdrs PrivsepSrcs "")

#create the library
add_library( privsep STATIC ${PrivsepHdrs} ${PrivsepSrcs} )
