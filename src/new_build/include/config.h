/* config.h.cmake.  Generated from configure.ac by autoheader. -> then updated for cmake */

#ifndef __CONFIGURE_H_CMAKE__
#define __CONFIGURE_H_CMAKE__

//////////////////////////////////////////////////
/// TODO: OS VARS, may be able to ax with some smart mods
/// the definitions
/// I may be able to do away with all of this.
///* Define if on FreeBSD4 */
/* #undef CONDOR_FREEBSD4 */
///* Define if on FreeBSD5 */
/* #undef CONDOR_FREEBSD5 */
///* Define if on FreeBSD6 */
/* #undef CONDOR_FREEBSD6 */
///* Define if on FreeBSD7 */
/* #undef CONDOR_FREEBSD7 */
//* Define if on DUX */
/* #undef DUX */
///* Define if on DUX5 */
/* #undef DUX5 */
///* Define if on OS X 10.3 */
/* #undef Darwin_10_3 */
///* Define if on OS X 10.4 */
/* #undef Darwin_10_4 */
//* Define if on Solaris28 (USED)*/
/* #undef Solaris28 */
///* Define if on Solaris29 (USED)*/
/* #undef Solaris29 */
//////////////////////////////////////////////////

//////////////////////////////////////////////////
/// Options which may be changed if standard universe
/// goes away
///* Define if we can do checkpointing */
/* #undef DOES_CHECKPOINTING */
///* Define if we can compress checkpoints */
/* #undef DOES_COMPRESS_CKPT */
///* Define if we can do remote syscalls */
/* #undef DOES_REMOTE_SYSCALLS */
///* Used in condor_ckpt/image.C */
/* #undef HAS_DYNAMIC_USER_JOBS */
//////////////////////////////////////////////////

//////////////////////////////////////////////////
/// Used only for libcondorapi
///* does gcc support the -fPIC flag */
/* #undef HAVE_CC_PIC_FLAG */
///* does gcc support the -shared flag */
/* #undef HAVE_CC_SHARED_FLAG */
//////////////////////////////////////////////////

//////////////////////////////////////////////////
// These don't appear to be used at all
///* Define to 1 if the tool 'find' is available */
/* #undef HAVE_FIND */
///* Define to 1 if you have the `fseeko' function. */
/* #undef HAVE_FSEEKO */
///* Define to 1 if you have the `ftello' function. */
/* #undef HAVE_FTELLO */
///* Define to 1 if you have the `getdirentries' function. */
/* #undef HAVE_GETDIRENTRIES */
///* Define if jar is available */
/* #undef HAVE_JAR */
///* Define if javac is available */
/* #undef HAVE_JAVAC */
///* Define to 1 if you have the `crypt' library (-lcrypt). */
/* #undef HAVE_LIBCRYPT */
///* check for usable libsasl */
/* #undef HAVE_LIBSASL */
///* check for usable libsasl2 */
/* #undef HAVE_LIBSASL2 */
///* Define if md5sum is available */
/* #undef HAVE_MD5SUM */
///* Define to 1 if you have the <memory.h> header file. */
/* #undef HAVE_MEMORY_H */
///* Define to 1 if the tool 'objcopy' is available */
/* #undef HAVE_OBJCOPY */
///* Define to 1 if objcopy can seperate the debugging symbols from an executable */
/* #undef HAVE_OBJCOPY_DEBUGLINK */
///* Define if making rpms */
/* #undef HAVE_RPM */
///* Define if sha1sum is available */
/* #undef HAVE_SHA1SUM */
///* Define to 1 if you have the <stdlib.h> header file. */
/* #undef HAVE_STDLIB_H */
///* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */
///* Define to 1 if you have the <string.h> header file. */
/* #undef HAVE_STRING_H */
///* Define to 1 if `f_fsid' is member of `struct statvfs'. (USED)*/
/* #undef HAVE_STRUCT_STATVFS_F_FSID */
///* Define to 1 if you have the <sys/stat.h> header file. */
/* #undef HAVE_SYS_STAT_H */
///* Define to 1 is tar has --exclude option */
/* #undef HAVE_TAR_EXCLUDE_FLAG */
///* Define to 1 is tar has --files-from option */
/* #undef HAVE_TAR_FILES_FROM_FLAG */
///* Define to 1 if you have the `tmpnam' function. */
/* #undef HAVE_TMPNAM */
///* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */
///* Define to 1 if you have the `vsnprintf' function. */
/* #undef HAVE_VSNPRINTF */
///* Define to 1 if you have the ANSI C header files. */
/* #undef STDC_HEADERS */
///* Define if enabling HDFS */
/* #undef WANT_HDFS */
///* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */
///* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a `char[]'. */
/* #undef YYTEXT_POINTER */
//////////////////////////////////////////////////

/////////////////////////////////////////
// The following are configurable options
// previously --enable or --with...
/* Define if md5 checksums are required for released packages*/
/* #undef ENABLE_CHECKSUM_MD5 */

/* Define if sha1 checksums are required for released packages*/
/* #undef ENABLE_CHECKSUM_SHA1 */

/* Define if enabling lease manager (USED)*/
/* #undef WANT_LEASE_MANAGER */

/* Define if enabling NeST (USED)*/
/* #undef WANT_NEST */

/* Define if enabling Quill (USED)*/
/* #undef WANT_QUILL */

/* Define if enabling Stork (USED)*/
/* #undef WANT_STORK */

/* Define to 1 to support invoking hooks throughout the workflow of a job (USED)*/
/* #undef HAVE_JOB_HOOKS */

/* Define to 1 to support Condor-controlled hibernation (USED)*/
/* #undef HAVE_HIBERNATION */

/* Define to 1 to support condor_ssh_to_job (USED)*/
/* #undef HAVE_SSH_TO_JOB */

/* Define if doing a clipped build (USED)*/
#define CLIPPED 1

/* Define if enabling KBDD (USED)*/
/* #undef NEEDS_KBDD */
// configurable options.
/////////////////////////////////////////

/* Define if we save sigstate*/
#define DOES_SAVE_SIGSTATE 1

/* Define if HAS_FLOCK*/
#define HAS_FLOCK 1

/* Define if HAS_INET_NTOA*/
#define HAS_INET_NTOA 1

/* Define if pthreads are available for DRMAA */
#define HAS_PTHREADS 1

/* Define if pthreads are available (USED)*/
#define HAVE_PTHREADS 1

/* Define to 1 if you have the <pthread.h> header file. (USED)*/
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `access' function. */
#define HAVE_ACCESS 1

/* are we compiling support for any backfill systems (USED)*/
#define HAVE_BACKFILL 1

/* are we compiling support for backfill with BOINC (USED)*/
#define HAVE_BOINC 1

/* Define to 1 to use clone() for fast forking (USED)*/
#define HAVE_CLONE 1

///* Define to 1 of you know you can produce the debuglink tarball (USED by Imake, might eliminate)*/
/* #undef HAVE_DEBUGLINK_TARBALL */

/* Define to 1 if you have the declaration of `res_init', and to 0 if you don't.  (USED-daemoncore */
#define HAVE_DECL_RES_INIT 1

/* Define to 1 if you have the declaration of `SIOCETHTOOL', and to 0 if you don't. (USED)*/
#define HAVE_DECL_SIOCETHTOOL 1

/* Define to 1 if you have the declaration of `SIOCGIFCONF', and to 0 if you don't. (USED)*/
#define HAVE_DECL_SIOCGIFCONF 1

/* Define to 1 if you have the `dirfd' function. (USED)*/
#define HAVE_DIRFD 1

/* Define to 1 if you have the <dlfcn.h> header file. (USED)*/
#define HAVE_DLFCN_H 1

/* dlopen function is available  (used) */
#define HAVE_DLOPEN 1

/* Define to 1 if you have the `execl' function. (used)*/
#define HAVE_EXECL 1

/* Do we have the blahp external (used Imake)*/
#define HAVE_EXT_BLAHP 1
# 
/* Do we have the classads external (used)*/
#define HAVE_EXT_CLASSADS 1

/* Do we have the coredumper external (used)*/
#define HAVE_EXT_COREDUMPER 1

/* Do we have the gcb external (USED)*/
#define HAVE_EXT_GCB 1

/* Do we have the globus external (USED)*/
#define HAVE_EXT_GLOBUS 1

/* Do we have the gsoap external (USED)*/
#define HAVE_EXT_GSOAP 1

/* Do we have the krb5 external (USED)*/
#define HAVE_EXT_KRB5 1

/* Do we have the openssl external (USED)*/
#define HAVE_EXT_OPENSSL 1

/* Do we have the srb external (USED) Imake stork*/
#define HAVE_EXT_SRB 1

/* Do we have the unicoregahp external (USED)*/
#define HAVE_EXT_UNICOREGAHP 1

/* Do we have the voms external (USED)*/
#define HAVE_EXT_VOMS 1

//// The following are only internally used by imake
//// but it doesn't appear that they do ... anything
///* Do we have the cream external (Imake?)*/
/* #undef HAVE_EXT_CREAM */
///* Do we have the curl external (Imake)*/
/* #undef HAVE_EXT_CURL */
///* Do we have the drmaa external (Imake)*/
/* #undef HAVE_EXT_DRMAA */
///* Do we have the expat external (Imake)*/
//#define HAVE_EXT_EXPAT
///* Do we have the glibc external*/
/* #undef HAVE_EXT_GLIBC */
///* Do we have the hadoop external*/
//#define HAVE_EXT_HADOOP
///* Do we have the linuxlibcheaders external*/
/* #undef HAVE_EXT_LINUXLIBCHEADERS */
///* Do we have the man external*/
/* #undef HAVE_EXT_MAN */
///* Do we have the pcre external*/
//#define HAVE_EXT_PCRE
///* Do we have the postgresql external*/
//#define HAVE_EXT_POSTGRESQL
///* Do we have the zlib external */
//#define HAVE_EXT_ZLIB

/* Define to 1 if you have the `fstat64' function. (USED)*/
#define HAVE_FSTAT64 1

/* Define to 1 if you have the `getdtablesize' function. (USED)*/
#define HAVE_GETDTABLESIZE 1

/* Define to 1 if you have the `getpagesize' function. (USED)*/
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getwd' function. (USED)*/
#define HAVE_GETWD 1

///* are we using the GNU linker (USED)- I want to remove this comments are untrue*/
/* #undef HAVE_GNU_LD */

/* Define to 1 if the system has the type `id_t'. (USED)*/
#define HAVE_ID_T 1

/* Define to 1 if the system has the type `int64_t'. (USED)*/
#define HAVE_INT64_T 1

/* Define to 1 if you have the <inttypes.h> header file. (USED)*/
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `lchown' function. (USED)*/
#define HAVE_LCHOWN 1

/* Define to 1 if you have the <ldap.h> header file. (USED)*/
#define HAVE_LDAP_H 1

/* Define to 1 if you have the `gen' library (-lgen) (USED)*/
/* #undef HAVE_LIBGEN */

/* Define to 1 if you have the <linux/ethtool.h> header file.*/
#define HAVE_LINUX_ETHTOOL_H 1

/* Define to 1 if you have the <linux/magic.h> header file. (USED)*/
#define HAVE_LINUX_MAGIC_H 1

/* Define to 1 if you have the <linux/nfsd/const.h> header file. (USED)*/
#define HAVE_LINUX_NFSD_CONST_H 1

/* Define to 1 if you have the <linux/personality.h> header file. (USED)*/
#define HAVE_LINUX_PERSONALITY_H 1

/* Define to 1 if you have the <linux/sockios.h> header file. (USED)*/
#define HAVE_LINUX_SOCKIOS_H 1

/* Define to 1 if you have the <linux/types.h> header file. (USED)*/
#define HAVE_LINUX_TYPES_H 1

/* Define to 1 if the system has the type `long long'. (USED)*/
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the `lstat' function. (USED)*/
#define HAVE_LSTAT 1

/* Define to 1 if you have the `lstat64' function. (USED)*/
#define HAVE_LSTAT64 1

/* Define to 1 if you have the `mkstemp' function. (used)*/
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the <net/if.h> header file. (USED)*/
#define HAVE_NET_IF_H 1

/* Define to 1 if you have the <openssl/ssl.h> header file. (USED)*/
#define HAVE_OPENSSL_SSL_H 1

/* Do we have Oracle support (USED)*/
/* #undef HAVE_ORACLE */

/* Define to 1 if you have the <os_types.h> header file. (USED)*/
/* #undef HAVE_OS_TYPES_H */

/* Define to 1 if you have the <pcre.h> header file. (USED)*/
#define HAVE_PCRE_H 1

/* Define to 1 if you have the <pcre/pcre.h> header file. (USED)*/
/* #undef HAVE_PCRE_PCRE_H */

/* Define to 1 if you have the <resolv.h> header file. (USED)*/
#define HAVE_RESOLV_H 1

/* does os support the sched_setaffinity (USED)*/
/* #undef HAVE_SCHED_SETAFFINITY */

/* Define to 1 if you have the `setegid' function. (USED)*/
#define HAVE_SETEGID 1

/* Define to 1 if you have the `setenv' function. (USED)*/
#define HAVE_SETENV 1

/* Define to 1 if you have the `seteuid' function. (USED)*/
#define HAVE_SETEUID 1

/* Define to 1 if you have the `setlinebuf' function. (USED)*/
#define HAVE_SETLINEBUF 1

/* Define to 1 if you have the `snprintf' function. (USED)*/
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `stat64' function. (USED)*/
#define HAVE_STAT64 1

/* Define to 1 if you have the `statfs' function. (USED)*/
#define HAVE_STATFS 1

/* Define to 1 if you have the `statvfs' function. (USED)*/
#define HAVE_STATVFS 1

/* Define to 1 if you have the <stdint.h> header file. (USED)*/
#define HAVE_STDINT_H 1

/* Define to 1 if you have the `strcasestr' function. (USED)*/
#define HAVE_STRCASESTR 1

/* Define to 1 if you have the `strsignal' function. (USED)*/
#define HAVE_STRSIGNAL 1

/* Define to 1 if the system has the type `struct ifconf'. (USED) */
#define HAVE_STRUCT_IFCONF 1

/* Define to 1 if the system has the type `struct ifreq'. (USED)*/
#define HAVE_STRUCT_IFREQ 1

/* Define to 1 if `f_fstyp' is member of `struct statfs'. (USED)*/
/* #undef HAVE_STRUCT_STATFS_F_FSTYP */

/* Define to 1 if `f_fstypename' is member of `struct statfs'. (USED)*/
/* #undef HAVE_STRUCT_STATFS_F_FSTYPENAME */

/* Define to 1 if `f_type' is member of `struct statfs'. (USED)*/
#define HAVE_STRUCT_STATFS_F_TYPE 1

/* Define to 1 if `f_basetype' is member of `struct statvfs'. (USED)*/
/* #undef HAVE_STRUCT_STATVFS_F_BASETYPE */

/* Define to 1 if you have the <sys/mount.h> header file. (USED)*/
#define HAVE_SYS_MOUNT_H 1

/* Define to 1 if you have the <sys/param.h> header file. (USED)*/
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/personality.h> header file. (USED)*/
#define HAVE_SYS_PERSONALITY_H 1

/* Define to 1 if you have the <sys/statfs.h> header file. (USED)*/
#define HAVE_SYS_STATFS_H 1

/* Define to 1 if you have the <sys/statvfs.h> header file. (USED)*/
/* #undef HAVE_SYS_STATVFS_H */

/* Define to 1 if you have the <sys/types.h> header file. (USED)*/
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/vfs.h> header file. (USED)*/
#define HAVE_SYS_VFS_H 1

/* Define to 1 if you have the `unsetenv' function. (USED)*/
#define HAVE_UNSETENV 1

/* Define to 1 if you have the <ustat.h> header file. (USED)*/
#define HAVE_USTAT_H 1

/* Define to 1 if you have the <valgrind.h> header file. (USED)*/
/* #undef HAVE_VALGRIND_H */

/* Define to 1 if you have the `vasprintf' function. (USED)*/
#define HAVE_VASPRINTF 1

/* Define if vmware is available (USED)*/
/* #undef HAVE_VMWARE */

/* "use system (v)snprintf instead of our replacement" (USED)*/
#define HAVE_WORKING_SNPRINTF 1

/* Define to 1 if you have the `_fstati64' function. (USED)*/
/* #undef HAVE__FSTATI64 */

/* Define to 1 if you have the `_lstati64' function. (USED)*/
/* #undef HAVE__LSTATI64 */

/* Define to 1 if you have the `_stati64' function. (USED)*/
/* #undef HAVE__STATI64 */

/* Define to 1 if the system has the type `__int64'. (USED)*/
/* #undef HAVE___INT64 */

/* Define if NEEDS_64BIT_STRUCTS (USED)*/
/* #undef NEEDS_64BIT_STRUCTS */

/* Define if NEEDS_64BIT_SYSCALLS (USED)*/
/* #undef NEEDS_64BIT_SYSCALLS */

/* Define to the address where bug reports for this package should be sent. (USED)*/
#define PACKAGE_BUGREPORT 1

/* Define to the full name of this package. (USED)*/
#define PACKAGE_NAME 1

/* Define to the full name and version of this package. (USED)*/
#define PACKAGE_STRING 1

/* Define to the one symbol short name of this package. (USED)*/
/* #undef PACKAGE_TARNAME */

/* Define to the version of this package. (USED)*/
#define PACKAGE_VERSION 1

/* Number of arguments to statfs() (USED)*/
#define STATFS_ARGS 2

///* Define if on TRU64 (DUX) (USED)*/
/* #undef TRU64 */


#endif
