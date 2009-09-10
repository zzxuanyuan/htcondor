#set the local target 
set(TargetName procd)

file( GLOB ProcdRmvElements *_test* proc_family_client* proc_family_io* local_client.UNIX* )

condor_glob(procdHDRS procdSRCS "${ProcdRmvElements}")

#create the library
add_executable(${TargetName} ${procdHDRS} ${procdSRCS})
target_link_libraries( ${TargetName} utils )
