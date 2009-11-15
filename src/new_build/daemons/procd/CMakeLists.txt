#set the local target 
set(TargetName condor_procd)

file( GLOB ProcdRmvElements *_test* proc_family_client* proc_family_io* local_client.UNIX* procd_ctl*)

#elements in here require .linux extension after looking at Imake
condor_glob( procdHDRS procdSRCS "${ProcdRmvElements}" )

#create the library
add_executable(${TargetName} ${procdHDRS} ${procdSRCS})

if (WINDOWS)
	target_link_libraries( ${TargetName} psapi )
else()
	target_link_libraries( ${TargetName} procapi utils)

    # TBD: clean this portion up.
	file( GLOB ProcClientElements proc_family_client*
                                  proc_family_io*
                                  local_client.UNIX*
                                  named_pipe_reader*
                                  named_pipe_writer*
                                  named_pipe_util*
                                  named_pipe_watchdog*)

	add_library(procd_client STATIC ${ProcClientElements} )
endif()

message(STATUS "**TBD** Need to deal with procd tests apps")