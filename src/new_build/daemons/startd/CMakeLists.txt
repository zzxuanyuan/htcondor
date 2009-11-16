#set the local target 
set(TargetName condor_collector)

file( GLOB collectorRmvElements Example* )
condor_glob( collectorHDRS collectorSRCS "${collectorRmvElements}")
gsoap_gen(collector collectorHDRS collectorSRCS)

#Add the executable target.
add_executable(${TargetName} ${collectorHDRS} ${collectorSRCS})

# update the dependencies based on options
if (GSOAP_FOUND)
	add_dependencies(${TargetName} gen_collector_soapfiles)
endif()

 set ( collectorLinkDeps procd_client daemon_core daemon_client procapi cedar privsep classad.old sysapi ccb utils ${GSOAP_FOUND} ${PCRE_FOUND} ${OPENSSL_FOUND} ${COREDUMPER_FOUND} )

if ( NOT WINDOWS )
set ( collectorLinkDeps -Wl,--start-group ${collectorLinkDeps} -Wl,--end-group )
endif()

target_link_libraries( ${TargetName} ${collectorLinkDeps} )



