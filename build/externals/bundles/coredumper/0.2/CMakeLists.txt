option(WITH_COREDUMPER "Compiling with support for COREDUMPER" ON)

if (WITH_COREDUMPER)

	if ( NOT PROPER )

		ExternalProject_Add(coredumper
				    PREFIX coredumper-1.2.3
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/coredumper-0.2.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/coredumper-0.2.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND make install )

		# Set the target dependencies which the rest of condor depends on.
		set(COREDUMPER_FOUND "${EXTERNAL_STAGE}/lib/libcoredumper.a")

	else( NOT PROPER )

		find_multiple( "coredumper" COREDUMPER_FOUND )

	endif( NOT PROPER )

	message (STATUS "external configured (COREDUMPER_FOUND=${COREDUMPER_FOUND})")
	set( COREDUMPER_FOUND ${COREDUMPER_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_COREDUMPER ON PARENT_SCOPE )

else(WITH_COREDUMPER)

	message (STATUS "external skiped (coredumper)")

endif(WITH_COREDUMPER)