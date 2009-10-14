###########################################################################
###########################################################################
# Set project
project(CONDOR)
cmake_minimum_required(VERSION 2.6)

###########################################################################
###########################################################################
# Setup package meta-data
set(PACKAGE "condor")
set(VERSION "7.5.0")
set(PACKAGE_TARNAME "")
set(PACKAGE_BUGREPORT " ")
set(PACKAGE_NAME "condor")
set(PACKAGE_VERSION "-")
set(PACKAGE_STRING "condor-")
set(PACKAGE_BUGREPORT "a suitable mailing list =>http://...")
string( TOUPPER "${CMAKE_SYSTEM_NAME}" OS_NAME )
string( TOUPPER "${CMAKE_SYSTEM_VERSION}" OS_VER )
string( TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" SYS_ARCH )
set(CMAKE_BUILD_TYPE RelWithDebInfo)

###########################################################################
###########################################################################
# Include options used for custom macros which we wish to create
list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/build/cmake)

###########################################################################
###########################################################################
# The following are the configure options
# checking X... Y.... Z...
message(STATUS "***********************************************************")
message(STATUS "Condor is now configuring based on your env & input options")
message(STATUS "System: ${OS_NAME}(${OS_VER}) Arch=${SYS_ARCH}")

if(${OS_NAME} STREQUAL "WINDOWS")
	set(WINDOWS ON)
	add_definitions(-DWINDOWS)	
endif()

include (gSOAPSrcs)
include (gSOAPGen)
include (CondorGlob)
include (CondorExternal)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckStructHasMember)
include (CheckTypeExists)
include (CheckLibraryExists)
include (CheckTypeSize)
include (FindThreads)
#include (CPack)

# use vars set by FindThreads.
set(HAS_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
set(HAVE_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
set(HAVE_PTHREAD_H ${CMAKE_HAVE_PTHREAD_H})

add_definitions(-D${OS_NAME}=${OS_NAME}_${OS_VER})
add_definitions(-D${SYS_ARCH}=${SYS_ARCH} )

########################################################
## Begin RANT
##
## So I've been thinking about how condor handles externals
## for a while now, and I must admit it's fairly odd. 
## If it is a requirement to have a package it should be 
## and treated as a pre-requisite.
##
## Step 1: Obtain condor
## Step 2: Obtain externals
## Step 3: Build Externals - script builds all & stages
## Step 4: Build Condor
##
## End RANT
########################################################

SET(UW_EXTERNALS_DIR "${CONDOR_SOURCE_DIR}/build/externals/stage" CACHE STRING "Location of UW staged externals")
message(STATUS "env: UW_EXTERNALS_DIR = ${UW_EXTERNALS_DIR}")
    
link_directories(${UW_EXTERNALS_DIR}/lib)
set(CMAKE_LIBRARY_PATH "${UW_EXTERNALS_DIR}/lib")
    
include_directories(${UW_EXTERNALS_DIR} ${UW_EXTERNALS_DIR}/include)
set(CMAKE_INCLUDE_PATH "${UW_EXTERNALS_DIR}/include")

##################################################
##################################################
# check symbols, libs, functions, headers, and types
# check_library_exists("gen" "" "" HAVE_LIBGEN)
if (NOT WINDOWS)
	check_library_exists(dl dlopen "" HAVE_DLOPEN)
	check_symbol_exists(res_init "sys/types.h;netinet/in.h;arpa/nameser.h;resolv.h" HAVE_DECL_RES_INIT)

	check_function_exists("access" HAVE_ACCESS)
	check_function_exists("clone" HAVE_CLONE)
	check_function_exists("dirfd" HAVE_DIRFD)
	check_function_exists("execl" HAVE_EXECL)
	check_function_exists("fstat64" HAVE_FSTAT64)
	check_function_exists("_fstati64" HAVE__FSTATI64)
	check_function_exists("getdtablesize" HAVE_GETDTABLESIZE)
	check_function_exists("getpagesize" HAVE_GETPAGESIZE)
	check_function_exists("getwd" HAVE_GETWD)
	check_function_exists("inet_ntoa" HAS_INET_NTOA)
	check_function_exists("lchown" HAVE_LCHOWN)
	check_function_exists("lstat" HAVE_LSTAT)
	check_function_exists("lstat64" HAVE_LSTAT64)
	check_function_exists("_lstati64" HAVE__LSTATI64)
	check_function_exists("mkstemp" HAVE_MKSTEMP)
	check_function_exists("setegid" HAVE_SETEGID)
	check_function_exists("setenv" HAVE_SETENV)
	check_function_exists("seteuid" HAVE_SETEUID)
	check_function_exists("setlinebuf" HAVE_SETLINEBUF)
	check_function_exists("snprintf" HAVE_SNPRINTF)
	check_function_exists("snprintf" HAVE_WORKING_SNPRINTF)	

	check_function_exists("stat64" HAVE_STAT64)
	check_function_exists("_stati64" HAVE__STATI64)
	check_function_exists("statfs" HAVE_STATFS)
	check_function_exists("statvfs" HAVE_STATVFS)
	check_function_exists("res_init" HAVE_DECL_RES_INIT)
	check_function_exists("strcasestr" HAVE_STRCASESTR)
	check_function_exists("strsignal" HAVE_STRSIGNAL)
	check_function_exists("unsetenv" HAVE_UNSETENV)
	check_function_exists("vasprintf" HAVE_VASPRINTF)
	
	# we can likely put many of the checks below in here.
	check_include_files("dlfcn.h" HAVE_DLFCN_H)
	check_include_files("inttypes.h" HAVE_INTTYPES_H)
	check_include_files("ldap.h" HAVE_LDAP_H)
	check_include_files("net/if.h" HAVE_NET_IF_H)
	check_include_files("os_types.h" HAVE_OS_TYPES_H)
	check_include_files("resolv.h" HAVE_RESOLV_H)
	check_include_files("sys/mount.h" HAVE_SYS_MOUNT_H)
	check_include_files("sys/param.h" HAVE_SYS_PARAM_H)
	check_include_files("sys/personality.h" HAVE_SYS_PERSONALITY_H)
	check_include_files("sys/statfs.h" HAVE_SYS_STATFS_H)
	check_include_files("sys/types.h" HAVE_SYS_TYPES_H)
	check_include_files("sys/vfs.h" HAVE_SYS_VFS_H)
	check_include_files("stdint.h" HAVE_STDINT_H)
	check_include_files("ustat.h" HAVE_USTAT_H)
	check_include_files("valgrind.h" HAVE_VALGRIND_H)

	check_type_exists("struct ifconf" "net/if.h" HAVE_STRUCT_IFCONF)
	check_type_exists("struct ifreq" "net/if.h" HAVE_STRUCT_IFREQ)

	check_struct_has_member("struct statfs" f_fstyp "sys/statfs.h" HAVE_STRUCT_STATFS_F_FSTYP)
	check_struct_has_member("struct statfs" f_fstypename "sys/statfs.h" HAVE_STRUCT_STATFS_F_FSTYPENAME)
	check_struct_has_member("struct statfs" f_type "sys/statfs.h" HAVE_STRUCT_STATFS_F_TYPE)
	check_struct_has_member("struct statvfs" f_basetype "sys/statfs.h" HAVE_STRUCT_STATVFS_F_BASETYPE)
	
else()
	set (HAVE_SNPRINTF 1)
	set (HAVE_WORKING_SNPRINTF 1)
endif()

# parseled off b/c could path searches 
# will use updated CMAKE_INCLUDE_PATH
# that were updated by UW_EXTERNALS_DIR
find_path(HAVE_OPENSSL_SSL_H "openssl/ssl.h" )
find_path(HAVE_PCRE_H "pcre.h")
find_path(HAVE_PCRE_PCRE_H "pcre/pcre.h" )

check_type_size("id_t" HAVE_ID_T)
check_type_size("__int64" HAVE___INT64)
check_type_size("int64_t" HAVE_INT64_T)
check_type_size("long long" HAVE_LONG_LONG)

##################################################
##################################################
# Now checking OS based options -
set(HAS_FLOCK ON)
set(DOES_SAVE_SIGSTATE OFF)

# this should be platform specific
set(STATFS_ARGS "2") 

if (${OS_NAME} STREQUAL "SOLARIS")
	set(NEEDS_64BIT_SYSCALLS ON)
	set(NEEDS_64BIT_STRUCTS ON)
	set(DOES_SAVE_SIGSTATE ON)
	set(HAS_FLOCK OFF)
elseif(${OS_NAME} STREQUAL "LINUX")
	set(DOES_SAVE_SIGSTATE ON)

	check_symbol_exists(SIOCETHTOOL "linux/sockios.h" HAVE_DECL_SIOCETHTOOL)
	check_symbol_exists(SIOCGIFCONF "linux/sockios.h" HAVE_DECL_SIOCGIFCONF)

	check_include_files("linux/ethtool.h" HAVE_LINUX_ETHTOOL_H)
	check_include_files("linux/magic.h" HAVE_LINUX_MAGIC_H)
	check_include_files("linux/nfsd/const.h" HAVE_LINUX_NFSD_CONST_H)
	check_include_files("linux/personality.h" HAVE_LINUX_PERSONALITY_H)
	check_include_files("linux/sockios.h" HAVE_LINUX_SOCKIOS_H)
	check_include_files("linux/types.h" HAVE_LINUX_TYPES_H)
elseif(${OS_NAME} STREQUAL "AIX")
	set(DOES_SAVE_SIGSTATE ON)
	set(NEEDS_64BIT_STRUCTS ON)
elseif(${OS_NAME} STREQUAL "HPUX")
	set(DOES_SAVE_SIGSTATE ON)
	set(NEEDS_64BIT_STRUCTS ON)
elseif(${OS_NAME} STREQUAL "WINDOWS")	
	#TBD
endif()

##################################################
##################################################
# Now checking input options --enable elements
# will likely change all the names to ENABLE_<OPTION> for consistency
option(ENABLE_CHECKSUM_SHA1 "Enable production and validation of SHA1 checksums." OFF)
option(ENABLE_CHECKSUM_MD5 "Enable production and validation of MD5 checksums for released packages." ON)
option(HAVE_HIBERNATION "Support for condor controlled hibernation" ON)
option(WANT_LEASE_MANAGER "Enable lease manager functionality" OFF)
option(WANT_STORK "Enable stork functionality" OFF)
option(HAVE_JOB_HOOKS "Enable job hook functionality" OFF)
option(HAVE_SSH_TO_JOB "Support for condor_ssh_to_job" OFF)
option(WANT_NEST "Enable NeST functionality" OFF)
option(WANT_QUILL "Enable Quill functionality" ON)
option(NEEDS_KBDD "Enable KBDD functionality" OFF)
option(HAVE_BACKFILL "Compiling support for any backfill system" ON)
option(HAVE_BOINC "Compiling support for backfill with BOINC" ON)

### Hopefully this will *always* be true.
option(CLIPPED "Disables the standard universe" ON)
option(STRICT "If externals are not found it will error" OFF)

##################################################
##################################################
# the following are the --with options
# we will find the external package i

########################################################
## The following externals are used by both *nix and windows.
## TBD ?? - CURL? doesn't look like it has deps in the code...?
condor_external(DRMAA ON "drmaa;libdrmaa")
condor_external(HADOOP OFF "hadoop") # Figure out Java Goop
condor_external(OPENSSL ON "ssl;ssleay32") 
condor_external(POSTGRESQL ON "pq;libpq") 
condor_external(KRB5 ON "krb5;krb5_32") 
condor_external(PCRE ON "pcre;libpcre")

########################################################
if (NOT WINDOWS)
    condor_external(GSOAP ON "gsoap")
    condor_external(BLAHP OFF "blahp")
    condor_external(CLASSADS ON "classad")
    condor_external(COREDUMPER ON "coredumper")
    condor_external(UNICOREGAHP OFF "unicoregahp")
    condor_external(VOMS OFF "voms")
    condor_external(EXPAT OFF "expat")
    condor_external(SRB OFF "srb")
    condor_external(GLOBUS OFF "globus")
    condor_external(ZLIB ON "z")
    condor_external(GCB OFF "gcb")
    # chat w/matt why this is all weird.
    # condor_external(MAN "" ON)
    # Below are TBD...
    condor_external(VMWARE OFF "") 
    condor_external(OCI OFF "" ) 
else()
    # I'm confused by this one, we don't build/link a lib in windows?
	set (HAVE_EXT_GSOAP 1) 
	set (HAVE_SCHED_SETAFFINITY 1)
	set (WANT_CLASSAD_ANALYSIS 1)
	# how is this set but not built on windows?
	# set (HAVE_EXT_CLASSADS 1)
endif()
########################################################
message(STATUS "***********************************************************")
configure_file(${CONDOR_SOURCE_DIR}/include/config.h.cmake ${CONDOR_SOURCE_DIR}/include/config.h)
add_definitions(-DHAVE_CONFIG_H)

###########################################################################
###########################################################################
# Set compilation and build options

###########################################
## set global build properties
# Build dynamic libraries unless explicitly stated
set(BUILD_SHARED_LIBS FLASE)
# Supress automatic regeneration of project files
set(CMAKE_SUPPRESS_REGENERATION FALSE)

# Set build location for targets
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CONDOR_SOURCE_DIR}/release_dir/sbin)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CONDOR_SOURCE_DIR}/release_dir/lib)

# TBD: what to do about glibc version checking...?
# NIX ALL STD UNIVERSE.  
add_definitions(-DGLIBC=GLIBC)
#I'm ignoring this check add_definitions(-DGLIBC29=GLIBC29)
#add_definitions(-DC_COMP_KIND_GCC)
#add_definitions(-DC_COMP_MAJOR_GCC_4)
#add_definitions(-DC_COMP_MINOR_GCC_4_3)
#add_definitions(-DC_COMP_PATCH_GCC_4__2)
###########################################

###########################################
# set package specific build options based on deps
# I don't know "why this was done... but we 
# may be able to remove this.
if (WITH_GSOAP AND WITH_OPENSSL)
	# added the goop that the old builds did
	add_definitions(-DCOMPILE_SOAP_SSL)
endif()

if (WITH_OPENSSL)
    add_definitions(-DWITH_OPENSSL)
endif()
###########################################

###########################################
# Set compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251 /wd4275 /wd4996 /wd4273")	
	
	# These were carried over from set_vars.bat
	set(CONDOR_EXT_LINK_LIBS ${CONDOR_EXT_LINK_LIBS} crypt32.lib mpr.lib psapi.lib mswsock.lib netapi32.lib imagehelp.lib ws2_32.lib powrprof.lib iphlpapi.lib userenv.lib)
			
else()

	#set(CONDOR_EXT_LINK_LIBS ${CONDOR_EXT_LINK_LIBS} m)

	if (HAVE_DLOPEN)
		set(CONDOR_EXT_LINK_LIBS ${CONDOR_EXT_LINK_LIBS} dl)
	endif()

	if (HAVE_DECL_RES_INIT)
		set(CONDOR_EXT_LINK_LIBS ${CONDOR_EXT_LINK_LIBS} resolv)
	endif()

	if (${CMAKE_USE_PTHREADS_INIT})
		set(CONDOR_EXT_LINK_LIBS ${CONDOR_EXT_LINK_LIBS} pthread)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
	endif()

    # common build flags 
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -W -Wextra -Wfloat-equal -Wshadow -Wendif-labels -Wpointer-arith -Wcast-qual -Wcast-align -Wvolatile-register-var -fstack-protector")

endif()
###########################################

###########################################
# The following two libraries can be considered ubiquitous
include_directories(${CONDOR_SOURCE_DIR}/include)
include_directories(${CONDOR_SOURCE_DIR}/libs/utils)
# the following will allow for <target>/.h includes
# and I've elliminated relative path includes.
include_directories(${CONDOR_SOURCE_DIR}/libs)
include_directories(${CONDOR_SOURCE_DIR}/daemons)
###########################################

###########################################################################
###########################################################################
## Add subdirectories and build targets 

## libs...
add_subdirectory(libs/utils)
add_subdirectory(libs/ccb)
add_subdirectory(libs/cedar)
add_subdirectory(libs/procapi)
add_subdirectory(libs/sysapi)
add_subdirectory(libs/daemon_core)
add_subdirectory(libs/classad.old)
add_subdirectory(libs/privsep)
#add_subdirectory(libs/classad) ..? .V6 wtf?

# the following libraries are *not* built in windows
if(NOT WINDOWS)
	add_subdirectory(libs/daemon_client)
endif()

## daemons... 
add_subdirectory(daemons/procd)
add_subdirectory(daemons/shadow)

###########################################################################

# update the install targets to proper location
# test packaging issues. 

#install (TARGETS x y z 
#         RUNTIME DESTINATION bin
#         LIBRARY DESTINATION lib)

#install (TARGETS x y z 
#         RUNTIME DESTINATION sbin
#         LIBRARY DESTINATION lib)
         



