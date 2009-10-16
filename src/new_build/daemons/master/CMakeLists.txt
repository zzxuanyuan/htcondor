#set the local target 
set(TargetName condor_master)

file( GLOB MasterRmvElements Example*)
condor_glob( masterHDRS masterSRCS "${MasterRmvElements}")
gsoap_gen(master masterHDRS masterSRCS)

#create the library
add_executable(${TargetName} ${masterHDRS} ${masterSRCS})
target_link_libraries( ${TargetName} ${GSOAP_FOUND} ${OPENSSL_FOUND} daemon_client daemon_core procapi cedar classad.old sysapi utils )



