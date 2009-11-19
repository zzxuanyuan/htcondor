
file (GLOB qmgmtElements qmgmt_send_stubs* qmgr_lib_support* qmgr_job_updater*)
file( GLOB scheddRmvElements Example* *.t.* ${qmgmtElements} )

condor_daemon_glob( schedd "${scheddRmvElements}" ON)
add_library(qmgmt STATIC ${qmgmtElements} qmgmt_common.cpp )

condor_set_link_libs( schedd "procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils;${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND}" )




