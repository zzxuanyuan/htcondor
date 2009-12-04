
file( GLOB RmvSrcs y.tab* lex.yy* syscall_numbers* libcondorapi_stubs* soap_helpers* test_* *.t.cpp *test.cpp cat_url.cpp)

# I've removed all the stubgen deps as I do not see
# (yet) how they are included into the utils project
# "stubgen" --?? Is this for standard univ? usedn by condor_syscall_lib
# syscall_numbers?  Also why if this softlinked that is insane.

condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

add_custom_command( OUTPUT param_info_init.c 
					COMMAND perl
					ARGS param_info_c_generator.pl )					

add_library( utils STATIC ${HeaderFiles} ${SourceFiles} )
#add_file_dependencies(param_info.cpp param_info_init.c)
