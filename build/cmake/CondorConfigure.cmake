# The following  cmake file is meant to include and set all the
# necessary parameters to build condor

message(STATUS "***********************************************************")
message(STATUS "System: ${OS_NAME}(${OS_VER}) Arch=${SYS_ARCH}")
message(STATUS "********* BEGINNING CONFIGURATION *********")

include (FindThreads)

if(${OS_NAME} MATCHES "WIN")
	set(WINDOWS ON)
	add_definitions(-DWINDOWS)
	# The following is necessary for ddk version to compile against.
	add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_WINXP) 
	add_definitions(-DWINVER=_WIN32_WINNT_WINXP)
	add_definitions(-DNTDDI_VERSION=NTDDI_WINXP)
endif()

# use vars set by FindThreads.
set(HAS_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
set(HAVE_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
set(HAVE_PTHREAD_H ${CMAKE_HAVE_PTHREAD_H})

add_definitions(-D${OS_NAME}=${OS_NAME}_${OS_VER})
add_definitions(-D${SYS_ARCH}=${SYS_ARCH} )

##################################################
##################################################
# check symbols, libs, functions, headers, and types
# check_library_exists("gen" "" "" HAVE_LIBGEN)
if (NOT WINDOWS)
	find_library(HAVE_X11 X11)
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

check_type_size("id_t" HAVE_ID_T)
check_type_size("__int64" HAVE___INT64)
check_type_size("int64_t" HAVE_INT64_T)
check_type_size("long long" HAVE_LONG_LONG)

##################################################
##################################################
# Now checking OS based options -
set(HAS_FLOCK ON)
set(DOES_SAVE_SIGSTATE OFF)

set(STATFS_ARGS "2") # this should be platform specific

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
endif()

##################################################
##################################################
# Now checking input options --enable elements
# will likely change all the names to ENABLE_<OPTION> for consistency
option(ENABLE_CHECKSUM_SHA1 "Enable production and validation of SHA1 checksums." OFF)
option(ENABLE_CHECKSUM_MD5 "Enable production and validation of MD5 checksums for released packages." ON)
option(HAVE_HIBERNATION "Support for condor controlled hibernation" ON)
option(WANT_LEASE_MANAGER "Enable lease manager functionality" ON)
option(HAVE_JOB_HOOKS "Enable job hook functionality" ON)
option(HAVE_SSH_TO_JOB "Support for condor_ssh_to_job" OFF)
option(NEEDS_KBDD "Enable KBDD functionality" ON)
option(HAVE_BACKFILL "Compiling support for any backfill system" ON)
option(HAVE_BOINC "Compiling support for backfill with BOINC" ON)

option(CLIPPED "Disables the standard universe" ON)
option(STRICT "If externals are not found it will error" OFF)

if (NOT WINDOWS)
	option(PROPER "If externals are not found it will error" ON)
endif()

if (PROPER)
	message(STATUS "********* Configuring externals using [local env] a.k.a. PROPER *********")
	find_path(HAVE_OPENSSL_SSL_H "openssl/ssl.h")
	find_path(HAVE_PCRE_H "pcre.h")
	find_path(HAVE_PCRE_PCRE_H "pcre/pcre.h" )
else()
	message(STATUS "********* Configuring externals using [uw-externals] a.k.a NONPROPER *********")
	set (HAVE_OPENSSL_SSL_H ON)
	set (HAVE_PCRE_H ON)

	set (EXTERNAL_STAGE ${CONDOR_SOURCE_DIR}/build/externals/stage/root)
	set (EXTERNAL_DL ${CONDOR_SOURCE_DIR}/build/externals/stage/download)
	include_directories( ${EXTERNAL_STAGE}/include )
	
	if (WINDOWS)
		include_directories( ${CONDOR_SOURCE_DIR}/src/libs/classad )
	endif()
	
	link_directories( ${EXTERNAL_STAGE}/lib ${EXTERNAL_STAGE}/lib64 )
endif()

###########################################
add_subdirectory(${CONDOR_SOURCE_DIR}/build/externals/bundles/drmaa/1.6)
add_subdirectory(${CONDOR_SOURCE_DIR}/build/externals/bundles/hadoop/0.20.0-p2)
add_subdirectory(${CONDOR_SOURCE_DIR}/build/externals/bundles/postgresql/8.0.2)
add_subdirectory(${CONDOR_SOURCE_DIR}/build/externals/bundles/openssl/0.9.8h-p2)
add_subdirectory(${CONDOR_SOURCE_DIR}/build/externals/bundles/krb5/1.4.3-p0)
add_subdirectory(${CONDOR_SOURCE_DIR}/build/externals/bundles/pcre/7.6)
add_subdirectory(${CONDOR_SOURCE_DIR}/build/externals/bundles/gsoap/2.7.10-p5)

set ( CONDOR_EXTERNALS drmaa postgresql openssl krb5 pcre gsoap )

if (NOT WINDOWS)

	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/zlib/1.2.3)
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/classads/1.0.4 )
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/curl/7.19.6-p1 )	
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/coredumper/0.2)
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/unicoregahp/1.2.0)	
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/expat/2.0.1)
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/gcb/1.5.6)
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/libxml2/2.7.3)
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/libvirt/0.6.2)

	# the following need verification on csl machines.. arg..
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/globus/4.2.1-p5)
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/blahp/1.12.2-p7)
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/voms/1.8.8_2-p1)
	add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/srb/3.2.1-p2)
	#add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/cream/1.10.1-p2)
	#add_subdirectory( ${CONDOR_SOURCE_DIR}/build/externals/bundles/man/current)
	
	set (CONDOR_EXTERNALS ${CONDOR_EXTERNALS} zlib blahp classads curl coredumper unicoregahp voms expat srb globus gcb libvirt libxml2 cream )

endif(NOT WINDOWS)

########################################################
message(STATUS "********* ENDING CONFIGURATION *********")
configure_file(${CONDOR_SOURCE_DIR}/src/include/config.h.cmake
${CONDOR_SOURCE_DIR}/src/include/config.h)
add_definitions(-DHAVE_CONFIG_H)

###########################################
## set global build properties
# Build dynamic libraries unless explicitly stated (it would be nice to change this
set(BUILD_SHARED_LIBS FLASE)

# Supress automatic regeneration of project files
if (WINDOWS)
	set(CMAKE_SUPPRESS_REGENERATION TRUE)
else()
	set(CMAKE_SUPPRESS_REGENERATION FALSE)
endif()

set (CONDOR_LIBS "procd_client;daemon_core;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils")
set (CONDOR_TOOL_LIBS "procd_client;daemon_client;procapi;cedar;privsep;classad.old;sysapi;ccb;utils")
###########################################
# Build flags
if (HAVE_EXT_OPENSSL)
	# this should be removed in preference to HAVE_EXT_OPENSSL
	add_definitions(-DWITH_OPENSSL)
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251 /wd4275 /wd4996 /wd4273")	
	
	set (CONDOR_WIN_LIBS "crypt32.lib;mpr.lib;psapi.lib;mswsock.lib;netapi32.lib;imagehlp.lib;ws2_32.lib;powrprof.lib;iphlpapi.lib;userenv.lib;Pdh.lib")
else()
	add_definitions(-DGLIBC=GLIBC)

    # common build flags 
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -W -Wextra -Wfloat-equal -Wshadow -Wendif-labels -Wpointer-arith -Wcast-qual -Wcast-align -Wvolatile-register-var -fstack-protector")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--warn-once -Wl,--warn-common")

endif()
###########################################

