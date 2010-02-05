


	file( GLOB qmgmtElements qmgmt_send_stubs* qmgr_lib_support* qmgr_job_updater* )
	file( GLOB scheddRmvElements Example* *.t.* ${qmgmtElements} )

if (NOT WIN_EXEC_NODE_ONLY)
	condor_daemon( schedd "${scheddRmvElements}" "${CONDOR_LIBS};${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}" "${C_SBIN}" ON)
endif(NOT WIN_EXEC_NODE_ONLY)

	add_library( qmgmt STATIC ${qmgmtElements} qmgmt_common.cpp )







