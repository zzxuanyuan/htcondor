
file( GLOB MasterRmvElements Example* )

condor_daemon(master "${MasterRmvElements}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" sbin OFF)




