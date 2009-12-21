
file( GLOB bgpScripts bgp_*)
file( GLOB OtherSrc htc_*)

condor_glob(startdFHdrs startdFSrcs "${bgpScripts};${OtherSrc}")

condor_exe( condor_startd_factory "${startdFHdrs};${startdFSrcs}" sbin "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}")

message(STATUS "TODO: TSTCLAIR add scripts to install targets")