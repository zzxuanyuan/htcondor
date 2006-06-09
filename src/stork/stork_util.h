/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
#ifndef __STORK_UTIL_H__
#define __STORK_UTIL_H__

#if 0
using namespace std;
#endif

#include "condor_common.h"
#include "condor_fix_access.h"
#include "condor_uid.h"

// New ClassAds.  Define WANT_NAMESPACES whenever both new and old ClassAds
// coexist in the same file.
#define WANT_NAMESPACES
#include "classad_distribution.h"

// Macros

// Typedefs

/**	access function, similar to POSIX access() system call, but also including:
	* access check is performed using process euid (not ruid)
	* R_OK and W_OK checks are performed using open(), which also checks AFS
	ACLS.  W_OK check will not leave a new zero length file behind.
	* W_OK access will check path directory, if path file does not exist.
	* R_OK, X_OK pathnames must refer to files, not directories.
	@param username The owner for euid check
	@param domain The domain for euid check.  Ignored for UNIX implementations.
	@pathname The subject of the access check.
	@mode The access mode to check, any _1_ of R_OK,  W_OK  or  X_OK, ala the
	access(2) system call.  F_OK is not supported.
	@return On sucess, zero is returned, else -1 is returned and errno is set
	appropriately
	**/
int
full_access(	const char username[],
				const char domain[],
				char *pathname,
				int mode);

/**	full_access() check but with [new] ClassAd interface, instead of username
	and domain.
	@ad must specifiy a new classad, containing a valid owner attribute string.
	@pathname The subject of the access check.
	@mode The access mode to check, any _1_ of R_OK,  W_OK  or  X_OK, ala the
	access(2) system call.  F_OK is not supported.
	@return On sucess, zero is returned, else -1 is returned and errno is set
	appropriately
	**/
int
full_access(	const classad::ClassAd& ad,
				char *pathname,
				int mode);

#endif//__STORK_UTIL_H__
