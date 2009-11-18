
file( GLOB ClassAdsOldRmvSrcs classad_lookup* *test*)

condor_glob( ClassAdsOldHdrs ClassAdsOldSrcs "${ClassAdsOldRmvSrcs}" )

#create the library
add_library( classad.old STATIC ${ClassAdsOldHdrs} ${ClassAdsOldSrcs} )
