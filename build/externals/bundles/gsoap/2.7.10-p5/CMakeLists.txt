option(WITH_GSOAP "Compiling with support for GSOAP" ON)

if (WITH_GSOAP)

	if ( NOT PROPER )

		if (WINDOWS)
			set (GSOAP_PATCH cd gsoap-2.7.10 \r\n
							 gsoap-2.7.10cp ${CMAKE_CURRENT_SOURCE_DIR}/config.WINDOWS.h gsoap/src/config.h)
			
			set (GSOAP_CONFIGURE cd gsoap-2.7.10 \r\n
								 cp ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.win32 gsoap/src/Makefile.win32 )
			
			set (GSOAP_MAKE cd gsoap-2.7.10 \r\n
							set BISON_SIMPLE=${BISON_SIMPLE} \r\n
			                gmake -f Makefile.win32 -C gsoap/src )
			
			set (GSOAP_INSTALL cd gsoap-2.7.10 \r\n
							   set PACKAGE_INSTALL_DIR=${EXTERNAL_STAGE} \r\n 
			                   gmake -f Makefile.win32 install -C gsoap/src )
		else()
			set (GSOAP_PATCH  cd gsoap-2.7.10 && 
							  patch --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/install_soapcpp2_wsdl2h_aux-2.7.10.patch &&
							  patch --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/fix_xlocale_insanity.patch )
			set (GSOAP_CONFIGURE  cd gsoap-2.7.10 && 
								  ./configure --prefix=${EXTERNAL_STAGE} --disable-dependency-tracking CPPFLAGS=-I${EXTERNAL_STAGE}/include/)
			set (GSOAP_MAKE cd gsoap-2.7.10 && make )
			set (GSOAP_INSTALL cd gsoap-2.7.10 && make install )
		endif()

		ExternalProject_Add( gsoap
					DEPENDS openssl
				    PREFIX gsoap-2.7.10
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/gsoap-2.7.10.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/gsoap-2.7.10.tar.gz
					#--Patch step ----------
					PATCH_COMMAND ${GSOAP_PATCH}
					#--Configure step ----------
				    CONFIGURE_COMMAND ${GSOAP_CONFIGURE}
				    #--Build Step ----------
				    BUILD_COMMAND ${GSOAP_MAKE}
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND ${GSOAP_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		if (NOT WINDOWS)
			set(GSOAP_FOUND "${EXTERNAL_STAGE}/lib/libgsoapssl++.a")
		else()
			set(GSOAP_SRC "${EXTERNAL_STAGE}/include/stdsoap2.cpp" PARENT_SCOPE )
		endif()

		set ( GSOAP_SHARE "${CMAKE_CURRENT_SOURCE_DIR}/gsoap-2.7.10/src/gsoap/gsoap" PARENT_SCOPE )
		set ( SOAPCPP2 "${EXTERNAL_STAGE}/bin/soapcpp2" PARENT_SCOPE )

	else( NOT PROPER )

		#find gsoap and supporting bit(s)
		find_multiple( "gsoapssl++" GSOAP_FOUND)
		find_program( SOAPCPP2
					  soapcpp2
		    	      HINTS /usr/bin )

		set ( SOAPCPP2 ${SOAPCPP2} PARENT_SCOPE )
		set ( GSOAP_SHARE "/usr/share/gsoap" PARENT_SCOPE )

	endif( NOT PROPER )

	message ( STATUS "external configured (GSOAP_FOUND=${GSOAP_FOUND})" )

	## Set all the config information	
	set( GSOAP_FOUND ${GSOAP_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_GSOAP ON PARENT_SCOPE )

else(WITH_GSOAP)

	message (STATUS "external skiped (gsoap)")

endif(WITH_GSOAP)