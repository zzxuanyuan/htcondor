
file( GLOB rmvLMFiles *test*)

condor_glob(LMHdrs LMSrcs "${rmvLMFiles}*")

condor_exe( lease_manager "${LMHdrs};${LMSrcs}" sbin "conversion;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}")