/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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
#include "local_user_log.h"
#include "job_info_communicator.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "exit.h"


LocalUserLog::LocalUserLog( JobInfoCommunicator* my_jic )
{
	jic = my_jic;
	is_initialized = false;
}


LocalUserLog::~LocalUserLog()
{
		// don't delete the jic, since it's not really ours
}


bool
LocalUserLog::init( const char* filename, bool is_xml, 
					int cluster, int proc, int subproc )
{
	if( ! jic->userPrivInitialized() ) { 
		EXCEPT( "LocalUserLog::init() "
				"called before user priv is initialized!" );
	}
	priv_state priv;
	priv = set_user_priv();

	u_log.initialize( filename, cluster, proc, subproc );

	set_priv( priv );

	if( is_xml ) {
		u_log.setUseXML( true );
	} else {
		u_log.setUseXML( false );
	}
	
	dprintf( D_FULLDEBUG, "Starter's UserLog is \"%s\"\n", filename );
	is_initialized = true;
	return true;
}


bool
LocalUserLog::initFromJobAd( ClassAd* ad, const char* iwd )
{
    char tmp[_POSIX_PATH_MAX], logfilename[_POSIX_PATH_MAX];
	int use_xml = FALSE;
	int cluster = 1, proc = 0, subproc = 0;

    if( ! ad->LookupString(ATTR_STARTER_ULOG_FILE, tmp) ) {
        dprintf( D_FULLDEBUG, "no %s found\n", 
				 ATTR_STARTER_ULOG_FILE );
		return false;
	}
	if ( tmp[0] == '/' || tmp[0]=='\\' || (tmp[1]==':' &&
										   tmp[2]=='\\') ) {
			// we have a full pathname in the job ad.  however, if the
			// job is using a different iwd (namely, filetransfer is
			// being used), we want to just stick it in the local iwd
			// for the job, instead.
		if( jic->iwdIsChanged() ) {
			char* base = basename(tmp);
			sprintf(logfilename, "%s/%s", iwd, base);
		} else {			
			strcpy(logfilename, tmp);
		}
	} else {
			// no full path, so, use the job's iwd...
		sprintf(logfilename, "%s/%s", iwd, tmp);
	}

	ad->LookupBool( ATTR_STARTER_ULOG_USE_XML, use_xml );

	ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	ad->LookupInteger( ATTR_PROC_ID, proc );

	return init( logfilename, (bool)use_xml, cluster, proc, subproc );
}


bool
LocalUserLog::logExecute( ClassAd* ad )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logExecute() called before init()" );
	}

	ExecuteEvent event;
	strcpy( event.executeHost, daemonCore->InfoCommandSinfulString() ); 

	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_EXECUTE event: "
                 "can't write to UserLog!\n" );
		return false;
    }
	return true;
}



bool
LocalUserLog::logSuspend( ClassAd* ad )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logSuspend() called before init()" );
	}

	JobSuspendedEvent event;

	int num = 0;
	if( ! ad->LookupInteger(ATTR_NUM_PIDS, num) ) {
		dprintf( D_ALWAYS, "LocalUserLog::logSuspend() "
				 "ERROR: %s not defined in update ad, assuming 1\n", 
				 ATTR_NUM_PIDS );
		num = 1;
	}
    event.num_pids = num;

	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_SUSPENDED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logContinue( ClassAd* ad )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logContinue() called before init()" );
	}

	JobUnsuspendedEvent event;

	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_UNSUSPENDED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logTerminate( ClassAd* ad, int exit_reason )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logTerminate() called before init()" );
	}
	switch( exit_reason ) {
    case JOB_EXITED:
    case JOB_COREDUMPED:
        break;
    default:
        dprintf( D_ALWAYS, "LocalUserLog::logTerminate() "
				 "called with unknown reason (%d), aborting", exit_reason ); 
        return false;
    }

	JobTerminatedEvent event;

	int int_value = 0;
	bool exited_by_signal = false;
	if( ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_value) ) {
        if( int_value ) {
            exited_by_signal = true;
        } 
    } else {
		EXCEPT( "in LocalUserLog::logTerminate() "
				"ERROR: ClassAd does not define %s!",
				ATTR_ON_EXIT_BY_SIGNAL );
	}

    if( exited_by_signal ) {
        event.normal = false;
		if( ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_value) ) {
			event.signalNumber = int_value;
		} else {
			EXCEPT( "in LocalUserLog::logTerminate() "
					"ERROR: ClassAd does not define %s!",
					ATTR_ON_EXIT_SIGNAL );
		}
    } else {
        event.normal = true;
		if( ad->LookupInteger(ATTR_ON_EXIT_CODE, int_value) ) {
			event.returnValue = int_value;
		} else {
			EXCEPT( "in LocalUserLog::logTerminate() "
					"ERROR: ClassAd does not define %s!",
					ATTR_ON_EXIT_CODE );
		}
    }

    struct rusage run_local_rusage;
	run_local_rusage = getRusageFromAd( ad );
	event.run_local_rusage = run_local_rusage;
        // remote rusage should be blank

	event.recvd_bytes = jic->bytesReceived();
    event.sent_bytes = jic->bytesSent();

		// TODO corefile name?!?!

	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_CONTINUED event\n" );
		return false;
    }
	return true;
}


bool
LocalUserLog::logEvict( ClassAd* ad, int exit_reason )
{
	if( ! is_initialized ) {
		EXCEPT( "LocalUserLog::logEvict() called before init()" );
	}

    switch( exit_reason ) {
    case JOB_CKPTED:
    case JOB_NOT_CKPTED:
    case JOB_KILLED:
        break;
    default:
        dprintf( D_ALWAYS, "LocalUserLog::logEvict() "
				 "called with unknown reason (%d), aborting", exit_reason ); 
        return false;
    }

    JobEvictedEvent event;

    struct rusage run_local_rusage;
	run_local_rusage = getRusageFromAd( ad );
	event.run_local_rusage = run_local_rusage;
        // remote rusage should be blank

    event.checkpointed = (exit_reason == JOB_CKPTED);
    
	event.recvd_bytes = jic->bytesReceived();
    event.sent_bytes = jic->bytesSent();
    
	if( !u_log.writeEvent(&event) ) {
        dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n" );
		return false;
    }
	return true;
}


struct rusage
LocalUserLog::getRusageFromAd( ClassAd* ad )
{
    struct rusage local_rusage;
    memset( &local_rusage, 0, sizeof(struct rusage) );

	float sys_time = 0;
	float user_time = 0;

	if( ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, sys_time) ) {
        local_rusage.ru_stime.tv_sec = (int) sys_time; 
    }
    if( ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, user_time) ) {
        local_rusage.ru_utime.tv_sec = (int) user_time; 
    }

	return local_rusage;
}

