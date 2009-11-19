
file( GLOB ProcdRmvElements *_test* proc_family_client* proc_family_io* local_client.UNIX* procd_ctl*)
#elements in here require .linux extension after looking at Imake
condor_daemon_glob(procd "${ProcdRmvElements}" OFF)

if (WINDOWS)
	condor_set_link_libs( procd psapi )
else()
	condor_set_link_libs( procd "procapi;utils")

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
