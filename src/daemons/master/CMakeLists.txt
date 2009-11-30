
file( GLOB MasterRmvElements Example* )
condor_daemon_glob(master "${MasterRmvElements}" ON)

condor_set_link_libs ( master "procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils;${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")




