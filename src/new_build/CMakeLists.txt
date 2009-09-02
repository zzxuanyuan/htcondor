###########################################################################
# Set project, reqs, and includes
project(CONDOR)
cmake_minimum_required(VERSION 2.6)

###########################################################################
# Setup package meta-data
set(PACKAGE "condor")
set(VERSION "7.X.X")
set(PACKAGE_TARNAME "")
set(PACKAGE_BUGREPORT " ")
set(PACKAGE_NAME "condor")
set(PACKAGE_VERSION "-")
set(PACKAGE_STRING "condor-")
set(PACKAGE_BUGREPORT "a suitable mailing list =>http://...")
string( TOUPPER "${CMAKE_SYSTEM_NAME}" OS_NAME )
string( TOUPPER "${CMAKE_SYSTEM_VERSION}" OS_VER )

###########################################################################
# Include options used for custom macros which we wish to create
# list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/tools/cmake)

###########################################################################
# The following are the configure options
# checking X... Y.... Z...
message(STATUS "##########################################################################")
message(STATUS "Condor is now configuring based on your env & input options")

include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckLibraryExists)
include (CheckTypeSize)
include (FindThreads)

##################################################
# Beginning with environment elements which are tacked on
message(STATUS "Operating System: ${OS_NAME}(${OS_VER})")
# I would like to make all the checks consistent.
add_definitions(-D${OS_NAME}=${OS_NAME}_${OS_VER})
# add_definitions(-D${OS_NAME}${OS_VER})
# add_definitions(-D${OS_NAME}_${OS_VER})

#checking glibc version stuff and tacking on.
add_definitions(-DGLIBC=GLIBC)
	#I'm ignoring this check add_definitions(-DGLIBC29=GLIBC29)
	#add_definitions(-DC_COMP_KIND_GCC)
	#add_definitions(-DC_COMP_MAJOR_GCC_4)
	#add_definitions(-DC_COMP_MINOR_GCC_4_3)
	#add_definitions(-DC_COMP_PATCH_GCC_4__2)

# Why is this a definition and not an option?
add_definitions(-DWITH_OPENSSL)
add_definitions(-DCOMPILE_SOAP_SSL)

##################################################
# find the dependent packages.
# Find required dependencies.
# find_package(Boost REQUIRED thread)
# find_package(PythonLibs REQUIRED)

##################################################
# Now checking OS based options

#set the defaults.  
option(DOES_SAVE_SIGSTATE "Defined if we save signal state" OFF)
option(HAS_FLOCK "Has Flocking Capability" ON)
option(HAVE_BACKFILL "Compiling support for any backfill system" ON)
option(HAVE_BOINC "Compiling support for backfill with BOINC" ON)

if (${OS_NAME} STREQUAL "SOLARIS")
	set(DOES_SAVE_SIGSTATE ON)
	set(HAS_FLOCK OFF)
elseif(${OS_NAME} STREQUAL "LINUX")
	set(DOES_SAVE_SIGSTATE ON)
	set(HAS_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
	set(HAVE_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
	set(HAVE_PTHREAD_H ${CMAKE_HAVE_PTHREAD_H})

	# I don't know why this check is only done on linux
	CHECK_SYMBOL_EXISTS(SIOCETHTOOL "linux/sockios.h" HAVE_DECL_SIOCETHTOOL)
	CHECK_SYMBOL_EXISTS(SIOCGIFCONF "linux/sockios.h" HAVE_DECL_SIOCGIFCONF)

	CHECK_INCLUDE_FILES("linux/ethtool.h" HAVE_LINUX_ETHTOOL_H)
	CHECK_INCLUDE_FILES("linux/magic.h" HAVE_LINUX_MAGIC_H)
	CHECK_INCLUDE_FILES("linux/nfsd/const.h" HAVE_LINUX_NFSD_CONST_H)
	CHECK_INCLUDE_FILES("linux/personality.h" HAVE_LINUX_PERSONALITY_H)
	CHECK_INCLUDE_FILES("linux/sockios.h" HAVE_LINUX_SOCKIOS_H)
	CHECK_INCLUDE_FILES("linux/types.h" HAVE_LINUX_TYPES_H)

elseif(${OS_NAME} STREQUAL "AIX")
	set(DOES_SAVE_SIGSTATE ON)
elseif(${OS_NAME} STREQUAL "HPUX")
	set(DOES_SAVE_SIGSTATE ON)
endif()

##################################################
# check libs, functions, headers, and types
CHECK_LIBRARY_EXISTS("gen" "" "" HAVE_LIBGEN)
CHECK_LIBRARY_EXISTS("sasl" "sasl_client_init" "" HAVE_LIBSASL)
CHECK_LIBRARY_EXISTS("sasl2" "sasl_client_init" "" HAVE_LIBSASL2)

CHECK_FUNCTION_EXISTS("access" HAVE_ACCESS)
CHECK_FUNCTION_EXISTS("clone" HAVE_CLONE)
CHECK_FUNCTION_EXISTS("dirfd" HAVE_DIRFD)
CHECK_FUNCTION_EXISTS("dlopen" HAVE_DLOPEN)
CHECK_FUNCTION_EXISTS("execl" HAVE_EXECL)
CHECK_FUNCTION_EXISTS("fstat64" HAVE_FSTAT64)
CHECK_FUNCTION_EXISTS("getdtablesize" HAVE_GETDTABLESIZE)
CHECK_FUNCTION_EXISTS("getpagesize" HAVE_GETPAGESIZE)
CHECK_FUNCTION_EXISTS("getwd" HAVE_GETWD)
CHECK_FUNCTION_EXISTS("inet_ntoa" HAS_INET_NTOA)
CHECK_FUNCTION_EXISTS("lchown" HAVE_LCHOWN)
CHECK_FUNCTION_EXISTS("lstat" HAVE_LSTAT)
CHECK_FUNCTION_EXISTS("lstat64" HAVE_LSTAT64)
CHECK_FUNCTION_EXISTS("mkstemp" HAVE_MKSTEMP)
CHECK_FUNCTION_EXISTS("setegid" HAVE_SETEGID)
CHECK_FUNCTION_EXISTS("setenv" HAVE_SETENV)
CHECK_FUNCTION_EXISTS("seteuid" HAVE_SETEUID)
CHECK_FUNCTION_EXISTS("setlinebuf" HAVE_SETLINEBUF)
CHECK_FUNCTION_EXISTS("snprintf" HAVE_SNPRINTF)
CHECK_FUNCTION_EXISTS("stat64" HAVE_STAT64)
CHECK_FUNCTION_EXISTS("statfs" HAVE_STATFS)
CHECK_FUNCTION_EXISTS("statvfs" HAVE_STATVFS)
CHECK_FUNCTION_EXISTS("res_init" HAVE_DECL_RES_INIT)
CHECK_FUNCTION_EXISTS("strcasestr" HAVE_STRCASESTR)
CHECK_FUNCTION_EXISTS("strsignal" HAVE_STRSIGNAL)

CHECK_INCLUDE_FILES("dlfcn.h" HAVE_DLFCN_H)
CHECK_INCLUDE_FILES("inttypes.h" HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES("ldap.h" HAVE_LDAP_H)
CHECK_INCLUDE_FILES("net/if.h" HAVE_NET_IF_H)
CHECK_INCLUDE_FILES("openssl/ssl.h" HAVE_OPENSSL_SSL_H)
CHECK_INCLUDE_FILES("os_types.h" HAVE_OS_TYPES_H)
CHECK_INCLUDE_FILES("pcre.h" HAVE_PCRE_H)
CHECK_INCLUDE_FILES("pcre/pcre.h" HAVE_PCRE_PCRE_H)
CHECK_INCLUDE_FILES("resolv.h" HAVE_RESOLV_H)
CHECK_INCLUDE_FILES("stdint.h" HAVE_STDINT_H)

CHECK_TYPE_SIZE("id_t" HAVE_ID_T)
CHECK_TYPE_SIZE("int64_t" HAVE_INT64_T)
CHECK_TYPE_SIZE("long long" HAVE_LONG_LONG)
CHECK_TYPE_SIZE("struct ifconf" HAVE_STRUCT_IFCONF)
CHECK_TYPE_SIZE("struct ifreq" HAVE_STRUCT_IFREQ)
CHECK_TYPE_SIZE("struct statfs" HAVE_STRUCT_STATFS_F_FSTYP)

##################################################
# Now checking input options --enable elements
# will likely change all the names to ENABLE_<OPTION> for consistency
option(ENABLE_CHECKSUM_SHA1 "Enable production and validation of SHA1 checksums." OFF)
option(ENABLE_CHECKSUM_MD5 "Enable production and validation of MD5 checksums for released packages." OFF)
option(HAVE_HIBERNATION "Support for condor controlled hibernation" OFF)
option(WANT_LEASE_MANAGER "Enable lease manager functionality" OFF)
option(WANT_STORK "Enable stork functionality" OFF)
option(HAVE_JOB_HOOKS "Enable job hook functionality" OFF)
option(HAVE_SSH_TO_JOB "Support for condor_ssh_to_job" OFF)
option(WANT_NEST "Enable NeST functionality" OFF)
option(WANT_QUILL "Enable Quill functionality" OFF)
option(NEEDS_KBDD "Enable KBDD functionality" OFF)

option(PROPER "Use system env versus condor externals" OFF)

option(CLIPPED "Will disable the standard universe port" OFF)
	#if(CLIPPED)
		# disable other things...
	#else()
		# do version checking
	#endif()

# Oracle support is odd?

##################################################
message(STATUS "##########################################################################")
CONFIGURE_FILE(${CONDOR_SOURCE_DIR}/include/config.h.cmake ${CONDOR_SOURCE_DIR}/include/config.h)
add_definitions(-DHAVE_CONFIG_H)

###########################################################################
# Build dynamic libraries unless explicitly stated
set(BUILD_SHARED_LIBS FLASE)
# Supress automatic regeneration of project files
set(CMAKE_SUPPRESS_REGENERATION FALSE)

# Set build location for targets
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CONDOR_SOURCE_DIR}/staging/lib)

# Add executable's directory to its library search path
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "../lib")

# Set compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /wd4251 /wd4275 /wd4996")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Wall -W -Wextra -Wfloat-equal -Wshadow -Wendif-labels -Wpointer-arith -Wcast-qual -Wcast-align -Wvolatile-register-var -fstack-protector")
endif()

include_directories(${CONDOR_SOURCE_DIR}/include)
include_directories(${CONDOR_SOURCE_DIR}/libs/utils)

# Add components in dependency order
add_subdirectory(libs/utils)
