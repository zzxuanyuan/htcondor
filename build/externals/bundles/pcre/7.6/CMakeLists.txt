option(WITH_PCRE "Compiling with support for PCRE" ON)

if (WITH_PCRE)

	if ( NOT PROPER )

		if (WINDOWS)
			set (PCRE_PATCH cp ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.win32 . \r\n
							cp ${CMAKE_CURRENT_SOURCE_DIR}/pcre.h.win32 pcre.h \r\n
							cp ${CMAKE_CURRENT_SOURCE_DIR}/libpcre.def.win32 libpcre.def )
			set (PCRE_CONFIGURE cp ${CMAKE_CURRENT_SOURCE_DIR}/config.h.win32 config.h)
			set (PCRE_MAKE gmake -f Makefile.win32 )
			set (PCRE_INSTALL cp libpcre.lib ${EXTERNAL_STAGE}/lib \r\n
							  cp libpcre.dll ${EXTERNAL_STAGE}/lib \r\n
							  cp pcre.h ${EXTERNAL_STAGE}/include )
		else()
			set (PCRE_PATCH  patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/longlong.patch &&
							 patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/unittests.patch )
			set (PCRE_CONFIGURE ./configure --prefix=${EXTERNAL_STAGE} --disable-cpp )
			set (PCRE_MAKE make && make test )
			set (PCRE_INSTALL make install )
		endif()

		ExternalProject_Add(pcre
				    PREFIX pcre-7.6
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/pcre-7.6.tar.gz \r\n									
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/pcre-7.6.tar.gz
					#--Patch step ----------
					PATCH_COMMAND ${PCRE_PATCH}
				    #--Configure step ----------
				    CONFIGURE_COMMAND ${PCRE_CONFIGURE}
				    #--Build Step ----------
				    BUILD_COMMAND ${PCRE_MAKE}
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND ${PCRE_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		if (WINDOWS)
			set(PCRE_FOUND "${EXTERNAL_STAGE}/lib/libpcre.lib")
		else()
			set(PCRE_FOUND "${EXTERNAL_STAGE}/lib/libpcre.a")
		endif()

	else( NOT PROPER )

		find_multiple( "pcre;libpcre" PCRE_FOUND )

	endif( NOT PROPER )

	message (STATUS "external configured (PCRE_FOUND=${PCRE_FOUND})")
	set( PCRE_FOUND ${PCRE_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_PCRE ON PARENT_SCOPE )

else(WITH_PCRE)

	message (STATUS "external skiped (pcre)")

endif(WITH_PCRE)