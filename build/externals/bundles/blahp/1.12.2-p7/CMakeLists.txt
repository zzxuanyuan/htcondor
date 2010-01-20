option(WITH_BLAHP "Compiling with support for BLAHP" ON)

if ( NOT PROPER AND WITH_BLAHP )

	message(STATUS "TODO:TSTCLAIR BLAHP eval if linking correct popt w/o mundging Makefile.am")

	ExternalProject_Add(popt
						PREFIX popt-1.14
						#-- Download Step ----------
						DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/popt-1.14.tar.gz ${CMD_TERM}
						DOWNLOAD_DIR ${EXTERNAL_DL}
						URL http://parrot.cs.wisc.edu/externals/popt-1.14.tar.gz
						#--Configure step ----------
			    		CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE}
						#--Build Step ----------
						BUILD_COMMAND make
						BUILD_IN_SOURCE 1
						#--install Step ----------
						INSTALL_DIR ${EXTERNAL_STAGE}
						INSTALL_COMMAND make install )

	ExternalProject_Add(blahp
						DEPENDS classads openssl globus popt
					    PREFIX blahp-1.12.2
					    #-- Download Step ----------
						DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/blahp-1.12.2.tar.gz ${CMD_TERM}
					    DOWNLOAD_DIR ${EXTERNAL_DL}
						URL http://parrot.cs.wisc.edu/externals/blahp-1.12.2.tar.gz
						#--Patch step ----------
						PATCH_COMMAND patch -i ${CMAKE_CURRENT_SOURCE_DIR}/classad.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/mtrace.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/env_free.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/regbuf.patch -p0 && 
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/registry.patch -p0 && 
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/dl.patch -p0 &&
							cp ${CMAKE_CURRENT_SOURCE_DIR}/classads.m4 org.glite.ce.blahp/project &&
							cp ${CMAKE_CURRENT_SOURCE_DIR}/globus.m4 org.glite.ce.blahp/project &&
							cp ${CMAKE_CURRENT_SOURCE_DIR}/glite.m4 org.glite.ce.blahp/project.&&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/name-change.patch -p0 &&
							mv org.glite.ce.blahp/config/blah.config.template org.glite.ce.blahp/config/batch_gahp.config.template &&
							cd org.glite.ce.blahp && ./bootstrap
						#--Configure step ----------
						CONFIGURE_COMMAND ./configure --with-classads-prefix=${EXTERNAL_STAGE}
							--with-openssl-prefix=${EXTERNAL_STAGE}
							--with-globus-prefix=${EXTERNAL_STAGE}
							--with-globus-nothr-flavor=${GLOBUS_FLAVOR}
							--prefix=${EXTERNAL_STAGE}
						#--Build Step ----------
						BUILD_COMMAND make
						BUILD_IN_SOURCE 1
						#--install Step ----------
						INSTALL_DIR ${EXTERNAL_STAGE}
						INSTALL_COMMAND make install )

	message( STATUS "external configured (blahp)" )
	set( HAVE_EXT_BLAHP ON PARENT_SCOPE )

else( NOT PROPER AND WITH_BLAHP )

	message ( STATUS "external skiped (blahp)" )

endif( NOT PROPER AND WITH_BLAHP )