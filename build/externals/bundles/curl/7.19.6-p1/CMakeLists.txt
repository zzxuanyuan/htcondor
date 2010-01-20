
option(WITH_CURL "Compiling with support for CURL" ON)

if (WITH_CURL)

	if ( NOT PROPER )

		if (WINDOWS)
			set (CURL_CONFIGURE echo "Nothing to configure" )
			set (CURL_MAKE nmake -f Makefile.vc9 CFG=release-ssl )
			set (CURL_INSTALL cp -f lib\lib*.lib ${EXTERNAL_STAGE}\lib && cp -rf include\curl ${EXTERNAL_STAGE}\include )
		else()
			set (CURL_CONFIGURE PKG_CONFIG_PATH=${EXTERNAL_STAGE} LIBS=-ldl ./configure --prefix=${EXTERNAL_STAGE} --with-ssl=${EXTERNAL_STAGE} --disable-shared --disable-ipv6 )
			set (CURL_MAKE make)
			set (CURL_INSTALL make install)
		endif()

		ExternalProject_Add(curl
							DEPENDS openssl zlib
							PREFIX curl-7.19.6-p1
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/curl-7.19.6.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/curl-7.19.6.tar.gz
							#--Configure step ----------
							CONFIGURE_COMMAND ${CURL_CONFIGURE}
							#--Build Step ----------
							BUILD_COMMAND ${CURL_MAKE}
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND ${CURL_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		if (WINDOWS)
			set(CURL_FOUND "${EXTERNAL_STAGE}/lib/libcurl.lib")
		else()
			set(CURL_FOUND "${EXTERNAL_STAGE}/lib/libcurl.a")
		endif()

		message (STATUS "external configured (CURL_FOUND=${CURL_FOUND})")

	else( NOT PROPER )

		#find the following libraries.
		find_multiple( "curl" CURL_FOUND)

	endif( NOT PROPER )	

	## Set all the config information	
	set( CURL_FOUND ${CURL_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_CURL ON PARENT_SCOPE )

else(WITH_CURL)

	message (STATUS "external skiped (curl)")

endif(WITH_CURL)
