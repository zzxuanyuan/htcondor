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
#include "env.h"
#include "user_proc.h"
#include "tool_daemon_proc.h"
#include "starter.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_distribution.h"
#ifdef WIN32
#include "perm.h"
#endif

extern CStarter *Starter;

/* ToolDaemonProc class implementation */

ToolDaemonProc::ToolDaemonProc( ClassAd *jobAd, int application_pid )
{
    dprintf( D_FULLDEBUG, "In ToolDaemonProc::ToolDaemonProc()\n" );
	family = NULL;
    JobAd = jobAd;
    JobPid = Cluster = Proc = -1;
	exit_status = -1;
	requested_exit = false;
    job_suspended = false;
	ApplicationPid = application_pid;
	snapshot_tid = -1;

}


ToolDaemonProc::~ToolDaemonProc()
{
    if( JobAd ) {
        delete JobAd;
    }
	if (family) {
		delete family;
	}
	if ( snapshot_tid != -1 ) {
		daemonCore->Cancel_Timer(snapshot_tid);
		snapshot_tid = -1;
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

	char DaemonNameTemp [_POSIX_PATH_MAX];
	char DaemonName	[_POSIX_PATH_MAX];
	if( JobAd->LookupString( ATTR_TOOL_DAEMON_CMD, DaemonNameTemp ) != 1 ) {
	    dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting StartJob. \n", 
				 ATTR_TOOL_DAEMON_CMD );
	    return 0;
	}

	sprintf( DaemonName, "%s%c%s", Starter->GetWorkingDir(),
				 DIR_DELIM_CHAR, DaemonNameTemp );

	dprintf ( D_FULLDEBUG, " Daemon Name %s \n", DaemonName);

		// This is something of an ugly hack.  filetransfer doesn't
		// preserve file permissions when it moves a file.  so, our
		// tool "binary" (or script, whatever it is), is sitting in
		// the starter's directory without an execute bit set.  So,
		// we've got to call chmod() so that exec() doesn't fail.

		// TODO:	
        // If daemon_name is an absolute path, chmod has to be applied
        // to the file copied in cwd (skipping path information referred
	    // to the submiting host).

	priv_state old_priv = set_user_priv();
	int retval = chmod( DaemonName, S_IRWXU | S_IRWXO | S_IRWXG );
	set_priv( old_priv );
	if( retval < 0 ) {
		dprintf( D_ALWAYS, "Failed to chmod %s!\n", DaemonName );
		return 0;
	}

//	initKillSigs ();

	char DaemonArgs[_POSIX_ARG_MAX];
	char tmp[_POSIX_ARG_MAX];

	if( JobAd->LookupString(ATTR_TOOL_DAEMON_ARGS, tmp) != 1 ) {
	    dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting "
				 "ToolDaemonProc::StartJob()\n",
				 ATTR_TOOL_DAEMON_ARGS );
	    return 0;
	}

		// for now, simply prepend the daemon name to the args - this
		// becomes argv[0]. We also pass the pid of the application 
	    // as a last argument (a temporal hack before the TDP communication
        // library is used for that. 

	if ( tmp[0] != '\0' ){
		sprintf (DaemonArgs, "%s %s %d", DaemonName, tmp, ApplicationPid);
	} else {
		sprintf (DaemonArgs, "%s %d", DaemonName, ApplicationPid);
	}

	dprintf ( D_FULLDEBUG, " Daemon Args %s \n", DaemonArgs);

	char* env_str = NULL;
	if( JobAd->LookupString(ATTR_JOB_ENVIRONMENT, &env_str) != 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  "
				 "Aborting OsProc::StartJob.\n", ATTR_JOB_ENVIRONMENT );  
		return 0;
	}
		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.

	Env job_env;

	if( ! job_env.Merge(env_str) ) {
		dprintf( D_ALWAYS, "Invalid %s found in JobAd.  "
				 "Aborting ToolDaemonProc::StartJob.\n", ATTR_JOB_ENVIRONMENT );  
		return 0;
	}
		// Next, we can free the string we got back from
		// LookupString() so we don't leak any memory.
	free( env_str );

	// Now, add some env vars the user job might want to see:
	char	envName[256];
	sprintf( envName, "%s_SCRATCH_DIR", myDistro->GetUc() );
	job_env.Put( envName, Starter->GetWorkingDir() );

		// Deal with port regulation stuff
	char* low = param( "LOWPORT" );
	char* high = param( "HIGHPORT" );
	if( low && high ) {
		sprintf( envName, "_%s_HIGHPORT", myDistro->Get() );
		job_env.Put( envName, high );
		sprintf( envName, "_%s_LOWPORT", myDistro->Get() );
		job_env.Put( envName, low );
		free( high );
		free( low );
	} else if( low ) {
		dprintf( D_ALWAYS, "LOWPORT is defined but HIGHPORT is not, "
				 "ignoring LOWPORT\n" );
		free( low );
	} else if( high ) {
		dprintf( D_ALWAYS, "HIGHPORT is defined but LOWPORT is not, "
				 "ignoring HIGHPORT\n" );
		free( high );
    }

	char* cwd = NULL;
	if (JobAd->LookupString(ATTR_JOB_IWD, &cwd) != 1) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  "
				 "Aborting ToolDaemonProc::StartJob()\n", ATTR_JOB_IWD );
		return 0;
	}

	dprintf (D_FULLDEBUG, "Before File management \n");

		// handle stdin, stdout, and stderr redirection for Daemon
	int daemon_fds[3];
	int failedStdin, failedStdout, failedStderr;
	daemon_fds[0] = -1; daemon_fds[1] = -1; daemon_fds[2] = -1;	
	failedStdin = 0; failedStdout = 0; failedStderr = 0;
	char filename1[_POSIX_PATH_MAX];
	char  *filename = NULL;
	char daemon_infile[_POSIX_PATH_MAX];
	char daemon_outfile[_POSIX_PATH_MAX];
	char daemon_errfile[_POSIX_PATH_MAX];

		// in order to open these files we must have the user's privs:

	priv_state priv;
	priv = set_user_priv();

	if( JobAd->LookupString(ATTR_TOOL_DAEMON_INPUT, filename1 ) == 1) {
		if ( !mynullFile(filename1) ) {
			if( Starter->wantsFileTransfer() ) {
				filename = basename( filename1 );
			} else {
				filename = filename1;
			}
            if ( filename[0] != '/' ) {  // prepend full path
                sprintf( daemon_infile, "%s%c", cwd, DIR_DELIM_CHAR );
            } else {
                daemon_infile[0] = '\0';
            }
			strcat ( daemon_infile, filename );
			if ( (daemon_fds[0]=open(daemon_infile, O_RDONLY) ) < 0 ) {
				dprintf(D_ALWAYS,"failed to open stdin file %s, errno %d\n",
						daemon_infile, errno);
				failedStdin = 1;
			}
			dprintf ( D_ALWAYS, "Tool Daemon Input file: %s\n", daemon_infile );
		}
	}else {
	#ifndef WIN32
		if ( (daemon_fds[0]=open( "/dev/null", O_RDONLY ) ) < 0 ) {
			dprintf(D_ALWAYS, "failed to open stdin file /dev/null, errno %d\n",
				errno);
			failedStdin = 1;
		}
	#endif
	}

	if( JobAd->LookupString(ATTR_TOOL_DAEMON_OUTPUT, filename1 ) == 1 ) {
		if( !mynullFile(filename1) ) {
			if( Starter->wantsFileTransfer() ) {
				filename = basename( filename1 );
			} else {
				filename = filename1;
			}
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
					failedStdout = 1;
				}
			}
			dprintf ( D_ALWAYS, " Tool Daemon Output file: %s\n",
					  daemon_outfile );
		}
	}else {
	#ifndef WIN32
		if ((daemon_fds[1]=open("/dev/null",O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
			// if failed, try again without O_TRUNC
			if ( (daemon_fds[1]=open( "/dev/null", O_WRONLY | O_CREAT, 0666)) < 0 ) {
				dprintf(D_ALWAYS, 
					"failed to open stdout file /dev/null, errno %d\n", 
					 errno);
				failedStdout = 1;
			}
		}
	#endif
	}
	if( JobAd->LookupString(ATTR_TOOL_DAEMON_ERROR, filename1 ) == 1 ) {
	    if(!mynullFile(filename1) ) {
			if( Starter->wantsFileTransfer() ) {
				filename = basename( filename1 );
			} else {
				filename = filename1;
			}
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
					failedStderr = 1;
				}
			}
			dprintf( D_ALWAYS, "Tool Daemon Error file: %s\n",
					 daemon_errfile ); 
		}
	} else {
	#ifndef WIN32
		if ((daemon_fds[2]=open("/dev/null",O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
			// if failed, try again without O_TRUNC
			if ( (daemon_fds[2]=open( "/dev/null", O_WRONLY | O_CREAT, 0666)) < 0 ) {
				dprintf(D_ALWAYS, 
						"failed to open stderr file /dev/null, errno %d\n", 
						errno);
				failedStderr = 1;
			}
		}
	#endif
	}

/* Bail out if we couldn't open the std files correctly */
	if ( failedStdin || failedStdout || failedStderr ) {
		/* only close ones that had been opened correctly */
		if (daemon_fds[0] != -1) {
			close(daemon_fds[0]);
		}
		if (daemon_fds[1] != -1) {
			close(daemon_fds[1]);
		}
		if (daemon_fds[2] != -1) {
			close(daemon_fds[2]);
		}
		dprintf(D_ALWAYS, "Failed to open some/all of the std files...\n");
		dprintf(D_ALWAYS, "Aborting OsProc::StartJob.\n");
		set_priv(priv); /* go back to original priv state before leaving */
		return 0;
	}

	dprintf ( D_FULLDEBUG, "After file management \n");

	char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if ( ptmp ) {
		nice_inc = atoi(ptmp);
		free(ptmp);
	} else {
		nice_inc = 0;
	}

	dprintf( D_ALWAYS, "About to exec %s %s\n", DaemonName,
			 DaemonArgs ); 
	
	// Grap the full environment back out of the Env object 
	env_str = job_env.getDelimitedString();

	set_priv( priv );

	JobPid = daemonCore->
		Create_Process( DaemonName, DaemonArgs,
						PRIV_USER_FINAL, 1, FALSE, env_str, cwd, TRUE,
						NULL, daemon_fds, nice_inc, DCJOBOPT_NO_ENV_INHERIT );

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
				DaemonName, DaemonArgs );
		return FALSE;
	}
	else{

		dprintf( D_ALWAYS, "Create_Process succeeded, pid=%d\n", JobPid );

		// success!  create a ProcFamily
		family = new ProcFamily(JobPid,PRIV_USER);
		ASSERT(family);

#ifdef WIN32
		// we only support running jobs as user nobody for the first pass
		char nobody_login[60];
		//sprintf(nobody_login,"condor-run-dir_%d",daemonCore->getpid());
		sprintf(nobody_login,"condor-run-%d",daemonCore->getpid());
		// set ProcFamily to find decendants via a common login name
		family->setFamilyLogin(nobody_login);
#endif

		// take a snapshot of the family every 15 seconds
		snapshot_tid = daemonCore->Register_Timer(2, 15, 
			(TimerHandlercpp)&ProcFamily::takesnapshot, 
			"ProcFamily::takesnapshot", family);
		return TRUE;
	}

	return FALSE;
}

/*
void
ToolDaemonProc::initKillSigs( void )
{
	int sig;

	sig = findSoftKillSig( JobAd );
	if( sig > 0 ) {
		soft_kill_sig = sig;
	} else {
		soft_kill_sig = SIGTERM;
	}

	sig = findRmKillSig( JobAd );
	if( sig > 0 ) {
		rm_kill_sig = sig;
	} else {
		rm_kill_sig = SIGTERM;
	}

	const char* tmp = signalName( soft_kill_sig );
	dprintf( D_FULLDEBUG, "KillSignal: %d (%s)\n", soft_kill_sig, 
			 tmp ? tmp : "Unknown" );

	tmp = signalName( rm_kill_sig );
	dprintf( D_FULLDEBUG, "RmKillSignal: %d (%s)\n", rm_kill_sig, 
			 tmp ? tmp : "Unknown" );
}

*/

int
ToolDaemonProc::JobCleanup(int pid, int status)
{
    int reason;	
    bool job_exit_wants_ad = true;

    dprintf( D_FULLDEBUG, "Inside ToolDaemonProc::JobCleanup()\n" );

		// If the tool exited, we want to shutdown everything, and
		// also return a 1 so the CStarter knows it can put us on the
		// CleanedUpJobList.
    if( JobPid == pid ) {	
	
		family->hardkill ();

		if( snapshot_tid >= 0 ) {
			daemonCore->Cancel_Timer(snapshot_tid);
		}
		snapshot_tid = -1;

        return 1;
    } 
		// If any other process (namely, the application) exited, kill
		// our own tool, since there's nothing more for us to do.

//	this->ShutdownGraceful();

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
		dprintf(D_FULLDEBUG,"in ToolDaemonProc::Suspend()\n");

		// suspend the tool daemon job
	if ( family ) {
		family->suspend();
	}

        // set our flag
	job_suspended = true;
}

void
ToolDaemonProc::Continue()
{
	dprintf(D_FULLDEBUG,"in ToolDaemonProc::Continue()\n");

	// resume user job
	if ( family ) {
		family->resume();
	}
        // set our flag
	job_suspended = false;
}


bool
ToolDaemonProc::ShutdownGraceful()
{
  dprintf(D_FULLDEBUG,"in ToolDaemonProc::ShutdownGraceful()\n");

	if ( !family ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

    // take a snapshot before we softkill the parent job process.
	// this helps ensure that if the parent exits without killing
	// the kids, our JobExit() handler will get em all.
	family->takesnapshot();

	// now softkill the parent job process.
    if( job_suspended ) {
        Continue();
    }
//    requested_exit = true;
    daemonCore->Send_Signal(JobPid, SIGTERM);
    return false;	// return false says shutdown is pending	
}

bool
ToolDaemonProc::ShutdownFast()
{
  	dprintf(D_FULLDEBUG,"in ToolDaemonProc::ShutdownFast()\n");

	if ( !family ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}  

    // We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
//    requested_exit = true;
	family->hardkill();

    return false;	// return false says shutdown is pending
}


bool
ToolDaemonProc::PublishUpdateAd( ClassAd* ad ) 
{
    dprintf( D_FULLDEBUG, "Inside ToolDaemonProc::PublishUpdateAd()\n" );
    // Nothing special for us to do.
    return true;
}

int 
mynullFile(const char *filename)
{
	// On WinNT, /dev/null is NUL
	// on UNIX, /dev/null is /dev/null
	
	// a UNIX->NT submit will result in the NT starter seeing /dev/null, so it
	// needs to recognize that /dev/null is the null file

	// an NT->NT submit will result in the NT starter seeing NUL as the null 
	// file

	// a UNIX->UNIX submit ill result in the UNIX starter seeing /dev/null as
	// the null file
	
	// NT->UNIX submits are not worried about - we don't think that anyone can
	// do them, and to make it clean we'll fix submit to always use /dev/null,
	// in the job ad, even on NT. 

	#ifdef WIN32
	if(_stricmp(filename, "NUL") == 0) {
		return 1;
	}
	#endif
	if(strcmp(filename, "/dev/null") == 0 ) {
		return 1;
	}
	return 0;
}
