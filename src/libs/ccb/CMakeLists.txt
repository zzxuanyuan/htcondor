
#file( GLOB ccbRmvSrcs  )
condor_glob(ccbHdrs ccbSrcs "")

#create the library
add_library(ccb STATIC ${ccbHdrs} ${ccbSrcs})

