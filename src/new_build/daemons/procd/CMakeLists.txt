#set the local target 
set(TargetName procd)

file( GLOB ProcdRmvElements *_test* )

condor_glob(procdHDRS procdSRCS "${ProcdRmvElements}")

#create the library
add_executable(${TargetName} ${procdHDRS} ${procdSRCS})
target_link_libraries( ${TargetName} utils procapi classad.old sysapi utils )
