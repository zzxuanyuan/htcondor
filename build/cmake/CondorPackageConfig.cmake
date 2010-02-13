
##################################################################
## Begin the CPACK variable on-slaught.
##
## Start with the common section.
##################################################################
set (PACKAGE_REVISION "1")
set (CPACK_PACKAGE_NAME ${PACKAGE_NAME})
set (CPACK_PACKAGE_VENDOR "Condor Team - University of Wisconsin Madison")
set (CPACK_PACKAGE_VERSION ${PACKAGE_VERSION})
set (CPACK_PACKAGE_CONTACT "condor-users@cs.wisc.edu") 
set (URL "http://www.cs.wisc.edu/condor/")
set (CONDOR_VER "${PACKAGE_NAME}-${PACKAGE_VERSION}")

set (CPACK_PACKAGE_DESCRIPTION "Condor is a specialized workload management system for
compute-intensive jobs. Like other full-featured batch systems,
Condor provides a job queueing mechanism, scheduling policy,
priority scheme, resource monitoring, and resource management.
Users submit their serial or parallel jobs to Condor, Condor places
them into a queue, chooses when and where to run the jobs based
upon a policy, carefully monitors their progress, and ultimately
informs the user upon completion.")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Condor: High Throughput Computing")

set(CPACK_RESOURCE_FILE_LICENSE "${CONDOR_SOURCE_DIR}/LICENSE-2.0.txt")
set(CPACK_RESOURCE_FILE_README "${CONDOR_SOURCE_DIR}/build/backstage/release_notes/README")
#set(CPACK_RESOURCE_FILE_WELCOME "${CONDOR_SOURCE_DIR}/build/backstage/release_notes/DOC") # this should be more of a Hiya welcome.

set(CPACK_SYSTEM_NAME "${OS_NAME}-${SYS_ARCH}" )
set(CPACK_TOPLEVEL_TAG "${OS_NAME}-${SYS_ARCH}" )
set(CPACK_PACKAGE_ARCHITECTURE ${SYS_ARCH} ) 

#set(CPACK_SOURCE_STRIP_FILES "") #ouput files to strip from source dump, if we do out of src builds we elim this need. (typically would be .svn dirs etc.)
set (CPACK_STRIP_FILES TRUE)

set (CPACK_PACKAGE_FILE_NAME "${CONDOR_VER}-${OS_NAME}-${SYS_ARCH}" )

##################################################################
## Now onto platform specific package generation
## http://www.itk.org/Wiki/CMake:CPackPackageGenerators
##################################################################

# 1st set the location of the install targets.
set( C_BIN		usr/bin )
set( C_LIB		usr/lib/condor )
set( C_LIBEXEC		usr/libexec/condor )
set( C_SBIN		usr/sbin )

set( C_INCLUDE		usr/include/condor )
set( C_MAN		usr/share/man )
set( C_SRC		usr/src)
set( C_SQL		usr/share/condor/sql)

set( C_INIT		etc/init.d )
set( C_ETC		etc/condor )

set( C_ETC_EXAMPLES	usr/share/doc/${CONDOR_VER}/etc/examples )
set( C_DOC		usr/share/doc/${CONDOR_VER} )

set( C_LOCAL_DIR	var/lib/condor )
set( C_LOG_DIR		var/log/condor )
set( C_LOCK_DIR		var/lock/condor )
set( C_RUN_DIR		var/run/condor )

# NOTE: any RPATH should use these variables + PREFIX for location

if ( ${OS_NAME} MATCHES "WIN" )

	# override for windows.
	set( C_BIN bin )
	set( C_LIB bin )
	set( C_LIBEXEC bin )
	set( C_SBIN bin )
	set( C_ETC etc )

	set (CPACK_PACKAGE_INSTALL_DIRECTORY "${CONDOR_VER}")
	set (CPACK_PACKAGE_FILE_NAME "${CONDOR_VER}")
	set (CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${CONDOR_VER}")
	set( CPACK_PACKAGE_ICON ${CONDOR_SOURCE_DIR}/build/backstage/win/Bitmaps/dlgbmp.bmp) # A branding image that will be displayed inside the installer.

	set (CPACK_GENERATOR "WIX")  #;WIX
	set (CPACK_WIX_PRODUCT_GUID "ea9608e1-9a9d-4678-800c-645df677094a")
	set (CPACK_WIX_UPGRADE_GUID "ef96d7c4-29df-403c-8fab-662386a089a4")
	
	## You could do the configure idea to strip out information from this prior
	## to packaging.  
	configure_file(${CONDOR_SOURCE_DIR}/build/backstage/win/win.xsl.in ${CONDOR_SOURCE_DIR}/build/backstage/win/win.xsl @ONLY)
	set (CPACK_WIX_XSL ${CONDOR_SOURCE_DIR}/build/backstage/win/win.xsl)

	option(WIN_EXEC_NODE_ONLY "Minimal Package Win exec node only" OFF)

    #######################################################################
    ## The following variables are used with an NSIS generator.  
    ## if all works well w/wix we can likely eliminate all of this 
    ## 
	#set (CPACK_GENERATOR "NSIS;ZIP") 
	# ommit - set (CPACK_NSIS_DISPLAY_NAME "${CONDOR_VER}") # defaults to CPACK_PACKAGE_INSTALL_DIRECTORY
	#set (CPACK_NSIS_MUI_ICON) # install icon
	#set (CPACK_NSIS_MUI_UNIICON) #uninstall icon
	#set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS)	 #Extra NSIS commands that will be added to the install Section
	#set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS)   #Extra NSIS commands that will be added to the uninstall Section
	#set (CPACK_NSIS_COMPRESSOR) 				 #The arguments that will be passed to the NSIS SetCompressor command.
	#set (CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\MyExecutable.exe") #
	#set (CPACK_NSIS_HELP_LINK "http://www.cs.wisc.edu/condor")
	#set (CPACK_NSIS_URL_INFO_ABOUT "http://www.cs.wisc.edu/condor/description.html")
	#set (CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}" )
	#set (CPACK_NSIS_CREATE_ICONS_EXTRA) # additional commands for creating start menu shortcuts
	#set (CPACK_NSIS_DELETE_ICONS_EXTRA ) # additional commands to uninstall start menu shortcuts.
	#set (CPACK_NSIS_MENU_LINKS )

elseif( ${OS_NAME} STREQUAL "LINUX" )


	# it's a smaller subset easier to differentiate.
	# check the operating system name

	# Debian is not currently support so it is X out
	if ( ${PLATFORM} STREQUAL  "XDebian" )

		##############################################################
		# For details on DEB package generation see:
		# http://www.itk.org/Wiki/CMake:CPackPackageGenerators#DEB_.28UNIX_only.29
		##############################################################
		set ( CPACK_GENERATOR "DEB" )

		if(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
			if(${BIT_MODE} STREQUAL "32")
				set (DEB_ARCH i386)
			else ()
				set (DEB_ARCH amd64)
			endif()
		else ()
			set (DEB_ARCH ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE})
		endif()

		set (CPACK_PACKAGE_FILE_NAME "${PACKAGE_NAME}_${PACKAGE_VERSION}-${PACKAGE_REVISION}_${DEB_ARCH}" )
		string( TOLOWER "${CPACK_PACKAGE_FILE_NAME}" CPACK_PACKAGE_FILE_NAME )

		set ( CPACK_DEBIAN_PACKAGE_SECTION "contrib/misc")
		set ( CPACK_DEBIAN_PACKAGE_PRIORITY "extra")

		set ( CPACK_DEBIAN_PACKAGE_DESCRIPTION "workload management system")
		set ( CPACK_PACKAGE_DESCRIPTION_SUMMARY ${CPACK_PACKAGE_DESCRIPTION})

		#Control files
		set( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/postinst"
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/postrm"
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/preinst"
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/prerm")		
		
		PackageDate( DEB CPACK_DEB_DATE)

		set( CPACK_SET_DESTDIR "ON")
		set( CMAKE_INSTALL_PREFIX "/")

		#set (CPACK_DEBIAN_PACKAGE_DEPENDS)
		#if (PROPER)
		#	set (CPACK_DEBIAN_PACKAGE_DEPENDS)
		#endif()

		# set you deb specific variables

	elseif ( ${PLATFORM} STREQUAL  "Redhat" )
						
		##############################################################
		# For details on RPM package generation see:
		# http://www.itk.org/Wiki/CMake:CPackPackageGenerators#RPM_.28Unix_Only.29
		##############################################################
		
		set ( CPACK_GENERATOR "RPM" )
		#set ( CPACK_SOURCE_GENERATOR "RPM" )

		#Enable trace during packaging
		set(CPACK_RPM_PACKAGE_DEBUG 1)			

		set (CPACK_PACKAGE_FILE_NAME "${CONDOR_VER}-${PACKAGE_REVISION}-${DIST_NAME}${DIST_VER}-${SYS_ARCH}" )
		string( TOLOWER "${CPACK_PACKAGE_FILE_NAME}" CPACK_PACKAGE_FILE_NAME )

		set ( CPACK_RPM_PACKAGE_RELEASE ${PACKAGE_REVISION})
		set ( CPACK_RPM_PACKAGE_GROUP "Applications/System")		
		set ( CPACK_RPM_PACKAGE_LICENSE "Apache License, Version 2.0")
		set ( CPACK_RPM_PACKAGE_VENDOR ${CPACK_PACKAGE_VENDOR})
		set ( CPACK_RPM_PACKAGE_URL ${URL})
		set ( CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})

		PackageDate( RPM CPACK_RPM_DATE)		

		#Specify SPEC file template
		set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_SOURCE_DIR}/build/backstage/rpm/condor.spec.in")

		set( CPACK_SET_DESTDIR "ON")
		set( CMAKE_INSTALL_PREFIX "/")
		
		#Set lib dir according to architecture
		if (${BIT_MODE} STREQUAL "32")
			set( C_LIB		usr/lib/condor )
		elseif (${BIT_MODE} STREQUAL "64")
			set( C_LIB		usr/lib64/condor )
		endif()

	endif()

# this needs to be evaluated for correctness
elseif(${OS_NAME} STREQUAL "OSX") 

	# whatever .dmg is.. 
	set ( CPACK_GENERATOR "PackageMaker;STGZ" )
	#set (CPACK_OSX_PACKAGE_VERSION)

else()

	set ( CPACK_GENERATOR "STGZ" )

endif()

