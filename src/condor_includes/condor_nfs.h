/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef _CONDOR_NFS_H
#define _CONDOR_NFS_H

#if defined(AIX)
#include <rpcsvc/mount.h>
#else
#include <sys/mount.h>
#endif

#if defined(LINUX)
#	include <linux/nfs.h>
#	include <linux/ipc.h>
#	include <dirent.h>
    typedef struct fhandle fhandle_t;

#elif !defined(IRIX)
#	include <rpc/rpc.h>

#if defined(Darwin) || defined(CONDOR_FREEBSD4) || defined(CONDOR_FREEBSD5) || defined(CONDOR_FREEBSD6) || defined(CONDOR_FREEBSD7)
#       include <nfs/rpcv2.h>
#endif

#if !defined(CONDOR_FREEBSD5) && !defined(CONDOR_FREEBSD6) && !defined(CONDOR_FREEBSD7)
#	include <nfs/nfs.h>
#else
	//
	// I am not sure what should go in here yet
	// Andy - 04.20.2006
	//
#	include <nfs/nfsproto.h>
#endif

#if defined(LINUX) || defined(IRIX) || defined(HPUX10) 
#	include <nfs/export.h>
#endif

#endif

#endif /* _CONDOR_NFS_H */

