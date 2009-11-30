
file( GLOB startdRmvElements Example* *.t.* glexec_wrapper*)
condor_daemon_glob( startd "${startdRmvElements}" ON)

condor_set_link_libs ( startd "procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils;${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" )




