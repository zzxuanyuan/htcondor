#set the local target 
set(TargetName utils)

# items to be removed from the compile.
# platform specific files will be handled by condor_glob
file( GLOB RmvSrcs y.tab* lex.yy* param_info* syscall_numbers* libcondorapi_stubs* )

# I will need to add separate targets for

# param_info.c? Hmmm, how should we deal with this?

# are the follow elements of standard universe?
# "stubgen" --?? Is this for standard univ? usedn by condor_syscall_lib
# syscall_numbers?  Also why if this softlinked that is insane.

condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

#create the library
add_library(${TargetName} STATIC ${HeaderFiles} ${SourceFiles})
target_link_libraries(${TargetName} daemon_core daemon_client classad.old sysapi procapi privsep ${CONDOR_EXT_LINK_LIBS})
