#set the local target 
set(TargetName utils)

file( GLOB RmvSrcs y.tab* lex.yy* syscall_numbers* libcondorapi_stubs* )

# I've removed all the stubgen deps as I do not see
# (yet) how they are included into the utils project
# "stubgen" --?? Is this for standard univ? usedn by condor_syscall_lib
# syscall_numbers?  Also why if this softlinked that is insane.

condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

add_custom_command(
    OUTPUT param_info_init.c
	COMMAND perl
	ARGS param_info_c_generator.pl
	COMMENT "Generating param_info_init.c"
)

# I hate having to create a target which does nothing.
# I tried adding as a PREBUILD dep to utils but no dice
# which doesn't make any sense given examples.  
add_custom_target(
	utils_genparams
	ALL
	DEPENDS param_info_init.c )

add_library( utils STATIC ${HeaderFiles} ${SourceFiles} )

