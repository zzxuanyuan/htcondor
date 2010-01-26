
## create targets
file( GLOB collectorRmvElements Example* )
condor_daemon ( collector
                "${collectorRmvElements}"
                "${CONDOR_LIBS};${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}"
                ${C_SBIN}
                ON )




