/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "env.h"
#include "user_proc.h"
#include "starter_proc.h"
#include "starter.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_distribution.h"

#ifdef WIN32
#include "perm.h"
#endif

extern CStarter *Starter;

/* StarterProc class implementation */

StarterProc::StarterProc( ClassAd *jobAd ) : OsProc( jobAd )
{
    dprintf( D_FULLDEBUG, "In StarterProc::StarterProc()\n" );
}


StarterProc::~StarterProc()
{
}


int
StarterProc::StartJob()
{
	dprintf( D_FULLDEBUG, "in StarterProc::StartJob()\n" );

	if( !JobAd ) {
		dprintf( D_ALWAYS, "No JobAd in StarterProc::StartJob()!\n" );
		return 0;
	}

	// determine command line
	char tmp[_POSIX_ARG_MAX];
	char command[_POSIX_ARG_MAX];
	char args[_POSIX_ARG_MAX];
	JobAd->LookupString("StarterCmd", command);
	dprintf(D_FULLDEBUG, "%s\n", command);
	JobAd->LookupString("StarterArgs", tmp);
	dprintf(D_FULLDEBUG, "%s\n", tmp);
	sprintf(args, "%s %s", command, tmp);

	dprintf ( D_FULLDEBUG, "starter command: %s\n", command);
	dprintf ( D_FULLDEBUG, "starter arguments: %s \n", args);

	// construct the inherit list (shadow socket)
	Stream **socks = daemonCore->GetInheritedSocks();
	if (socks[0] == NULL || 
		socks[1] != NULL || 
		socks[0]->type() != Stream::reli_sock) 
	{
		dprintf(D_ALWAYS, "Failed to inherit remote system call socket.\n");
		DC_Exit(1);
	}

	// see if we wish to suspend at exec
	bool suspend_at_exec = false;
	JobAd->LookupBool( ATTR_SUSPEND_JOB_AT_EXEC, suspend_at_exec );
	int job_opt = suspend_at_exec ? DCJOBOPT_SUSPEND_ON_EXEC : 0;

	if ( suspend_at_exec )
		dprintf( D_FULLDEBUG, "job will start suspended\n");

	dprintf(D_FULLDEBUG, "About to call Create_Process\n");
	JobPid = daemonCore->
		Create_Process( command, args, PRIV_ROOT, 1, TRUE,
				NULL, NULL, TRUE, socks, NULL, 0, job_opt );

	// Create_Process saves "interesting" errors
	char const *create_process_error = NULL;
	if(JobPid == FALSE && errno) create_process_error = strerror(errno);
	if ( JobPid == FALSE ) {
		JobPid = -1;
		
		if ( create_process_error ) {
			MyString err_msg = "Failed to execute '";
			err_msg += command;
			err_msg += ' ';
			err_msg += args;
			err_msg += "': ";
			err_msg += create_process_error;
			Starter->jic->notifyStarterError( err_msg.Value(), true );
		}

		EXCEPT("Create_Process for nested starter");
		return 0;
	}

	return TRUE;
}
