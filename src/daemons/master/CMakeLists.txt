
file( GLOB MasterRmvElements Example* )
condor_daemon_glob(master "${MasterRmvElements}" OFF)

condor_set_link_libs ( master "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")




