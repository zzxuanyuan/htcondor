
condor_glob(CedarHdrs CedarSrcs "")

#create the library
add_library(cedar STATIC ${CedarHdrs} ${CedarSrcs})
