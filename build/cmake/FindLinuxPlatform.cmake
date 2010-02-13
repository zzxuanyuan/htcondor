#Detect Redhat or Debian platform
# The following variable will be set
# $PLATFORM		Debian	Redhat		none
# $DIST_NAME		deb	rhel	fc	none

# $BIT_MODE		32|64


if ( NOT WINDOWS )	

	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES i386|i586|i686)
		set ( BIT_MODE "32")
	else ()
		set ( BIT_MODE "64")
	endif ()


	if(EXISTS "/etc/debian_version")
	   set ( PLATFORM "Debian")
	endif(EXISTS "/etc/debian_version")

	if(EXISTS "/etc/redhat-release")
	   set ( PLATFORM "Redhat")
	endif(EXISTS "/etc/redhat-release")

	execute_process(
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/cmake
		COMMAND perl FindLinuxPlatform.pl
		RESULT_VARIABLE RC
		OUTPUT_VARIABLE DIST_NAME
		ERROR_VARIABLE DIST_VER
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_STRIP_TRAILING_WHITESPACE
		)

	if (RC EQUAL 0)
		message (STATUS "Building for ${DIST_NAME} ${DIST_VER} ${BIT_MODE}-bit")
	else ()
		message (STATUS "Cannot detect platform")
		set (DIST_NAME "NA")
		set (DIST_VER ""NA)
	endif ()

endif ( NOT WINDOWS )	



