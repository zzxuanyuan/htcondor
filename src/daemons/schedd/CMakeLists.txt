
file( GLOB qmgmtElements qmgmt_send_stubs* qmgr_lib_support* qmgr_job_updater* )
file( GLOB scheddRmvElements Example* *.t.* ${qmgmtElements} )

condor_daemon( schedd "${scheddRmvElements}" "${CONDOR_LIBS};${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" "${C_SBIN}" ON)

add_library( qmgmt STATIC ${qmgmtElements} qmgmt_common.cpp )






