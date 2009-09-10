#set the local target 
set(TargetName cedar)

file( GLOB CedarRmvSrcs proc.cpp )

condor_glob(CedarHdrs CedarSrcs "${CedarRmvSrcs}")

#create the library
add_library(${TargetName} STATIC ${CedarHdrs} ${CedarSrcs})
