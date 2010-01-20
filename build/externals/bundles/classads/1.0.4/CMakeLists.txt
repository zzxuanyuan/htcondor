
option( WITH_CLASSADS "Compiling with support for CLASSADS" ON )

if (WITH_CLASSADS)

	if ( NOT PROPER )

		ExternalProject_Add(classads
							PREFIX classads-1.0.4
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/classads-1.0.4.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/classads-1.0.4.tar.gz
							#--Configure step ----------
							CONFIGURE_COMMAND ./configure --without-pcre --disable-shared --enable-static --enable-namespace --prefix=${EXTERNAL_STAGE} CC=g++
							#--Build Step ----------
							BUILD_COMMAND make check
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND make install)

		# Set the target dependencies which the rest of condor depends on.
		set(CLASSADS_FOUND "${EXTERNAL_STAGE}/lib/libclassad.a;${EXTERNAL_STAGE}/lib/libclassad_ns.a")

		message (STATUS "external configured (CLASSADS_FOUND=${CLASSADS_FOUND})")

	else( NOT PROPER )

		#find the following libraries.
		find_multiple( "classad;classad_ns" CLASSADS_FOUND)

	endif( NOT PROPER )

	## Set all the config information
	set( CLASSADS_FOUND ${CLASSADS_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_CLASSADS ON PARENT_SCOPE )

else(WITH_CLASSADS)

	message (STATUS "external skiped (classads)")

endif(WITH_CLASSADS)
