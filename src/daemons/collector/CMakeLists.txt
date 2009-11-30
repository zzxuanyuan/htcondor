
## create targets
file( GLOB collectorRmvElements Example* )
condor_daemon_glob(collector "${collectorRmvElements}" ON)

set(COLLECTOR_LINK_LIBS "procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils;${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")

#if (WINDOWS)
#	set(COLLECTOR_LINK_LIBS "${COLLECTOR_LINK_LIBS};${KRB5_FOUND};${CONDOR_WIN_LIBS}")																
#endif()
#message(STATUS "COLLECTOR_LINK_LIBS=${COLLECTOR_LINK_LIBS}")

## set linkage
condor_set_link_libs ( collector "${COLLECTOR_LINK_LIBS}")




