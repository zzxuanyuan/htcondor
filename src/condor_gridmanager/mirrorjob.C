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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "environ.h"  // for Environ object
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"

#include "gridmanager.h"
#include "mirrorjob.h"
#include "condor_config.h"


// GridManager job states
#define GM_INIT					0
#define GM_REGISTER				1
#define GM_STDIO_UPDATE			2
#define GM_UNSUBMITTED			3
#define GM_SUBMIT				4
#define GM_SUBMIT_SAVE			5
#define GM_SUBMIT_COMMIT		6
#define GM_SUBMITTED			7
#define GM_DONE_SAVE			8
#define GM_DONE_COMMIT			9
#define GM_CANCEL				10
#define GM_FAILED				11
#define GM_DELETE				12
#define GM_CLEAR_REQUEST		13
#define GM_HOLD					14
#define GM_PROXY_EXPIRED		15
#define GM_REFRESH_PROXY		16
#define GM_START				17
#define GM_ENABLE_LOCAL_SCHEDULING 18
#define GM_DISABLE_LOCAL_SCHEDULING 19

static char *GMStateNames[] = {
	"GM_INIT",
	"GM_REGISTER",
	"GM_STDIO_UPDATE",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMIT_COMMIT",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_DONE_COMMIT",
	"GM_CANCEL",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_REFRESH_PROXY",
	"GM_START",
	"GM_ENABLE_LOCAL_SCHEDULING",
	"GM_DISABLE_LOCAL_SCHEDULING"
};

#define JOB_STATE_UNKNOWN				-1
#define JOB_STATE_UNSUBMITTED			UNEXPANDED

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, remoteState %d: %s returned error %d\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],remoteState, \
        func,error)


#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, MirrorJob *>;
template class HashBucket<HashKey, MirrorJob *>;

HashTable <HashKey, GT3Job *> MirrorJobsById( HASH_TABLE_SIZE,
											  hashFunction );


void MirrorJobInit()
{
}

void MirrorJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "MIRROR_JOB_POLL_INTERVAL", 5 * 60 );
	MirrorJob::setPollInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	MirrorJob::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	MirrorJob::setConnectFailureRetry( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_MIRROR_LEASE_INTERVAL",1800);
	MirrorJob::setLeaseInterval( tmp_int );
}

const char *MirrorJobAdConst = "JobUniverse =?= 5 && MirrorJob =?= True";

bool MirrorJobAdMustExpand( const ClassAd *jobad )
{
	int must_expand = 0;

	jobad->LookupBool(ATTR_JOB_MUST_EXPAND, must_expand);

	return must_expand != 0;
}

BaseJob *MirrorJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new MirrorJob( jobad );
}


int MirrorJob::pollMirrorInterval = 300;		// default value
int MirrorJob::submitInterval = 300;			// default value
int MirrorJob::gahpCallTimeout = 300;			// default value
int MirrorJob::maxConnectFailures = 3;			// default value
int MirrorJob::leaseInterval = 1800;			// default value

MirrorJob::MirrorJob( ClassAd *classad )
	: BaseJob( classad )
{
	int bool_value;
	char buff[4096];
	char *error_string = NULL;

	mirrorJobId.cluster = 0;
	gahpAd = NULL;
	gmState = GM_INIT;
	remoteState = JOB_STATE_UNKNOWN;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	submitFailureCode = 0;
	mirrorScheddName = NULL;
	remoteJobIdString = NULL;
	localJobSchedulingEnabled = false;
	myResource = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( ad->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		UpdateJobAd( ATTR_HOLD_REASON, "UNDEFINED" );
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_MIRROR_SCHEDD, buff );
	if ( buff[0] != '\0' ) {
		mirrorScheddName = strdup( buff );
	} else {
		error_string = "MirrorSchedd is not set in the job ad";
		goto error_exit;
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_MIRROR_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		SetRemoteJobId( buff );
	} else {
		remoteState = JOB_STATE_UNSUBMITTED;
	}

	myResource = MirrorResource::FindOrCreateResource( mirrorScheddName );
	myResource->RegisterJob( this );

	char *gahp_path = param("MIRROR_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "MIRROR_GAHP not defined";
		goto error_exit;
	}
		// TODO remove mirrorScheddName from the gahp server key if/when
		//   a gahp server can handle multiple schedds
	sprintf( buff, "MIRROR/%s", mirrorScheddName );
	gahp = new GahpClient( buff, gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	int tmp1 = 0, tmp2 = 0;
	ad->LookupInteger( ATTR_MIRROR_OK_TO_MATCH, tmp1 );
	ad->LookupInteger( ATTR_SCHEDD_BIRTHDATE, tmp2 );
	if ( tmp1 != 0 && tmp1 != 0 && tmp1 => tmp2 ) {
		localJobSchedulingEnabled = true;
	}

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( error_string ) {
		UpdateJobAdString( ATTR_HOLD_REASON, error_string );
	}
	return;
}

MirrorJob::~MirrorJob()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( remoteJobIdString != NULL ) {
		MirrorJobsById.remove( HashKey( remoteJobIdString ) );
		free( remoteJobIdString );
	}
	if ( mirrorScheddName ) {
		free( mirrorScheddName );
	}
	if ( gahpAd ) {
		delete gahpAd;
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
}

void MirrorJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int MirrorJob::doEvaluateState()
{
	int old_gm_state;
	int old_remote_state;
	bool reevaluate_state = true;
	time_t now;	// make sure you set this before every use!!!

	bool done;
	int rc;
	int status;
	int error;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, remoteState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],remoteState);

	gahp->setMode( GahpClient::normal );

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_remote_state = remoteState;

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			int err;

			if ( gahp->Startup() == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error starting GAHP\n",
						 procID.cluster, procID.proc );

				UpdateJobAdString( ATTR_HOLD_REASON, "Failed to start GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gmState = GM_START;
			} break;
		case GM_START: {
			// This state is the real start of the state machine, after
			// one-time initialization has been taken care of.
			// If we think there's a running jobmanager
			// out there, we try to register for callbacks (in GM_REGISTER).
			// The one way jobs can end up back in this state is if we
			// attempt a restart of a jobmanager only to be told that the
			// old jobmanager process is still alive.
			errorString = "";
			if ( mirrorJobId.cluster == 0 ) {
				gmState = GM_CLEAR_REQUEST;
			} else {

				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_UNSUBMITTED: {
			// There are no outstanding remote submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			} else {
				gmState = GM_SUBMIT;
			}
			} break;
		case GM_SUBMIT: {
			// Start a new remote submission for this job.
			char *job_contact = NULL;
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				UpdateJobAdString( ATTR_HOLD_REASON,
									"Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				char *job_id_string = NULL;
				if ( gahpAd == NULL ) {
					gahpAd = buildSubmitAd();
				}
				if ( gahpAd == NULL ) {
					gmState = GM_HOLD;
					break;
				}
				rc = gahp->condor_job_submit( mirrorScheddName,
											  gahpAd,
											  &job_id_string );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				if ( rc == GLOBUS_SUCCESS ) {
					SetRemoteJobId( job_id_string );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) condor_job_submit() failed\n",
							 procID.cluster, procID.proc );
					gmState = GM_UNSUBMITTED;
					reevaluate_state = true;
				}
				if ( job_id_string != NULL ) {
					free( job_id_string );
				}
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_UNSUBMITTED;
			} else {
				unsigned int delay = 0;
				if ( (lastSubmitAttempt + submitInterval) > now ) {
					delay = (lastSubmitAttempt + submitInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_SUBMIT_SAVE: {
			// Save the job id for a new remote submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_SUBMIT_COMMIT;
			}
			} break;
		case GM_SUBMIT_COMMIT: {
			// Now that we've saved the job id, ???

			// TODO tell the schedd that it's ok to schedule the job now?
			gmState = GM_SUBMITTED;
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted. Wait for completion or failure,
			// and poll the remote schedd occassionally to let it know
			// we're still alive.
			if ( remoteState == COMPLETED ) {
				gmState = GM_DONE_SAVE;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else if ( remoteState == HELD &&
						localJobSchedulingEnabled == false ) {
				gmState = GM_ENABLE_LOCAL_SCHEDULING;
			} else if ( remoteState != HELD &&
						localJobSchedulerEnabled == true ) {
				gmState = GM_DISABLE_LOCAL_SCHEDULING;
			}
			} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			JobTerminated();
			if ( condorState == COMPLETED ) {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the remote schedd it can remove the job from the queue.
			if ( gahpAd == NULL ) {
				MyString expr;
				gahpAd = new ClassAd;
				expr.sprintf( "%s = False", ATTR_JOB_LEAVE_IN_QUEUE );
				gahpAd->Insert( expr.Value() );
			}
			rc = gahp->condor_job_update( mirrorScheddName, mirrorJobId,
										  gahpAd );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				dprintf( D_ALWAYS,
						 "(%d.%d) condor_job_complete() failed\n",
						 procID.cluster, procID.proc );
				gmState = GM_CANCEL;
				break;
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				SetRemoteJobId( NULL );
				requestScheddUpdate( this );
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.

			// Should this if-stmt be here? Even if the job is completed,
			// we may still want to remove it (say if we have trouble
			// fetching the output files).
			if ( remoteState != COMPLETED ) {
				rc = gahp->condor_job_remove( mirrorScheddName, mirrorJobId );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					dprintf( D_ALWAYS,
							 "(%d.%d) condor_job_remove() failed\n",
							 procID.cluster, procID.proc );
					gmState = GM_CLEAR_REQUEST;
					break;
				}
			}
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_DELETE: {
			// We are done with the job. Propagate any remaining updates
			// to the schedd, then delete this object.
			DoneWithJob();
			// This object will be deleted when the update occurs
			} break;
		case GM_CLEAR_REQUEST: {
			// Remove all knowledge of any previous or present job
			// submission, in both the gridmanager and the schedd.

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( mirrorJobId.cluster != 0 && condorState != REMOVED ) {
				gmState = GM_HOLD;
				break;
			}
			errorString = "";
			SetRemoteJobId( NULL );
			JobIdle();
			if ( submitLogged ) {
				JobEvicted();
//				if ( !evictLogged ) {
//					WriteEvictEventToUserLog( ad );
//					evictLogged = true;
//				}
			}
			
			// If there are no updates to be done when we first enter this
			// state, requestScheddUpdate will return done immediately
			// and not waste time with a needless connection to the
			// schedd. If updates need to be made, they won't show up in
			// schedd_actions after the first pass through this state
			// because we modified our local variables the first time
			// through. However, since we registered update events the
			// first time, requestScheddUpdate won't return done until
			// they've been committed to the schedd.
			done = requestScheddUpdate( this );
			if ( !done ) {
				break;
			}
			submitLogged = false;
			executeLogged = false;
			submitFailedLogged = false;
			terminateLogged = false;
			abortLogged = false;
			evictLogged = false;
			gmState = GM_UNSUBMITTED;
			remoteState = JOB_STATE_UNSUBMITTED;
			} break;
		case GM_HOLD: {
			// Put the job on hold in the schedd.
			// TODO: what happens if we learn here that the job is removed?

			// If the condor state is already HELD, then someone already
			// HELD it, so don't update anything else.
			if ( condorState != HELD ) {

				// Set the hold reason as best we can
				// TODO: set the hold reason in a more robust way.
				char holdReason[1024];
				holdReason[0] = '\0';
				holdReason[sizeof(holdReason)-1] = '\0';
				ad->LookupString( ATTR_HOLD_REASON, holdReason,
								  sizeof(holdReason) - 1 );
				if ( holdReason[0] == '\0' && errorString != "" ) {
					strncpy( holdReason, errorString.Value(),
							 sizeof(holdReason) - 1 );
				}
				if ( holdReason[0] == '\0' ) {
					strncpy( holdReason, "Unspecified gridmanager error",
							 sizeof(holdReason) - 1 );
				}

				JobHeld( holdReason );
			}
			gmState = GM_DELETE;
			} break;
		case GM_ENABLE_LOCAL_SCHEDULING: {
			UpdateJobAdInt( ATTR_MIRROR_OK_TO_MATCH, time(NULL) );
			requestScheddUpdate( this );
			localJobSchedulingEnabled = true;
			gmState = GM_SUBMITTED;
			} break;
		case GM_DISABLE_LOCAL_SCHEDULING: {
			UpdateJobAd( ATTR_MIRROR_OK_TO_MATCH, "UNDEFINED" );
			requestScheddUpdate( this );
				// may have to send a vacate command and wait for job to
				// be vacated. MirrorOkToMatch needs to be set in schedd
				// ad before job finishes vacating.
			localJobSchedulingEnabled = false;
			gmState = GM_SUBMITTED;
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state || remoteState != old_remote_state ) {
			reevaluate_state = true;
		}
		if ( remoteState != old_remote_state ) {
//			dprintf(D_FULLDEBUG, "(%d.%d) remote state change: %s -> %s\n",
//					procID.cluster, procID.proc,
//					JobStatusNames(old_remote_state),
//					JobStatusNames(remoteState));
			enteredCurrentRemoteState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending gahp call, we're not
			// anymore so purge it.
			gahp->purgePendingRequests();
			// If we were calling a gahp func that used gahpAd, we're done
			// with it now, so free it.
			if ( gahpAd ) {
				delete gahpAd;
				gahpAd = NULL;
			}
		}

	} while ( reevaluate_state );

	return TRUE;
}

void MirrorJob::SetRemoteJobId( const char *job_id )
{
	if ( remoteJobIdString != NULL ) {
		MirrorJobsById.remove( HashKey( remoteJobIdString ) );
		free( remoteJobIdString );
		remoteJobIdString = NULL;
		mirrorJobId.cluster = 0;
		UpdateJobAd( ATTR_MIRROR_JOB_ID, "UNDEFINED" );
	}
	if ( job_id != NULL ) {
		MyString id_string;
		sscanf( job_id, "%d.%d", &mirrorJobId.cluster, &mirrorJobId.proc );
		id_string.sprintf( "%s/%d.%d", mirrorScheddName, mirrorJobId.cluster,
						   mirrorJobId.proc );
		remoteJobIdString = strdup( id_string.Value() );
		MirrorJobsById.insert( HashKey( remoteJobIdString ), job );
		UpdateJobAdString( ATTR_MIRROR_JOB_ID, job_id );
	}
	requestScheddUpdate( this );
}

void MirrorJob::RemoteJobStatusUpdate( ClassAd *update_ad )
{
	int rc;
	int tmp_int;

	update_ad->LookupInteger( ATTR_JOB_STATUS, &tmp_int );

	if ( remoteState == HELD && tmp_int != HELD ) {
		UpdateJobAdBool( ATTR_MIRROR_ACTIVE, 1 );
	}
	remoteState = tmp_int;

	rc = update_ad->LookupInteger( ATTR_MIRROR_LEASE_TIME, &tmp_int );
	if ( rc ) {
		UpdateJobAdInt( ATTR_MIRROR_REMOTE_LEASE_TIME, tmp_int );
	} else {
		UpdateJobAd( ATTR_MIRROR_REMOTE_LEASE_TIME, "Undefined" );
	}

	requestScheddUpdate( this );
}

BaseResource *MirrorJob::GetResource()
{
	return (BaseResource *)NULL;
}

ClassAd *MirrorJob::buildSubmitAd()
{
	MyString expr;
	ClassAd *submit_ad;

		// Base the submit ad on our own job ad
	submit_ad = new ClassAd( *ad );

	submit_ad->Delete( ATTR_CLUSTER_ID );
	submit_ad->Delete( ATTR_PROC_ID );
	submit_ad->Delete( ATTR_MIRROR_JOB );
	submit_ad->Delete( ATTR_MIRROR_REMOTE_LEASE_TIME );
	submit_ad->Delete( ATTR_MIRROR_ACTIVE );
	submit_ad->Delete( ATTR_MIRROR_SCHEDD );
	submit_ad->Delete( ATTR_MIRROR_JOB_ID );
	submit_ad->Delete( ATTR_MIRROR_LEASE_TIME );
	submit_ad->Delete( ATTR_PERIODIC_HOLD_CHECK );
	submit_ad->Delete( ATTR_PERIODIC_RELEASE_CHECK );
	submit_ad->Delete( ATTR_PERIODIC_REMOVE_CHECK );
	submit_ad->Delete( ATTR_ON_EXIT_HOLD_CHECK );
	submit_ad->Delete( ATTR_ON_EXIT_REMOVE_CHECK );
	submit_ad->Delete( ATTR_HOLD_REASON );
	submit_ad->Delete( ATTR_HOLD_REASON_CODE );
	submit_ad->Delete( ATTR_HOLD_REASON_SUBCODE );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON_CODE );
	submit_ad->Delete( ATTR_LAST_HOLD_REASON_SUBCODE );
	submit_ad->Delete( ATTR_RELEASE_REASON );
	submit_ad->Delete( ATTR_LAST_RELEASE_REASON );
	submit_ad->Delete( ATTR_JOB_STATUS_ON_RELEASE );
	submit_ad->Delete( ATTR_DAG_NODE_NAME );
	submit_ad->Delete( ATTR_DAGMAN_JOB_ID );
	submit_ad->Delete( ATTR_ULOG_FILE );

	expr.sprintf( "%s = %d", ATTR_Q_DATE, time(NULL) );
	submit_ad->Insert( expr.Value() );

	expr.sprintf( "%s = 0", ATTR_COMPLETION_DATE );
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0.0", ATTR_JOB_REMOTE_WALL_CLOCK);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0.0", ATTR_JOB_LOCAL_USER_CPU);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0.0", ATTR_JOB_LOCAL_SYS_CPU);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0.0", ATTR_JOB_REMOTE_USER_CPU);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0.0", ATTR_JOB_REMOTE_SYS_CPU);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0", ATTR_JOB_EXIT_STATUS);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0", ATTR_NUM_CKPTS);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0", ATTR_NUM_RESTARTS);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0", ATTR_NUM_SYSTEM_HOLDS);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0", ATTR_JOB_COMMITTED_TIME);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0", ATTR_TOTAL_SUSPENSIONS);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0", ATTR_LAST_SUSPENSION_TIME);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = 0", ATTR_CUMULATIVE_SUSPENSION_TIME);
	submit_ad->Insert( expr.Value() );

	expr.sprintf (buffer, "%s = FALSE", ATTR_ON_EXIT_BY_SIGNAL);
	submit_ad->Insert( expr.Value() );

	expr.sprintf( "%s = 0", ATTR_CURRENT_HOSTS );
	submit_ad->Insert( expr.Value() );

	expr.sprintf( "%s = %d", ATTR_ENTERED_CURRENT_STATUS );
	submit_ad->Insert( expr.Value() );

	expr.sprintf( "%s = NEVER", ATTR_JOB_NOTIFICATION );
	submit_ad->Insert( expr.Value() );

	expr.sprintf( "%s = True", ATTR_JOB_LEAVE_IN_QUEUE );
	submit_ad->Insert( expr.Value() );

	expr.sprintf( "%s = %s =?= Undefined && ENV.CurrentTime > %s + %d",
				  ATTR_JOB_PERIODIC_REMOVE_CHECK, ATTR_MIRROR_LEASE_TIME,
				  ATTR_Q_DATE, 1800 );
	submit_ad->Insert( expr.Value() );

		// worry about ATTR_JOB_[OUTPUT|ERROR]_ORIG

	return submit_ad;
}
