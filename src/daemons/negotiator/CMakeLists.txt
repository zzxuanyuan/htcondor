
file( GLOB negotiatorRmvElements Example* )
condor_daemon_glob( negotiator "${negotiatorRmvElements}" ON )

condor_set_link_libs( negotiator "procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils;${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" )




