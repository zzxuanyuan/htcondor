###########################################################################
###########################################################################
# Set project, reqs, and includes
project(CONDOR)
cmake_minimum_required(VERSION 2.6)

###########################################################################
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
###########################################################################
# Include options used for custom macros which we wish to create
list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/build/cmake)

###########################################################################
###########################################################################
# The following are the configure options
# checking X... Y.... Z...
message(STATUS "***********************************************************")
message(STATUS "Condor is now configuring based on your env & input options")
message(STATUS "Operating System: ${OS_NAME}(${OS_VER})")

include (CondorGlob)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckStructHasMember)
include (CheckTypeExists)
include (CheckLibraryExists)
include (CheckTypeSize)
include (FindThreads)

add_definitions(-D${OS_NAME}=${OS_NAME}_${OS_VER})
#what to do about glibc version checking...?
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
# Now checking input options --enable elements
# will likely change all the names to ENABLE_<OPTION> for consistency
option(ENABLE_checkSUM_SHA1 "Enable production and validation of SHA1 checksums." OFF)
option(ENABLE_checkSUM_MD5 "Enable production and validation of MD5 checksums for released packages." OFF)
option(HAVE_HIBERNATION "Support for condor controlled hibernation" OFF)
option(WANT_LEASE_MANAGER "Enable lease manager functionality" OFF)
option(WANT_STORK "Enable stork functionality" OFF)
option(HAVE_JOB_HOOKS "Enable job hook functionality" OFF)
option(HAVE_SSH_TO_JOB "Support for condor_ssh_to_job" OFF)
option(WANT_NEST "Enable NeST functionality" OFF)
option(WANT_QUILL "Enable Quill functionality" OFF)
option(NEEDS_KBDD "Enable KBDD functionality" OFF)
option(HAVE_BACKFILL "Compiling support for any backfill system" ON)
option(HAVE_BOINC "Compiling support for backfill with BOINC" ON)

option(PROPER "Use system env versus condor externals" OFF)

# Isn't it all clipped? for 7.5
#option(CLIPPED "Will disable the standard universe port" OFF)
#
	#if(CLIPPED)
		# disable other things...
	#else()
		# do version checking
	#endif()

# Oracle check
# VMware Check

##################################################
##################################################
# find the dependent packages.
# These should be the externals based on input options.

# Find required dependencies.
# find_package(Boost REQUIRED thread)
# find_package(PythonLibs REQUIRED)

##################################################
##################################################
# Now checking OS based options - may depend on
# user input options

set(HAS_FLOCK ON)

# this should be platform specific
set(STATFS_ARGS "2")

if (${OS_NAME} STREQUAL "SOLARIS")
	set(NEEDS_64BIT_SYSCALLS ON)
	set(NEEDS_64BIT_STRUCTS ON)
	set(DOES_SAVE_SIGSTATE ON)
	set(HAS_FLOCK OFF)
elseif(${OS_NAME} STREQUAL "LINUX")
	set(DOES_SAVE_SIGSTATE ON)
	set(HAS_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
	set(HAVE_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
	set(HAVE_PTHREAD_H ${CMAKE_HAVE_PTHREAD_H})

	# I don't know why this check is only done on linux
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
	
endif()

##################################################
##################################################
# check symbols, libs, functions, headers, and types

# check_library_exists("gen" "" "" HAVE_LIBGEN)
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

check_include_files("dlfcn.h" HAVE_DLFCN_H)
check_include_files("inttypes.h" HAVE_INTTYPES_H)
check_include_files("ldap.h" HAVE_LDAP_H)
check_include_files("net/if.h" HAVE_NET_IF_H)
check_include_files("openssl/ssl.h" HAVE_OPENSSL_SSL_H)
check_include_files("os_types.h" HAVE_OS_TYPES_H)
check_include_files("pcre.h" HAVE_PCRE_H)
check_include_files("pcre/pcre.h" HAVE_PCRE_PCRE_H)
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

check_type_size("id_t" HAVE_ID_T)
check_type_size("__int64" HAVE___INT64)
check_type_size("int64_t" HAVE_INT64_T)
check_type_size("long long" HAVE_LONG_LONG)

check_type_exists("struct ifconf" "net/if.h" HAVE_STRUCT_IFCONF)
check_type_exists("struct ifreq" "net/if.h" HAVE_STRUCT_IFREQ)

check_struct_has_member("struct statfs" f_fstyp "sys/statfs.h" HAVE_STRUCT_STATFS_F_FSTYP)
check_struct_has_member("struct statfs" f_fstypename "sys/statfs.h" HAVE_STRUCT_STATFS_F_FSTYPENAME)
check_struct_has_member("struct statfs" f_type "sys/statfs.h" HAVE_STRUCT_STATFS_F_TYPE)
check_struct_has_member("struct statvfs" f_basetype "sys/statfs.h" HAVE_STRUCT_STATVFS_F_BASETYPE)

message(STATUS "***********************************************************")
configure_file(${CONDOR_SOURCE_DIR}/include/config.h.cmake ${CONDOR_SOURCE_DIR}/include/config.h)
add_definitions(-DHAVE_CONFIG_H)

###########################################################################
###########################################################################
# Set compilation and build options

# Build dynamic libraries unless explicitly stated
set(BUILD_SHARED_LIBS FLASE)
# Supress automatic regeneration of project files
set(CMAKE_SUPPRESS_REGENERATION FALSE)

# Set build location for targets
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CONDOR_SOURCE_DIR}/staging/lib)

# Add executable's directory to its library search path I don't think condor uses .so's...
#set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
#set(CMAKE_INSTALL_RPATH "../lib")

# Set compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /wd4251 /wd4275 /wd4996")
	add_definitions(-DWINDOWS)
elseif(GNU)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Wall -W -Wextra -Wfloat-equal -Wshadow -Wendif-labels -Wpointer-arith -Wcast-qual -Wcast-align -Wvolatile-register-var -fstack-protector")
endif()

include_directories(${CONDOR_SOURCE_DIR}/include)
include_directories(${CONDOR_SOURCE_DIR}/libs/utils)

#########################################################################
# Add subdirectories and build targets in dependency order
add_subdirectory(libs/utils)
add_subdirectory(libs/cedar)
