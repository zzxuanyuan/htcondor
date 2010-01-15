option(WITH_UNICOREGAHP "Compiling with support for UNICOREGAHP" ON)

if ( WITH_UNICOREGAHP AND NOT PROPER )
	ExternalProject_Add(unicoregahp
			    PREFIX unicoregahp-1.2.0
			    #-- Download Step ----------
			    DOWNLOAD_DIR ${EXTERNAL_DL}
			    URL http://parrot.cs.wisc.edu/externals/unicoregahp-1.2.0.tar.gz
				#--Configure step ----------
				CONFIGURE_COMMAND cd .
				#--Build Step ----------
				BUILD_COMMAND cd .
				BUILD_IN_SOURCE 1
				#--install Step ----------
			    INSTALL_DIR ${EXTERNAL_STAGE}/usr/share/java
			    INSTALL_COMMAND cp ugahp.jar ${EXTERNAL_STAGE}/usr/share/java )

	set( HAVE_EXT_UNICOREGAHP ON PARENT_SCOPE )

else()
	message (STATUS "external skiped (unicoregahp)")
endif()