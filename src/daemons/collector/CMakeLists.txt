
## create targets
file( GLOB collectorRmvElements Example* )
condor_daemon_glob(collector "${collectorRmvElements}" ON)

## set linkage
condor_set_link_libs ( collector "${CONDOR_LIBS};${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")




