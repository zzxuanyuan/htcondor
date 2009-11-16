#set the local target 
set(TargetName condor_master)

file( GLOB MasterRmvElements Example* )
condor_glob( masterHDRS masterSRCS "${MasterRmvElements}")
gsoap_gen(master masterHDRS masterSRCS)

#Add the executable target.
add_executable(${TargetName} ${masterHDRS} ${masterSRCS})

# update the dependencies based on options
if (GSOAP_FOUND)
	add_dependencies(${TargetName} gen_master_soapfiles)
endif()

set ( masterLinkDeps procd_client daemon_core daemon_client procapi cedar privsep classad.old sysapi ccb utils ${GSOAP_FOUND} ${PCRE_FOUND} ${OPENSSL_FOUND} ${COREDUMPER_FOUND} )

if ( NOT WINDOWS )
set ( masterLinkDeps -Wl,--start-group ${masterLinkDeps} -Wl,--end-group )
endif()

target_link_libraries( ${TargetName} ${masterLinkDeps} )



