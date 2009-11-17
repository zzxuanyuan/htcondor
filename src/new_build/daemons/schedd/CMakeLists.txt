#set the local target 
set(TargetName condor_schedd)

file (GLOB qmgmtElements qmgmt_send_stubs* qmgr_lib_support* qmgr_job_updater*)
file( GLOB scheddRmvElements Example* *.t.* ${qmgmtElements} )
condor_glob( scheddHDRS scheddSRCS "${scheddRmvElements}")
gsoap_gen(schedd scheddHDRS scheddSRCS)

#Add the executable target.
add_executable(${TargetName} ${scheddHDRS} ${scheddSRCS})
add_library(qmgmt STATIC ${qmgmtElements} qmgmt_common.cpp )

# update the dependencies based on options
if (GSOAP_FOUND)
	add_dependencies(${TargetName} gen_schedd_soapfiles)
endif()

 set ( scheddLinkDeps procd_client daemon_core daemon_client procapi cedar privsep classad.old sysapi ccb utils ${GSOAP_FOUND} ${PCRE_FOUND} ${OPENSSL_FOUND} ${COREDUMPER_FOUND} )

if ( NOT WINDOWS )
set ( scheddLinkDeps -Wl,--start-group ${scheddLinkDeps} -Wl,--end-group )
endif()

target_link_libraries( ${TargetName} ${scheddLinkDeps} )



