
## Create targets
file( GLOB shadowRmvElements Example* *.t.* )
condor_daemon_glob( shadow "${shadowRmvElements}" ON )

## set linkage
condor_set_link_libs( shadow "qmgmt;procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils;${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")

