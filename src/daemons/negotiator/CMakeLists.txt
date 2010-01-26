
file( GLOB negotiatorRmvElements Example* )
condor_daemon( negotiator "${negotiatorRmvElements}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" "${C_SBIN}" OFF )





