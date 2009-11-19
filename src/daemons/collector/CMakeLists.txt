
## create targets
file( GLOB collectorRmvElements Example* )
condor_daemon_glob(collector "${collectorRmvElements}" ON)

## set linkage
condor_set_link_libs ( collector "procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils;${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND}")




