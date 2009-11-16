#set the local target 
set(TargetName condor_startd)

file( GLOB startdRmvElements Example* *.t.* glexec_wrapper*)
condor_glob( startdHDRS startdSRCS "${startdRmvElements}")
gsoap_gen(startd startdHDRS startdSRCS)

#Add the executable target.
add_executable(${TargetName} ${startdHDRS} ${startdSRCS})

# update the dependencies based on options
if (GSOAP_FOUND)
	add_dependencies(${TargetName} gen_startd_soapfiles)
endif()

 set ( startdLinkDeps procd_client daemon_core daemon_client procapi cedar privsep classad.old sysapi ccb utils ${GSOAP_FOUND} ${PCRE_FOUND} ${OPENSSL_FOUND} ${COREDUMPER_FOUND} )

if ( NOT WINDOWS )
set ( startdLinkDeps -Wl,--start-group ${startdLinkDeps} -Wl,--end-group )
endif()

target_link_libraries( ${TargetName} ${startdLinkDeps} )



