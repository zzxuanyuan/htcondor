
if ( NOT PROPER )

	option( WITH_VOMS "Compiling with support for VOMS" ON )

	if ( WITH_VOMS )

		ExternalProject_Add(voms
							DEPENDS expat openssl globus
						    PREFIX voms-1.8.8_2
						    #-- Download Step ----------
							DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/voms-1.8.8_2.tar.gz ${CMD_TERM}
						    DOWNLOAD_DIR ${EXTERNAL_DL}
						    URL http://parrot.cs.wisc.edu/externals/voms-1.8.8_2.tar.gz
							#--Patch step ----------
							PATCH_COMMAND patch -p0 < configure.patch &&
							 patch -p0 < includes.patch &&
							 patch -p0 < makefile.patch
					    	#--Configure step ----------
						    CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE} --with-api-only
							 --with-openssl-prefix=${EXTERNAL_STAGE}
							 --with-expat-prefix=${EXTERNAL_STAGE}
							 --with-globus-prefix=${EXTERNAL_STAGE}
							 --disable-java --disable-docs --disable-glite
						    #--Build Step ----------
						    BUILD_COMMAND make
						    BUILD_IN_SOURCE 1
						    #--install Step ----------
						    INSTALL_DIR ${EXTERNAL_STAGE}
						    INSTALL_COMMAND make install )

		message ( STATUS "external configured (voms)" )
		set( HAVE_EXT_VOMS ON )

	endif(WITH_VOMS)
endif(NOT PROPER)

if ( NOT HAVE_EXT_VOMS )
	message ( STATUS "external skiped (voms)" )
else()
	set( HAVE_EXT_VOMS ON PARENT_SCOPE )
endif( )