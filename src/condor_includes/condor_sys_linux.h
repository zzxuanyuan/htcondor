/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef CONDOR_SYS_LINUX_H
#define CONDOR_SYS_LINUX_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <sys/types.h>
typedef long rlim_t;
#define HAS_U_TYPES

#define idle _hide_idle
#if defined(GLIBC)
#	define truncate _hide_truncate
#	define ftruncate _hide_ftruncate
#	define profil _hide_profil
#	define daemon _hide_daemon
#endif /* GLIBC */
#include <unistd.h>
#undef idle
#if defined(GLIBC)
#	undef truncate
#	undef ftruncate
#	undef profil
#	undef daemon
BEGIN_C_DECLS
    int truncate( const char *, size_t );
    int ftruncate( int, size_t );
    int profil( char*, int, int, int );
END_C_DECLS
#endif /* GLIBC */

/* Want stdarg.h before stdio.h so we get GNU's va_list defined */
#include <stdarg.h>

/* <stdio.h> on glibc Linux defines a "dprintf()" function, which
   we've got hide since we've got our own. */
#if defined(GLIBC)
#	define dprintf _hide_dprintf
#endif
#include <stdio.h>
#if defined(GLIBC)
#	undef dprintf
#endif

#define SignalHandler _hide_SignalHandler
#include <signal.h>
#undef SignalHandler

/* Since param.h defines NBBY  w/o check, we want to include it here 
   before we check for and define it ourselves */ 
#include <sys/param.h>

/* There is no <sys/select.h> on Linux, select() and friends are 
   defined in <sys/time.h> */
#include "condor_fix_sys_time.h"

/* Need these to get statfs and friends defined */
#include <sys/stat.h>
#include <sys/vfs.h>

#include <sys/wait.h>
/* Some versions of Linux don't define WCOREFLG, but we need it */
#if !defined( WCOREFLG )
#	define WCOREFLG 0200
#endif 

#include "condor_fix_sys_resource.h"

#include <sys/uio.h>

#endif /* CONDOR_SYS_LINUX_H */

