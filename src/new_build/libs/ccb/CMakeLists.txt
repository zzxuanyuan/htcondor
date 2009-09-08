#set the local target 
set(TargetName ccb)

condor_glob(ccbHdrs ccbSrcs)

#create the library
add_library(${TargetName} STATIC ${ccbHdrs} ${ccbSrcs})
