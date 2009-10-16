#set the local target 
set(TargetName classad.old)


file( GLOB ClassAdsOldRmvSrcs classad_lookup* *test*)

condor_glob( ClassAdsOldHdrs ClassAdsOldSrcs "${ClassAdsOldRmvSrcs}" )

#create the library
add_library( ${TargetName} STATIC ${ClassAdsOldHdrs} ${ClassAdsOldSrcs} )
