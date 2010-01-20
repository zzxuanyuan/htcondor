option(WITH_EXPAT "Compiling with support for EXPAT" ON)

if (WITH_EXPAT)

	if ( NOT PROPER )

		ExternalProject_Add(expat
				    PREFIX expat-1.2.3
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/expat-2.0.1.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/expat-2.0.1.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND make install )

		# Set the target dependencies which the rest of condor depends on.
		set(EXPAT_FOUND "${EXTERNAL_STAGE}/lib/libexpat.a")

	else( NOT PROPER )

		find_multiple( "expat" EXPAT_FOUND )

	endif( NOT PROPER )

	message (STATUS "external configured (EXPAT_FOUND=${EXPAT_FOUND})")
	set( EXPAT_FOUND ${EXPAT_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_EXPAT ON PARENT_SCOPE )

else(WITH_EXPAT)

	message (STATUS "external skiped (expat)")

endif(WITH_EXPAT)