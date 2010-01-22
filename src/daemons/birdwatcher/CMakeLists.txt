if (WINDOWS)

	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
	
	file( GLOB BWSrcs *.cpp *.h  res/*)
	
	condor_exe( condor_birdwatcher "${BWSrcs}" ${C_SBIN} "")


endif()