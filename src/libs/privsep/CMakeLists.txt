
#file( GLOB PrivsepRmvSrcs  )

condor_glob(PrivsepHdrs PrivsepSrcs "")

#create the library
condor_static_lib( privsep "${PrivsepHdrs};${PrivsepSrcs}" )
