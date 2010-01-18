option(WITH_POSTGRESQL "Compiling with support for POSTGRESQL" ON)

if (WITH_POSTGRESQL)

	if ( NOT PROPER )

		if (WINDOWS)
			set (POSTGRESQL_CONFIGURE echo "Nothing to patch on Windows")
			set (POSTGRESQL_MAKE cd src/interfaces/libpq \r\n
			                     nmake -f win32.mak )
			set (POSTGRESQL_INSTALL cp src/interfaces/libpq/Release/*.lib  ${EXTERNAL_STAGE}/lib \r\n
									cp src/interfaces/libpq/Release/*.dll ${EXTERNAL_STAGE}/lib \r\n
									cp src/interfaces/libpq/libpq-fe.h ${EXTERNAL_STAGE}/include \r\n
									cp -r src/include/* ${EXTERNAL_STAGE}/include )
									
									#mt.exe /manifest ${CMAKE_CURRENT_SOURCE_DIR}/libpq.dll.manifest /outputresource:${EXTERNAL_STAGE}/lib/libpq.dll)
									#imbedding custom manifests is a bad idea.  									
		else()
			set (POSTGRESQL_CONFIGURE ./configure --prefix=${EXTERNAL_STAGE} --without-readline --without-zlib)
			set (POSTGRESQL_MAKE make)
			set (POSTGRESQL_INSTALL make install)
		endif()

		ExternalProject_Add(postgresql
							PREFIX postgresql-8.0.2
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/postgresql-8.0.2.tar.gz \r\n
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/postgresql-8.0.2.tar.gz
							#--Configure step ----------
							CONFIGURE_COMMAND ${POSTGRESQL_CONFIGURE}
							#--Build Step ----------
							BUILD_COMMAND ${POSTGRESQL_MAKE}
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND ${POSTGRESQL_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		if (WINDOWS)
			set(POSTGRESQL_FOUND "${EXTERNAL_STAGE}/lib/libpqdll.lib")
		else()
			set(POSTGRESQL_FOUND "${EXTERNAL_STAGE}/lib/libpq.a")
		endif()

		message (STATUS "external configured (POSTGRESQL_FOUND=${POSTGRESQL_FOUND})")

	else( NOT PROPER )

		#find the following libraries.
		find_multiple( "pq;libpq" POSTGRESQL_FOUND)

	endif( NOT PROPER )

	## Set all the config information	
	set( POSTGRESQL_FOUND ${POSTGRESQL_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_POSTGRESQL ON PARENT_SCOPE )

else(WITH_POSTGRESQL)

	message (STATUS "external skiped (postgresql)")

endif(WITH_POSTGRESQL)