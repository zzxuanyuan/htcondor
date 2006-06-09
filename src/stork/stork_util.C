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

// Common stork utilities

#define ACCESS_SUCCESS		0
#define ACCESS_FAILURE		(-1)

#include "stork_util.h"
#include "condor_debug.h"
#include "stork_config.h"
#include "condor_uid.h"
#include "directory.h"
#include "basename.h"
#include "stork_job_ad.h"

using std::string;	// used _everywhere_ in new ClassAds API

int
full_access(	const char username[],
				const char domain[],
				char *pathname,
				int mode)
{
	int fd;
	priv_state initialPriv;		// initial privilege state
	bool unlink_write_path = false;		// write path does not initially exist
	int access_status;

	if (! param_boolean(STORK_CHECK_ACCESS_FILES,
				STORK_CHECK_ACCESS_FILES_DEFAULT)
	   )
	{
			return ACCESS_SUCCESS;
	}

	// stat() the file/directory first
	StatInfo pathStat(pathname);
	if ( pathStat.Error() != 0 ) {
		// error stat'ing the file occurred
		if ( mode==W_OK && pathStat.Errno()==ENOENT ) {
			// ok to check write access on a nonexistent file.  just remember
			// to unlink the empty file this access may create
			unlink_write_path = true;
			// TODO It would be useful to take this a step further and verify
			// the write access path directory exists.
		} else {
			dprintf(D_ALWAYS, "stat() %s error: %s\n",
					pathname, strerror(pathStat.Errno() ) );
			return ACCESS_FAILURE;
		}
	}
	if ( pathStat.IsDirectory() ) {
		dprintf(D_ALWAYS, "error: access path %s is a directory\n",
				pathname );
		return ACCESS_FAILURE;
	}

	// Prepare to switch to euid
	if (! init_user_ids(username, domain) ) {
		dprintf(D_ALWAYS, "error: unable to switch to user %s\n", username);
		return ACCESS_FAILURE;
	}


	// handle each access mode
	switch (mode) {

		// read access test
		case R_OK:
			initialPriv = set_user_priv();
			fd = open(pathname, O_RDONLY);
			if (fd >= 0) {
				close(fd);
			}
			set_priv( initialPriv );	// restore initial privilege state
			return (fd == -1) ? ACCESS_FAILURE : ACCESS_SUCCESS;

		// write access test
		case W_OK:
			initialPriv = set_user_priv();
			fd = open(pathname, O_WRONLY|O_CREAT|O_APPEND);
			if (fd >= 0) {
				close(fd);
				if ( unlink_write_path ) {
					// Don't leave empty write access test file behind.
					unlink(pathname);
				}
			}
			set_priv( initialPriv );	// restore initial privilege state
			return (fd == -1) ? ACCESS_FAILURE : ACCESS_SUCCESS;

		// execute access test
		case X_OK:
			initialPriv = set_user_priv();
			access_status = access_euid(pathname, X_OK);
			set_priv( initialPriv );	// restore initial privilege state
			return access_status;

		default:
			dprintf(D_ALWAYS, "error: unknown access mode: %d\n", mode);
			return ACCESS_FAILURE;
	}
}

int
full_access(	const classad::ClassAd& ad,
				char *pathname,
				int mode)
{
	string owner;
	if ( ! ad.EvaluateAttrString(STORK_JOB_ATTR_OWNER, owner)  ||
			owner.empty() )
	{
		dprintf(D_ALWAYS, "job ad %s attribute missing\n",
				STORK_JOB_ATTR_OWNER);
		return ACCESS_FAILURE;
	}

	// TODO test domain attribute for WIN32 platforms.
#ifdef WIN32
#error full_access() check for Windows not yet supported
#endif

	return full_access(owner.c_str(), NULL, pathname, mode);
}

