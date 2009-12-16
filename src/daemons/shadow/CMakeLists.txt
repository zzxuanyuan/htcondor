
## Create targets
file( GLOB shadowRmvElements Example* *.t.* )
condor_daemon_glob( shadow "${shadowRmvElements}" OFF )

## set linkage
condor_set_link_libs( shadow "qmgmt;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")

