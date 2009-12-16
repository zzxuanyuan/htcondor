
file( GLOB startdRmvElements Example* *.t.* glexec_wrapper*)
condor_daemon_glob( startd "${startdRmvElements}" OFF)

condor_set_link_libs ( startd "procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils;${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" )




