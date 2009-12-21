##########################################################################
# Set project
project(CONDOR)
cmake_minimum_required(VERSION 2.6)

###########################################################################
#Setup package meta-data
set(PACKAGE "condor")
set(VERSION "7.5.0")
set(PACKAGE_TARNAME "")
set(PACKAGE_BUGREPORT "http://condor-wiki.cs.wisc.edu/index.cgi/tktnew")
set(PACKAGE_NAME "condor")
set(PACKAGE_VERSION "7.5.0")
set(PACKAGE_STRING "condor")
string( TOUPPER "${CMAKE_SYSTEM_NAME}" OS_NAME )
string( TOUPPER "${CMAKE_SYSTEM_VERSION}" OS_VER )
string( TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" SYS_ARCH )

###########################################################################
#Set build type
set(CMAKE_BUILD_TYPE RelWithDebInfo) # = -O2 -g 

###########################################
# start cmake module inclusion
list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/build/cmake)
include (CommandTarget)
include (gSOAPGen)
include (AddFileDependencies)
include (CondorGlob)
include (CondorSetLinkLibs)
include (CondorDaemon)
include (CondorExternal)
include (CondorExe)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckStructHasMember)
include (CheckTypeExists)
include (CheckLibraryExists)
include (CheckTypeSize)
include (CondorConfigure) # will initiate the configure

###########################################
# The following two locations can be considered ubiquitous
include_directories(${EXTERNAL_INCLUDES})
if ($ENV{JAVA_HOME})
	include_directories($ENV{JAVA_HOME}/include)
endif()
include_directories(${CONDOR_SOURCE_DIR}/src/include)
include_directories(${CONDOR_SOURCE_DIR}/src/libs/utils)
# the following will allow for <target>/.h includes
# and I've elliminated relative path includes.
include_directories(${CONDOR_SOURCE_DIR}/src/libs)
include_directories(${CONDOR_SOURCE_DIR}/src/daemons)
include_directories(${CONDOR_SOURCE_DIR}/src/libs/classad)
###########################################

##########################################################################
## Add subdirectories and build targets
###########################################
## libs...
add_subdirectory(src/libs/utils)
add_subdirectory(src/libs/ccb)
add_subdirectory(src/libs/cedar)
add_subdirectory(src/libs/procapi)
add_subdirectory(src/libs/sysapi)
add_subdirectory(src/libs/daemon_core)
add_subdirectory(src/libs/classad.old)
add_subdirectory(src/libs/privsep)
add_subdirectory(src/libs/h)
add_subdirectory(src/libs/daemon_client)
add_subdirectory(src/libs/classad_analysis)
add_subdirectory(src/libs/tt)
add_subdirectory(src/libs/chirp)
#add_subdirectory(libs/classad) new classads? - external
###########################################
## daemons...
add_subdirectory(src/daemons/procd)
add_subdirectory(src/daemons/master)
add_subdirectory(src/daemons/collector)
add_subdirectory(src/daemons/negotiator)
add_subdirectory(src/daemons/startd)
add_subdirectory(src/daemons/schedd)
add_subdirectory(src/daemons/shadow)
add_subdirectory(src/daemons/credd)
add_subdirectory(src/daemons/dbmsd)
add_subdirectory(src/daemons/eventd)
add_subdirectory(src/daemons/had)
add_subdirectory(src/daemons/kbdd)
add_subdirectory(src/daemons/transferd)
add_subdirectory(src/daemons/birdwatcher)
###########################################
## gahps...
add_subdirectory(src/gahps/amazon)
add_subdirectory(src/gahps/c)
add_subdirectory(src/gahps/gridmanager)
add_subdirectory(src/gahps/vm)
add_subdirectory(src/gahps/cream)
add_subdirectory(src/gahps/gt2)
add_subdirectory(src/gahps/gt4)
add_subdirectory(src/gahps/gt42)
add_subdirectory(src/gahps/nordugrid)
###########################################
## tools...
add_subdirectory(src/tools/condor_tools)
add_subdirectory(src/tools/q)
add_subdirectory(src/tools/dagman)
add_subdirectory(src/tools/hdfs)
add_subdirectory(src/tools/power)
add_subdirectory(src/tools/prio)
add_subdirectory(src/tools/rm)
add_subdirectory(src/tools/shared_port)
add_subdirectory(src/tools/startd_factory)
add_subdirectory(src/tools/status)
add_subdirectory(src/tools/submit)
add_subdirectory(src/tools/userlog)
add_subdirectory(src/tools/ft_hooks)
add_subdirectory(src/tools/job_router)
add_subdirectory(src/tools/lease_manager)
#add_subdirectory(src/tools/hawkeye_modules) #??
##########################################################################



# What should be done is:
# configure_file( CpackOptions.cmake.in CPackOption.cmake )
# include (CPackOptions)
#if(NOT WINDOWS)
	#set (CPACK_PACKAGE_NAME ${PACKAGE_NAME})
	#set (CPACK_PACKAGE_VENDOR "RedHat")
	#set (CPACK_PACKAGE_VERSION_MAJOR 7)
	#set (CPACK_PACKAGE_VERSION_MINOR 5)
	#set (CPACK_PACKAGE_VERSION_PATCH 0)
	#CPACK_PACKAGE_DESCRIPTION_FILE
	#set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "condor blah blah blah")
	#set (CPACK_PACKAGE_FILE_NAME "condor_7_5_0")
	#CPACK_RESOURCE_FILE_LICENSE
	#CPACK_RESOURCE_FILE_README
	#set (CPACK_STRIP_FILES TRUE)
	#set (CPACK_GENERATOR "RPM")

	#include (CPack)
#endif()













