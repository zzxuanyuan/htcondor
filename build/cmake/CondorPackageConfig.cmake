
##################################################################
## Begin the CPACK variable on-slaught.
##
## Start with the common section.
##################################################################
set (CPACK_PACKAGE_NAME ${PACKAGE_NAME})
set (CPACK_PACKAGE_VENDOR "University of Wisconsin Madison")
set (CPACK_PACKAGE_VERSION ${PACKAGE_VERSION})

message(STATUS "TODO: Where do questions go?")
set (CPACK_PACKAGE_CONTACT "help@condor.cs.wisc.edu") 

set (CONDOR_VER "${PACKAGE_NAME}-${PACKAGE_VERSION}")

set (CPACK_INSTALL_CMAKE_PROJECTS ";ALL/")

set (CPACK_PACKAGE_DESCRIPTION "Condor is a specialized workload management system for
            				    compute-intensive jobs. Like other full-featured batch systems,
           					    Condor provides a job queueing mechanism, scheduling policy,
           					    priority scheme, resource monitoring, and resource management.
           					    Users submit their serial or parallel jobs to Condor, Condor places
           					    them into a queue, chooses when and where to run the jobs based
           					    upon a policy, carefully monitors their progress, and ultimately
           					    informs the user upon completion.")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Condor: High Throughput Computing")

set(CPACK_RESOURCE_FILE_LICENSE "${CONDOR_SOURCE_DIR}/build/release_notes/LICENSE-2.0.txt")
set(CPACK_RESOURCE_FILE_README "${CONDOR_SOURCE_DIR}/build/release_notes/README")
set(CPACK_RESOURCE_FILE_WELCOME "${CONDOR_SOURCE_DIR}/build/release_notes/DOC") # this should be more of a Hiya welcome.

set(CPACK_SYSTEM_NAME "${OS_NAME}-${SYS_ARCH}" )
set(CPACK_TOPLEVEL_TAG "${OS_NAME}-${SYS_ARCH}" )

set(CPACK_PACKAGE_ARCHITECTURE ${SYS_ARCH} ) # This may need to be evaluated. i686, x86_64...

#set(CPACK_SOURCE_STRIP_FILES "") #ouput files to strip from source dump.
set (CPACK_STRIP_FILES TRUE)

#e.g. condor-X.-Linux-
set (CPACK_PACKAGE_FILE_NAME "${CONDOR_VER}-${OS_NAME}-${SYS_ARCH}" )

# set( CPACK_PACKAGE_ICON ) # .bmp which is used across various installers.

##################################################################
## Now onto platform specific package generation
## http://www.itk.org/Wiki/CMake:CPackPackageGenerators
##################################################################

# 1st set the location of the install targets.
set( C_BIN /usr/bin )
set( C_LIB /usr/lib/condor )
set( C_LIBEXEC /usr/libexec/condor )
set( C_SBIN /usr/sbin )

if ( WINDOWS )

	# override for windows.
	set( C_BIN bin )
	set( C_LIB bin )
	set( C_LIBEXEC bin )
	set( C_SBIN bin )	

	set (CPACK_PACKAGE_INSTALL_DIRECTORY "${CONDOR_VER}")
	set (CPACK_PACKAGE_FILE_NAME "${CONDOR_VER}")
	set (CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${CONDOR_VER}")

	set (CPACK_GENERATOR "NSIS;ZIP") # Generate an nsis installer + a zip archive.

	set (CPACK_NSIS_DISPLAY_NAME "${CONDOR_VER}")
	#set (CPACK_NSIS_MUI_ICON) # install icon
	#set (CPACK_NSIS_MUI_UNIICON) #uninstall icon
	#set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS)	 #Extra NSIS commands that will be added to the install Section
	#set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS)   #Extra NSIS commands that will be added to the uninstall Section
	#set (CPACK_NSIS_COMPRESSOR) 				 #The arguments that will be passed to the NSIS SetCompressor command.
	#set (CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\MyExecutable.exe") #
	#set (CPACK_NSIS_HELP_LINK "http://www.my-project-home-page.org"
	#set (CPACK_NSIS_URL_INFO_ABOUT "http://www.my-personal-home-page.com")
	set ( CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}" )
	#set (CPACK_NSIS_CREATE_ICONS_EXTRA) # additional commands for creating start menu shortcuts
	#set (CPACK_NSIS_DELETE_ICONS_EXTRA ) # additional commands to uninstall start menu shortcuts.
	#set (CPACK_NSIS_MENU_LINKS ) 

	## Include all NSIS variables.. 

elseif( ${OS_NAME} STREQUAL "LINUX" )

	# it's a smaller subset easier to differentiate.
	# check the operating system name
	#if (debian || ubuntu)

		##############################################################
		# For details on DEB package generation see:
		# http://www.itk.org/Wiki/CMake:CPackPackageGenerators#DEB_.28UNIX_only.29
		##############################################################
		#set ( CPACK_GENERATOR "DEB" )

		#set (CPACK_DEBIAN_PACKAGE_DEPENDS)
		#set (CPACK_DEBIAN_PACKAGE_SECTION ) # defaults to devel
		#set (CPACK_DEBIAN_PACKAGE_PRIORITY ) #defaults to optional
		#if (PROPER)
		#	set (CPACK_DEBIAN_PACKAGE_DEPENDS)
		#endif()

		# set you deb specific variables

	#else()

		#set ( CPACK_GENERATOR "RPM" )
		#set ( CPACK_SOURCE_GENERATOR "RPM" )

		##############################################################
		# For details on RPM package generation see:
		# http://www.itk.org/Wiki/CMake:CPackPackageGenerators#RPM_.28Unix_Only.29
		##############################################################

		#set ( CPACK_RPM_PACKAGE_RELEASE ) #defaults to 1 this is the version of the RPM file
		#set ( CPACK_RPM_PACKAGE_GROUP ) #defaults to none
		#set ( CPACK_RPM_PACKAGE_LICENSE )
		set (CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})

		#set(CPACK_RPM_PACKAGE_DEBUG ) # this can be used to debug the package generation process

		##############################################################
		## Use the following if you have your own spec file
		##############################################################

		#set(CPACK_RPM_USER_BINARY_SPECFILE)
		#set(CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE)

		##############################################################
		## Use the following if you wish to generate a spec file.
		##############################################################

		# The following can be used in place of spec file for auto gen'd spec
		#set (CPACK_RPM_PACKAGE_REQUIRES)
		#if (PROPER)
		#	set (CPACK_RPM_PACKAGE_REQUIRES)
		#endif()
		#set(CPACK_RPM_SPEC_INSTALL_POST)
		#set(CPACK_RPM_SPEC_MORE_DEFINE)
		#set(CPACK_RPM_<POST/PRE>_<UN>INSTALL_SCRIPT_FILE)

	#endif()

# this needs to be evaluated for correctness
elseif(${OS_NAME} STREQUAL "OSX") 

	set ( CPACK_GENERATOR "PackageMaker;STGZ" )
	#set (CPACK_OSX_PACKAGE_VERSION)

else()

	set ( CPACK_GENERATOR "STGZ" )

endif()

