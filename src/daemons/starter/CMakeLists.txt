
file( GLOB starterRmvElements condor_rsh.cpp glexec_job_wrapper* )

condor_daemon( starter "${starterRmvElements}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};qmgmt" "${C_SBIN}" OFF)

# need to add all the other install targets for libexec. 





