
if (NOT WIN_EXEC_NODE_ONLY)
## Create targets
file( GLOB shadowRmvElements Example* *.t.* )
condor_daemon( shadow "${shadowRmvElements}" "qmgmt;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" "${C_SBIN}" OFF )

add_dependencies ( condor_shadow syscall_numbers )

endif (NOT WIN_EXEC_NODE_ONLY)
