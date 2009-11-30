
file( GLOB ProcdRmvElements *_test* proc_family_client* proc_family_io* local_client.UNIX* procd_ctl*)
#elements in here require .linux extension after looking at Imake
condor_daemon_glob(procd "${ProcdRmvElements}" OFF)

if (WINDOWS)
	set (PROCD_WIN_LINK_LIBS psapi) 
		
	file( GLOB ProcClientElements proc_family_client*
                                  proc_family_io*
                                  local_client.WIN*
                                  local_client.h)
	
else()
	

    # TBD: clean this portion up.
	file( GLOB ProcClientElements proc_family_client*
                                  proc_family_io*
                                  local_client.UNIX*
								  local_client.h
                                  named_pipe_reader*
                                  named_pipe_writer*
                                  named_pipe_util*
                                  named_pipe_watchdog*)

	
endif()

condor_set_link_libs( procd "procapi;utils;${PROCD_WIN_LINK_LIBS}")

add_library(procd_client STATIC ${ProcClientElements} )