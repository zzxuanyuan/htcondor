#set the local target 
set(TargetName condor_procd)

#need to add the multiple targets separately...
# need to cleanup as there is too much goopyness here.  
#list(REMOVE_ITEM CONDOR_EXT_LINK_LIBS gsoap )

#elements in here require .linux extension after looking at Imake

file( GLOB ProcdRmvElements *_test* proc_family_client* proc_family_io* local_client.UNIX* procd_ctl*)

condor_glob(procdHDRS procdSRCS "${ProcdRmvElements}")

#create the library
add_executable(${TargetName} ${procdHDRS} ${procdSRCS})
target_link_libraries( ${TargetName} procapi utils)
