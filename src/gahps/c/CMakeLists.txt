if (NOT WIN_EXEC_NODE_ONLY)

	file( GLOB RmvSrcs gahp.cpp io_loop.* )
	condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

	condor_exe( condor_c-gahp "${HeaderFiles};${SourceFiles}" ${C_SBIN} "qmgmt;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${LIBVIRT_FOUND};${COREDUMPER_FOUND}" )

endif()