#set the local target 
set(TargetName cedar)

include_directories(${CONDOR_SOURCE_DIR}/libs/cedar)

condor_glob(CedarHdrs CedarSrcs)

#create the library
add_library(${TargetName} STATIC ${CedarHdrs} ${CedarSrcs})
