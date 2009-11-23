
file( GLOB RmvSrcs y.tab* lex.yy* syscall_numbers* libcondorapi_stubs* soap_helpers* test_* *.t.cpp test.cpp ckpt_* cat_url.cpp)

# I've removed all the stubgen deps as I do not see
# (yet) how they are included into the utils project
# "stubgen" --?? Is this for standard univ? usedn by condor_syscall_lib
# syscall_numbers?  Also why if this softlinked that is insane.

condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

# call perl to gen the params.  
command_target( utils_genparams perl param_info_c_generator.pl param_info_init.c )

add_library( utils STATIC ${HeaderFiles} ${SourceFiles} )
add_dependencies(utils utils_genparams)
