
file( GLOB negotiatorRmvElements Example* )
condor_daemon_glob( negotiator "${negotiatorRmvElements}" OFF )

condor_set_link_libs( negotiator "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" )




