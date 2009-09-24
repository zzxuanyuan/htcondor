#set the local target 
set(TargetName condor_procd)

file( GLOB ProcdRmvElements *_test* proc_family_client* proc_family_io* local_client.UNIX* procd_ctl*)

#elements in here require .linux extension after looking at Imake
condor_glob(procdHDRS procdSRCS "${ProcdRmvElements}")

#create the library
add_executable(${TargetName} ${procdHDRS} ${procdSRCS})
target_link_libraries( ${TargetName} procapi utils)
if (WINDOWS)
	target_link_libraries( ${TargetName} psapi )
endif()


# there is a client library procd_client which doesn't appear to be used... ever... it includes (proc_family_client.o proc_family_io.o local_client.UNIX.o named_pipe_reader.o named_pipe_writer.o named_pipe_watchdog.o named_pipe_util.o)

message(STATUS "**TBD** Need to deal with procd tests apps")