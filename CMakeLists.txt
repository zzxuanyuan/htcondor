##########################################################################
# Set project
project(CONDOR)
cmake_minimum_required(VERSION 2.8)

##########################################################################
#Setup package meta-data (used in packaging)
set(PACKAGE "condor")
set(VERSION "7.5.2")
set(PACKAGE_BUGREPORT "http://condor-wiki.cs.wisc.edu/index.cgi/tktnew")
set(PACKAGE_NAME ${PACKAGE} )
set(PACKAGE_VERSION ${VERSION} )
set(PACKAGE_STRING ${PACKAGE} )
string( TOUPPER "${CMAKE_SYSTEM_NAME}" OS_NAME )
string( TOUPPER "${CMAKE_SYSTEM_VERSION}" OS_VER )
string( TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" SYS_ARCH )

###########################################
# start cmake module inclusion
list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/build/cmake)
include (CommandTarget)
include (gSOAPGen)
include (FindMultiple)
include (ExternalProjectPatched)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckStructHasMember)
include (CheckTypeExists)
include (CheckLibraryExists)
include (CheckTypeSize)
include (PackageDate)
include (SystemSpecificInformations)
include (CondorPackageConfig) # setup packaging variables needed prior to install targets
include (CondorGlob)
include (CondorSetLinkLibs)
include (CondorExe)
include (CondorDaemon)
include (CondorStaticLib)
include (CondorConfigure)     # will initiate the configure

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
add_subdirectory(src/daemons/startd)
add_subdirectory(src/daemons/starter)
add_subdirectory(src/daemons/shadow)
add_subdirectory(src/daemons/credd)
add_subdirectory(src/daemons/eventd)
add_subdirectory(src/daemons/kbdd)
add_subdirectory(src/daemons/collector)
add_subdirectory(src/daemons/negotiator)
add_subdirectory(src/daemons/schedd)
add_subdirectory(src/daemons/dbmsd)
add_subdirectory(src/daemons/had)
add_subdirectory(src/daemons/transferd)
###########################################
## gahps...
add_subdirectory(src/gahps/amazon)
add_subdirectory(src/gahps/c)
add_subdirectory(src/gahps/cream)
add_subdirectory(src/gahps/gridmanager)
add_subdirectory(src/gahps/gt2)
add_subdirectory(src/gahps/gt4)
add_subdirectory(src/gahps/gt42)
add_subdirectory(src/gahps/nordugrid)
###########################################
## tools...
add_subdirectory(src/tools/birdwatcher)
add_subdirectory(src/tools/condor_tools)
add_subdirectory(src/tools/q)
#add_subdirectory(src/tools/dagman)
add_subdirectory(src/tools/mail)
add_subdirectory(src/tools/hdfs)
add_subdirectory(src/tools/power)
add_subdirectory(src/tools/prio)
add_subdirectory(src/tools/rm)
add_subdirectory(src/tools/shared_port)
add_subdirectory(src/tools/startd_factory)
add_subdirectory(src/tools/status)
add_subdirectory(src/tools/submit)
#add_subdirectory(src/tools/userlog)
add_subdirectory(src/tools/ft_hooks)
add_subdirectory(src/tools/job_router)
add_subdirectory(src/tools/lease_manager)
#add_subdirectory(src/tools/hawkeye_modules) #??
###########################################
## other targets mostly specific to installation then anything else.
add_subdirectory(build/backstage)

##########################################################################

##########################################################################
# Must in included post *install targets
include ( CPack )
#include ( CondorDebGenerator )
















