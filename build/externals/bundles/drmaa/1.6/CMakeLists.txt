
if ( NOT PROPER )

	option(WITH_DRMAA "Compiling with support for drmaa" ON)

	if (WITH_DRMAA)

		if (NOT WINDOWS)
			set (DRMAA_CONFIGURE ./configure --prefix=${EXTERNAL_STAGE})
			set (DRMAA_MAKE make)
			set (DRMAA_INSTALL cp libdrmaa.so ${EXTERNAL_STAGE}/lib/ \r\n 
							   cp drmaa.h ${EXTERNAL_STAGE}/include/ )

			set( DRMAA_FOUND "${EXTERNAL_STAGE}/lib/libdrmaa.so" )
		else()
			set (DRMAA_CONFIGURE echo "Nothing to configure for Windows")
			set (DRMAA_MAKE nmake /f libdrmaa.mak CFG="libdrmaa - Win32 Release")
			set (DRMAA_INSTALL  cp Release/* ${EXTERNAL_STAGE}/lib \r\n
			                    cp drmaa.h ${EXTERNAL_STAGE}/include)
			
			set( DRMAA_FOUND "${EXTERNAL_STAGE}/lib/libdrmaa.lib" )
		endif()

		ExternalProject_Add(drmaa
							PREFIX drmaa-1.6
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/drmaa-1.6.tar.gz
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/drmaa-1.6.tar.gz
							#--Configure step ----------
							CONFIGURE_COMMAND ${DRMAA_CONFIGURE}
							#--Build Step ----------
							BUILD_COMMAND ${DRMAA_MAKE}
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND ${DRMAA_INSTALL})

		## Set all the config information
		set( DRMAA_TARGET ${DRMAA_TARGET} PARENT_SCOPE )
		set( DRMAA_FOUND ${DRMAA_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_DRMAA ON )

		message (STATUS "external configured (drmaa)")

	endif (WITH_DRMAA)

endif( NOT PROPER )

if( NOT HAVE_EXT_DRMAA )
	message ( STATUS "external skipped (drmaa)" )
else( NOT HAVE_EXT_DRMAA )
	set( HAVE_EXT_DRMAA ON PARENT_SCOPE )
endif( NOT HAVE_EXT_DRMAA )




