option(WITH_SRB "Compiling with support for SRB" OFF)

if ( NOT PROPER AND WITH_SRB )

	ExternalProject_Add(srb
						DEPENDS openssl globus
			    		PREFIX srb-3.2.1
			   			 #-- Download Step ----------
			    		DOWNLOAD_DIR ${EXTERNAL_DL}
			    		URL http://parrot.cs.wisc.edu/externals/srb-3.2.1.tar.gz
						#--Patch step ----------
						#--Configure step ----------
						CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE}
						--enable-gsi-auth
						--enable-globus-location=${EXTERNAL_STAGE}
						--enable-installdir=${EXTERNAL_STAGE}
						--enable-secure-comm
						--enable-openssl-location=${EXTERNAL_STAGE}
						#--Build Step ----------
						BUILD_COMMAND make
						BUILD_IN_SOURCE 1
						#--install Step ----------
						INSTALL_DIR ${EXTERNAL_STAGE}
						INSTALL_COMMAND make install )

	message (STATUS "external configured (srb)")
	set( SRB_FOUND ${SRB_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_SRB ON PARENT_SCOPE )

else()

	message (STATUS "external skiped (srb)")

endif()