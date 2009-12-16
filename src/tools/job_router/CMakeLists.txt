
set (JR_LINK_LIBS "qmgmt;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}")

condor_glob(JRHdrs JRSrcs "VanillaToGrid_main.cpp")

condor_exe( job_router "${JRHdrs};${JRSrcs}" libexec "${JR_LINK_LIBS}")
condor_exe( router_q "${JRHdrs};${JRSrcs}" bin "${JR_LINK_LIBS}")
condor_exe( router_rm "${JRHdrs};${JRSrcs}" bin "${JR_LINK_LIBS}")
condor_exe( router_history "${JRHdrs};${JRSrcs}" bin "${JR_LINK_LIBS}")
