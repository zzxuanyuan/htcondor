
if (NOT PROPER)

	option(WITH_GLOBUS "Compiling with support for GLOBUS" ON)

	if (WITH_GLOBUS)
		
		message(STATUS "TODO:TSTCLAIR GLOBUS Builds require review... Need to test the Darwin scenario as well.")

		ExternalProject_Add(gt4
							PREFIX gt4
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/globus-4.0.8-java.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/globus-4.0.8-java.tar.gz
							#--Configure step ----------
							CONFIGURE_COMMAND echo "Nothing to do"
							#--Build Step ----------
							BUILD_COMMAND echo "Nothing to do"
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND cp -r . ${EXTERNAL_STAGE} )

		ExternalProject_Add(gt42
							PREFIX gt42
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/globus-4.2.1-java.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/globus-4.2.1-java.tar.gz
							#--Configure step ----------
							CONFIGURE_COMMAND echo "Nothing to do"
							#--Build Step ----------
							BUILD_COMMAND echo "Nothing to do"
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND cp -r . ${EXTERNAL_STAGE} )


		set (GLOBUS_PATCH  patch -i ${CMAKE_CURRENT_SOURCE_DIR}/xio-plugins.patch -p0 &&
						${CMAKE_CURRENT_SOURCE_DIR}/directory_update.sh &&
						cd source-trees/gram/protocol/source &&
						patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/gram-reregister.patch &&
						cd ../../../gsi/gssapi/source &&
						patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/gsi-leak.patch )

		set( GLOBUS_FLAVOR gcc32dbg )

		ExternalProject_Add(globus
							DEPENDS openssl gt4 gt42
							PREFIX globus-4.2.1
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/globus-4.2.1.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/globus-4.2.1.tar.gz
							#--Patch step ----------
							PATCH_COMMAND ${GLOBUS_PATCH}
							#--Configure step ----------
							CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE} --with-flavor=${GLOBUS_FLAVOR}
							#--Build Step ----------
							BUILD_COMMAND GLOBUS_LOCATION=${EXTERNAL_STAGE} make gpt gssapi_error globus-resource-management-sdk globus_gridftp_server
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND make install )

		## Set all the config information	
		message( STATUS "external configured (globus)" )
		set( HAVE_EXT_GLOBUS ON PARENT_SCOPE )
		set( GLOBUS_FLAVOR ${GLOBUS_FLAVOR} PARENT_SCOPE )

	endif(WITH_GLOBUS)
endif(NOT PROPER)

if (NOT HAVE_EXT_GLOBUS)
	message (STATUS "external skiped (globus)")
endif()
