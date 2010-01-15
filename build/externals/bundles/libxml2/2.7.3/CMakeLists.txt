option(WITH_LIBXML2 "Compiling with support for LIBXML2" ON)

if (WITH_LIBXML2)

	if ( NOT PROPER )

		ExternalProject_Add(libxml2
				    PREFIX libxml2-2.7.3
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/libxml2-2.7.3.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE} --disable-shared
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND make install )

		# Set the target dependencies which the rest of condor depends on.
		set(LIBXML2_FOUND "${EXTERNAL_STAGE}/lib/libxml2.a")

	else( NOT PROPER )

		find_multiple( "xml2" LIBXML2_FOUND )

	endif( NOT PROPER )

	message (STATUS "external configured (LIBXML2_FOUND=${LIBXML2_FOUND})")
	set( LIBXML2_FOUND ${LIBXML2_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_LIBXML2 ON PARENT_SCOPE )

else(WITH_LIBXML2)

	message (STATUS "external skiped (libxml2)")

endif(WITH_LIBXML2)