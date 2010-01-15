
condor_glob(CedarHdrs CedarSrcs "")

#create the library
condor_static_lib(cedar "${CedarHdrs};${CedarSrcs}")
