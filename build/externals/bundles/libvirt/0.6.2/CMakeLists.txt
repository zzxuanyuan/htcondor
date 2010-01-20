option(WITH_LIBVIRT "Compiling with support for LIBVIRT" ON)

if (WITH_LIBVIRT)

	if ( NOT PROPER )

		ExternalProject_Add(libvirt
					DEPENDS libxml2
				    PREFIX libvirt-0.6.2
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget http://parrot.cs.wisc.edu/externals/libvirt-0.6.2.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/libvirt-0.6.2.tar.gz
					PATCH_COMMAND patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/no_gnutls.patch
					#--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${EXTERNAL_STAGE} --disable-shared --without-python --without-xen --without-libvirtd --without-remote --without-test --without-uml --without-openvz --without-avahi --without-selinux --without-sasl --with-libxml=${EXTERNAL_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND make install )

		# Set the target dependencies which the rest of condor depends on.
		set(LIBVIRT_FOUND "${EXTERNAL_STAGE}/lib/libvirt.a")

	else( NOT PROPER )

		find_multiple( "virt" LIBVIRT_FOUND )

	endif( NOT PROPER )

	message (STATUS "external configured (LIBVIRT_FOUND=${LIBVIRT_FOUND})")
	set( LIBVIRT_FOUND ${LIBVIRT_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_LIBVIRT ON PARENT_SCOPE )

else(WITH_LIBVIRT)

	message (STATUS "external skiped (libvirt)")

endif(WITH_LIBVIRT)