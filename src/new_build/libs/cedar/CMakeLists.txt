#set the local target 
set(TargetName cedar)

condor_glob(CedarHdrs CedarSrcs)

#create the library
add_library(${TargetName} STATIC ${CedarHdrs} ${CedarSrcs})
