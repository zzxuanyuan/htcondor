option(WITH_GCB "Compiling with support for GCB" ON)

if (NOT PROPER AND WITH_GCB)

	ExternalProject_Add(gcb
				    	PREFIX gcb-1.5.6
				    	#-- Download Step ----------
				    	DOWNLOAD_DIR ${EXTERNAL_DL}
				    	URL http://parrot.cs.wisc.edu/externals/gcb-1.5.6.tar.gz
			    		#--Configure step ----------
				    	CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE}
				    	#--Build Step ----------
				    	BUILD_COMMAND make
				    	BUILD_IN_SOURCE 1
				    	#--install Step ----------
				    	INSTALL_DIR ${EXTERNAL_STAGE}
				    	INSTALL_COMMAND make install )

	# Set the target dependencies which the rest of condor depends on.
	set(GCB_FOUND "${EXTERNAL_STAGE}/lib/libGCB.a")

	message (STATUS "external configured (GCB_FOUND=${GCB_FOUND})")
	set( GCB_FOUND ${GCB_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_GCB ON PARENT_SCOPE )

else()

	message (STATUS "external skiped (gcb)")

endif()