#set the local target 
set(TargetName privsep)

#file( GLOB PrivsepRmvSrcs  )

condor_glob(PrivsepHdrs PrivsepSrcs "")

#create the library
add_library( ${TargetName} STATIC ${PrivsepHdrs} ${PrivsepSrcs} )
target_link_libraries( ${TargetName} utils )