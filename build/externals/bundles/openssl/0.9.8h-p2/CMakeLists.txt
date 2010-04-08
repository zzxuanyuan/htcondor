option(WITH_OPENSSL "Compiling with support for OPENSSL" ON)

if (WITH_OPENSSL)

	if ( NOT PROPER )

		message (STATUS "TODO: TSTCLAIR check again if we support AIX , also I think shoving in specific manifests is a bad idea!")

		if (WINDOWS)
			set (OPENSSL_PATCH cd openssl-0.9.8h \r\n
							   patch --binary -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/renegotiate.patch \r\n
							   patch --binary -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/mk1mf.pl-0.9.8h-patch \r\n
							   patch --binary -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/VC-32.pl-0.9.8h-patch )

			set (OPENSSL_CONFIGURE cd openssl-0.9.8h \r\n
								   perl Configure VC-WIN32 )
			set (OPENSSL_MAKE  cd openssl-0.9.8h \r\n
							   call ms/do_nt.bat \r\n 
							   nmake -f ms/ntdll.mak )
			set (OPENSSL_INSTALL cd openssl-0.9.8h \r\n
								 cp out32dll/*.lib ${EXTERNAL_STAGE}/lib \r\n
								 cp out32dll/*.dll ${EXTERNAL_STAGE}/lib \r\n
								 cp -r inc32/* ${EXTERNAL_STAGE}/include \r\n
								 cp ${CMAKE_CURRENT_SOURCE_DIR}/ossl_typ-post.h ${EXTERNAL_STAGE}/include/openssl/ossl_typ.h )
		else()
			set (OPENSSL_PATCH cd openssl-0.9.8h && 
							   patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/renegotiate.patch &&
							   sed -i "s/-fomit-frame-pointer//" Configure )

			set (OPENSSL_CONFIGURE cd openssl-0.9.8h &&
								   ./config --prefix=${EXTERNAL_STAGE} no-asm -fPIC )
			set (OPENSSL_MAKE cd openssl-0.9.8h &&
						      make && make test )
			set (OPENSSL_INSTALL cd openssl-0.9.8h && make install )
		endif()

		ExternalProject_Add(openssl
				    PREFIX openssl-0.9.8h
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/openssl-0.9.8h.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/openssl-0.9.8h.tar.gz
					#--Patch step ----------
					PATCH_COMMAND ${OPENSSL_PATCH}
				    #--Configure step ----------
				    CONFIGURE_COMMAND ${OPENSSL_CONFIGURE}
				    #--Build Step ----------
				    BUILD_COMMAND ${OPENSSL_MAKE}
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND ${OPENSSL_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		if (WINDOWS)
			set(OPENSSL_FOUND "${EXTERNAL_STAGE}/lib/libeay32.lib;${EXTERNAL_STAGE}/lib/ssleay32.lib")

			#this may become the common case.
			install ( FILES ${EXTERNAL_STAGE}/lib/libeay32.dll
							${EXTERNAL_STAGE}/lib/ssleay32.dll
					  DESTINATION ${C_LIB}
		  			  #PERMISSIONS 
					)

		else(WINDOWS)
			set(OPENSSL_FOUND "${EXTERNAL_STAGE}/lib/libssl.a")
		endif(WINDOWS)

	else( NOT PROPER )

		#find the following libraries.
		find_multiple( "ssl;libeay32;ssleay32" OPENSSL_FOUND)

	endif( NOT PROPER )

	## Set all the config information
	message (STATUS "external configured (OPENSSL_FOUND=${OPENSSL_FOUND})")
	set( OPENSSL_FOUND ${OPENSSL_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_OPENSSL ON PARENT_SCOPE )

else(WITH_OPENSSL)

	message (STATUS "external skiped (openssl)")

endif(WITH_OPENSSL)