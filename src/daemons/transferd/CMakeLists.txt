#set the local target 
set(TargetName condor_transferd)

file( GLOB transferdRmvElements Example* *.t.* )
condor_glob( transferdHDRS transferdSRCS "${transferdRmvElements}")
gsoap_gen(transferd transferdHDRS transferdSRCS)

#Add the executable target.
add_executable(${TargetName} ${transferdHDRS} ${transferdSRCS})

# update the dependencies based on options
if (GSOAP_FOUND)
	add_dependencies(${TargetName} gen_transferd_soapfiles)
endif()

 set ( transferdLinkDeps procd_client daemon_core daemon_client procapi cedar privsep classad.old sysapi ccb utils ${GSOAP_FOUND} ${PCRE_FOUND} ${OPENSSL_FOUND} ${COREDUMPER_FOUND} )

if ( NOT WINDOWS )
set ( transferdLinkDeps -Wl,--start-group ${transferdLinkDeps} -Wl,--end-group )
endif()

target_link_libraries( ${TargetName} ${transferdLinkDeps} )



