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

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "user_proc.h"
#include "tool_daemon_proc.h"
#include "starter.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#ifdef WIN32
#include "perm.h"
#endif

extern CStarter *Starter;

/* ToolDaemonProc class implementation */

ToolDaemonProc::ToolDaemonProc( ClassAd *jobAd )
{
    dprintf( D_FULLDEBUG, "In ToolDaemonProc::ToolDaemonProc()\n" );
    JobAd = jobAd;
    JobPid = Cluster = Proc = -1;
    job_suspended = false;
}


ToolDaemonProc::~ToolDaemonProc()
{
    if( JobAd ) {
        delete JobAd;
    }
}


int
ToolDaemonProc::StartJob()
{
    int i;
    int nice_inc = 0;

    dprintf( D_FULLDEBUG, "in ToolDaemonProc::StartJob()\n" );

    if( !JobAd ) {
        dprintf( D_ALWAYS, "No JobAd in ToolDaemonProc::StartJob()!\n" );
		return 0;
    }

    if( JobAd->LookupInteger(ATTR_CLUSTER_ID, Cluster) != 1 ) {
        dprintf( D_ALWAYS, "%s not found in JobAd.  "
				 "Aborting StartJob.\n",  ATTR_CLUSTER_ID );
		return 0;
    }

	if (JobAd->LookupInteger(ATTR_PROC_ID, Proc) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n", 
				ATTR_PROC_ID);
		return 0;
	}

	char* daemon_name = NULL;
	if( JobAd->LookupString( ATTR_TOOL_DAEMON_CMD, &daemon_name ) != 1 ) {
	    dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting StartJob. \n", 
				 ATTR_TOOL_DAEMON_CMD );
	    return 0;
	}

		// This is something of an ugly hack.  filetransfer doesn't
		// preserve file permissions when it moves a file.  so, our
		// tool "binary" (or script, whatever it is), is sitting in
		// the starter's directory without an execute bit set.  So,
		// we've got to call chmod() so that exec() doesn't fail.
	priv_state old_priv = set_user_priv();
	int retval = chmod( daemon_name, S_IRWXU | S_IRWXO | S_IRWXG );
	set_priv( old_priv );
	if( retval < 0 ) {
		dprintf( D_ALWAYS, "Failed to chmod %s!\n", daemon_name );
		free( daemon_name );
		return 0;
	}

	MyString daemon_args;
	char* tmp = NULL;

	if( JobAd->LookupString(ATTR_TOOL_DAEMON_ARGS, &tmp) != 1 ) {
	    dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting "
				 "ToolDaemonProc::StartJob()\n",
				 ATTR_TOOL_DAEMON_ARGS );
		free( daemon_name );
	    return 0;
	}
		// for now, simply prepend the daemon name to the args - this
		// becomes argv[0].
	daemon_args += daemon_name;
	daemon_args += " ";
	daemon_args += tmp;
	free( tmp );

	char* cwd = NULL;
	if (JobAd->LookupString(ATTR_JOB_IWD, &cwd) != 1) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  "
				 "Aborting ToolDaemonProc::StartJob()\n", ATTR_JOB_IWD );
		return 0;
	}

		// handle stdin, stdout, and stderr redirection for Daemon
	int daemon_fds[3];
	daemon_fds[0] = -1; daemon_fds[1] = -1; daemon_fds[2] = -1;
	char* filename = NULL;
	char daemon_infile[_POSIX_PATH_MAX];
	char daemon_outfile[_POSIX_PATH_MAX];
	char daemon_errfile[_POSIX_PATH_MAX];

		// in order to open these files we must have the user's privs:

	priv_state priv;
	priv = set_user_priv();

	if( JobAd->LookupString(ATTR_TOOL_DAEMON_INPUT, &filename ) == 1) {
		if ( strcmp(filename,"NUL") != 0 ) {
            if ( filename[0] != '/' ) {  // prepend full path
                sprintf( daemon_infile, "%s%c", cwd, DIR_DELIM_CHAR );
            } else {
                daemon_infile[0] = '\0';
            }
			strcat ( daemon_infile, filename );
			if ( (daemon_fds[0]=open(daemon_infile, O_RDONLY) ) < 0 ) {
				dprintf(D_ALWAYS,"failed to open stdin file %s, errno %d\n",
						daemon_infile, errno);
			}
			dprintf ( D_ALWAYS, "Tool Daemon Input file: %s\n", daemon_infile );
		}
		free( filename );
		filename = NULL;
	}

	if( JobAd->LookupString(ATTR_TOOL_DAEMON_OUTPUT, &filename ) == 1 ) {
		if( strcmp(filename,"NUL") != 0 ) {
            if( filename[0] != '/' ) {  // prepend full path
                sprintf( daemon_outfile, "%s%c", cwd, DIR_DELIM_CHAR );
            } else {
                daemon_outfile[0] = '\0';
            }
			strcat ( daemon_outfile, filename );
			if( (daemon_fds[1] = 
				 open(daemon_outfile,O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
					// if failed, try again without O_TRUNC
				if( (daemon_fds[1] = 
					 open( daemon_outfile,O_WRONLY|O_CREAT, 0666)) < 0 ) {
					dprintf( D_ALWAYS,
							 "failed to open stdout file %s, errno %d\n",
							 daemon_outfile, errno );
				}
			}
			dprintf ( D_ALWAYS, " Tool Daemon Output file: %s\n",
					  daemon_outfile );
		}
		free( filename );
		filename = NULL;
	}


	if( JobAd->LookupString(ATTR_TOOL_DAEMON_ERROR, &filename ) == 1 ) {
	    if( strcmp(filename,"NUL") != 0 ) {
            if( filename[0] != '/' ) {  // prepend full path
                sprintf( daemon_errfile, "%s%c", cwd, DIR_DELIM_CHAR );
            } else {
                daemon_errfile[0] = '\0';
            }
			strcat( daemon_errfile, filename );
			if( (daemon_fds[2] =
				 open(daemon_errfile,O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) { 
					// if failed, try again without O_TRUNC
				if( (daemon_fds[2] = 
					 open(daemon_errfile,O_WRONLY|O_CREAT, 0666)) < 0 ) {
					dprintf( D_ALWAYS,
							 "failed to open stderr file %s, errno %d\n",
							 daemon_errfile, errno );
				}
			}
			dprintf( D_ALWAYS, "Tool Daemon Error file: %s\n",
					 daemon_errfile ); 
		}
		free( filename );
		filename = NULL;
	}

	char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if ( ptmp ) {
		nice_inc = atoi(ptmp);
		free(ptmp);
	} else {
		nice_inc = 0;
	}

	dprintf( D_ALWAYS, "About to exec %s %s\n", daemon_name,
			 daemon_args.GetCStr() ); 

	set_priv( priv );

	JobPid = daemonCore->
		Create_Process( daemon_name, (char*)daemon_args.GetCStr(),
						PRIV_USER_FINAL, 1, FALSE, NULL, cwd, TRUE,
						NULL, daemon_fds, nice_inc );

		// now close the descriptors in daemon_fds array.  our child has inherited
		// them already, so we should close them so we do not leak descriptors.

	for (i=0;i<3;i++) {
		if ( daemon_fds[i] != -1 ) {
			close(daemon_fds[i]);
		}
	}

	if ( JobPid == FALSE ) {
		JobPid = -1;
		EXCEPT( "Create_Process(%s,%s, ...) failed",
				daemon_name, daemon_args.GetCStr() );
		free( daemon_name );
		return 0;
	}

	dprintf( D_ALWAYS, "Create_Process succeeded, pid=%d\n", JobPid );

	free( daemon_name );
	return 1;
}

int
ToolDaemonProc::JobCleanup(int pid, int status)
{
    int reason;	
    bool job_exit_wants_ad = true;

    dprintf( D_FULLDEBUG, "Inside ToolDaemonProc::JobCleanup()\n" );

    // For now, we don't try to do any special cleanup.  All we have
    // to do is decide if the pid that exited is the ToolDaemon.
    if( JobPid == pid ) {		
        return 1;
    }
    return 0;
}


// We don't have to do anything special to notify a shadow that we've
// really exited.
bool
ToolDaemonProc::JobExit( void )
{
    return true;
}


void
ToolDaemonProc::Suspend()
{
	daemonCore->Send_Signal(JobPid, DC_SIGSTOP);
	job_suspended = true;
}

void
ToolDaemonProc::Continue()
{
	daemonCore->Send_Signal(JobPid, DC_SIGCONT);
	job_suspended = false;
}

bool
ToolDaemonProc::ShutdownGraceful()
{
    if( job_suspended ) {
        Continue();
    }
    requested_exit = true;
    daemonCore->Send_Signal(JobPid, DC_SIGTERM);
    return false;	// return false says shutdown is pending	
}

bool
ToolDaemonProc::ShutdownFast()
{
        // We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
    requested_exit = true;
    daemonCore->Send_Signal(JobPid, DC_SIGKILL);
    return false;	// return false says shutdown is pending
}


bool
ToolDaemonProc::PublishUpdateAd( ClassAd* ad ) 
{
    dprintf( D_FULLDEBUG, "Inside ToolDaemonProc::PublishUpdateAd()\n" );
    // Nothing special for us to do.
    return true;
}
