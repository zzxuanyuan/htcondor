option(WITH_QPID "Compiling with support for QPID" ON)

if (WITH_QPID)

	if ( NOT PROPER )

		set (QPID_MAJOR_VER 0.8)
		set (QPID_MINOR_VER RC2)

		ExternalProject_Add(qpid
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${QPID_VER}
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://people.apache.org/~robbie/qpid/${QPID_MAJOR_VER}/${QPID_MINOR_VER}/qpid-cpp-${QPID_MAJOR_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://people.apache.org/~robbie/qpid/qpid-cpp-${QPID_MAJOR_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd qpidc-${QPID_MAJOR_VER} && ./configure --prefix=${EXTERNAL_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND cd qpidc-${QPID_MAJOR_VER} && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd qpidc-${QPID_MAJOR_VER} && make install )

		# Set the target dependencies which the rest of condor depends on.
		set (QPID_INCLUDE_FOUND ${EXTERNAL_STAGE}/include)
		append_var(CONDOR_EXTERNALS qpid)

	else( NOT PROPER )

		find_multiple( "qpidclient;qpidtypes" QPID_FOUND )

	endif( NOT PROPER )

	message (STATUS "external configured (QPID_FOUND=${QPID_FOUND})")
	set( QPID_FOUND ${QPID_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_QPID ON PARENT_SCOPE )

else(WITH_QPID)

	message (STATUS "external skipped (qpid)")

endif(WITH_QPID)
