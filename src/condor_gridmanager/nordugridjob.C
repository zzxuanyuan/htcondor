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

#if defined(NORDUGRID_UNIVERSE)

#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "environ.h"  // for Environ object
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"

#include "gridmanager.h"
#include "nordugridjob.h"
#include "condor_config.h"


// GridManager job states
#define GM_INIT					0
#define GM_UNSUBMITTED			1
#define GM_SUBMIT				2
#define GM_SUBMIT_SAVE			3
#define GM_SUBMIT_COMMIT		4
#define GM_SUBMITTED			5
#define GM_DONE_SAVE			6
#define GM_DONE_COMMIT			7
#define GM_CANCEL				8
#define GM_FAILED				9
#define GM_DELETE				10
#define GM_CLEAR_REQUEST		11
#define GM_HOLD					12
#define GM_PROBE_JOB			13

static char *GMStateNames[] = {
	"GM_INIT",
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
	"GM_PROBE_JOB"
};

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

//////////////////////from gridmanager.C
#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, NordugridJob *>;
template class HashBucket<HashKey, NordugridJob *>;

HashTable <HashKey, NordugridJob *> JobsByRemoteId( HASH_TABLE_SIZE,
													hashFunction );

void
rehashRemoteJobId( NordugridJob *job, const char *old_id,
				   const char *new_id )
{
	if ( old_id ) {
		JobsByRemoteId.remove(HashKey(old_id));
	}
	if ( new_id ) {
		JobsByRemoteId.insert(HashKey(new_id), job);
	}
}

void NordugridJobInit()
{
}

void NordugridJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	NordugridJob::setProbeInterval( tmp_int );
}

bool NordugridJobAdMatch( const ClassAd *jobad )
{
	int universe;
	if ( jobad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) == 0 ||
		 universe != CONDOR_UNIVERSE_GLOBUS ||
		 jobad->LookupBool( "NordugridJob", universe ) == 0 ) {
dprintf(D_FULLDEBUG,"***NordugridJobAdMatch returning false\n");
		return false;
	} else {
dprintf(D_FULLDEBUG,"***NordugridJobAdMatch returning true\n");
		return true;
	}
}

bool NordugridJobAdMustExpand( const ClassAd *jobad )
{
	int must_expand = 0;

	jobad->LookupBool(ATTR_JOB_MUST_EXPAND, must_expand);

	return must_expand != 0;
}

BaseJob *NordugridJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new NordugridJob( jobad );
}

int NordugridJob::probeInterval = 300;		// default value
int NordugridJob::submitInterval = 300;	// default value

NordugridJob::NordugridJob( ClassAd *classad )
	: BaseJob( classad )
{
	char buff[4096];
	char *error_string = NULL;

	remoteJobId = NULL;
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	resourceManagerString = NULL;
	ftp_srvr = NULL;

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( ad->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		UpdateJobAd( ATTR_HOLD_REASON, "UNDEFINED" );
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBUS_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		resourceManagerString = strdup( buff );
	} else {
		error_string = "GlobusResource is not set in the job ad";
		goto error_exit;
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBUS_CONTACT_STRING, buff );
	if ( buff[0] != '\0' && strcmp( buff, NULL_JOB_CONTACT ) != 0 ) {
		rehashRemoteJobId( this, remoteJobId, buff );
		remoteJobId = strdup( buff );
	}

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( error_string ) {
		UpdateJobAdString( ATTR_HOLD_REASON, error_string );
	}
	return;
}

NordugridJob::~NordugridJob()
{
	if ( remoteJobId ) {
		rehashRemoteJobId( this, remoteJobId, NULL );
		free( remoteJobId );
	}
}

void NordugridJob::Reconfig()
{
	BaseJob::Reconfig();
}

int NordugridJob::doEvaluateState()
{
	int old_gm_state;
	bool reevaluate_state = true;
	time_t now;	// make sure you set this before every use!!!

	bool done;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, condorState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],condorState);

	do {
		reevaluate_state = false;
		old_gm_state = gmState;

		switch ( gmState ) {
		case GM_INIT: {
			errorString = "";
			if ( remoteJobId == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
				submitLogged = true;
				if ( condorState == RUNNING ||
					 condorState == COMPLETED ) {
					executeLogged = true;
				}

				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_UNSUBMITTED: {
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
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
//				UpdateJobAdString( ATTR_HOLD_REASON,
//									"Attempts to submit failed" );
				gmState = GM_HOLD;
				break;
			}
			now = time(NULL);
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {

				char *job_id;

				job_id = doSubmit();

				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;

				if ( job_id != NULL ) {
					rehashRemoteJobId( this, remoteJobId, job_id );
					remoteJobId = job_id;
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   job_id );
					gmState = GM_SUBMIT_SAVE;
				} else {
					dprintf(D_ALWAYS,"(%d.%d) job submit failed!\n",
							procID.cluster, procID.proc);
					gmState = GM_UNSUBMITTED;
				}

			} else {
				unsigned int delay = 0;
				if ( (lastSubmitAttempt + submitInterval) > now ) {
					delay = (lastSubmitAttempt + submitInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_SUBMIT_SAVE: {
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
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = doCommit();
				if ( rc >= 0 ) {
					gmState = GM_SUBMITTED;
				} else {
					dprintf(D_ALWAYS,"(%d.%d) job commit failed!\n",
							procID.cluster, procID.proc);
					gmState = GM_CANCEL;
				}
			}
			} break;
		case GM_SUBMITTED: {
			if ( condorState == COMPLETED ) {
					gmState = GM_DONE_SAVE;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				now = time(NULL);
				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				if ( now >= lastProbeTime + probeInterval ) {
					gmState = GM_PROBE_JOB;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_PROBE_JOB: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = doStatus();
				if ( rc < 0 ) {
					// What to do about failure?
					dprintf( D_ALWAYS, "(%d.%d) job probe failed!\n",
							 procID.cluster, procID.proc );
				} else if ( rc != condorState ) {
					if ( rc == RUNNING && condorState != RUNNING ) {
						JobRunning();
					}
					if ( rc == IDLE && condorState != IDLE ) {
						JobIdle();
					}
					if ( rc == COMPLETED && condorState != COMPLETED ) {
						JobTerminated( true, 0 );
					}
					requestScheddUpdate( this );
				}
				lastProbeTime = now;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_DONE_SAVE: {
			if ( condorState != HELD && condorState != REMOVED ) {
				JobTerminated( true, 0 );
				if ( condorState == COMPLETED ) {
					done = requestScheddUpdate( this );
					if ( !done ) {
						break;
					}
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			rc = doRemove();
			if ( rc < 0 ) {
				dprintf( D_ALWAYS, "(%d.%d) job cleanup failed!\n",
						 procID.cluster, procID.proc );
				gmState = GM_HOLD;
				break;
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				if ( remoteJobId != NULL ) {
					rehashRemoteJobId( this, remoteJobId, NULL );
					free( remoteJobId );
					remoteJobId = NULL;
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   NULL_JOB_CONTACT );
					requestScheddUpdate( this );
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			rc = doRemove();
			if ( rc >= 0 ) {
				gmState = GM_FAILED;
			} else {
				// What to do about a failed cancel?
				dprintf( D_ALWAYS, "(%d.%d) job cancel failed!\n",
						 procID.cluster, procID.proc );
				gmState = GM_FAILED;
			}
			} break;
		case GM_FAILED: {
			rehashRemoteJobId( this, remoteJobId, NULL );
			free( remoteJobId );
			remoteJobId = NULL;
			UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
							   NULL_JOB_CONTACT );
			requestScheddUpdate( this );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_DELETE: {
			// The job has completed or been removed. Delete it from the
			// schedd.
			DoneWithJob();
			// This object will be deleted when the update occurs
			} break;
		case GM_CLEAR_REQUEST: {
			// Remove all knowledge of any previous or present job
			// submission, in both the gridmanager and the schedd.

			// If we are doing a rematch, we are simply waiting around
			// for the schedd to be updated and subsequently this globus job
			// object to be destroyed.  So there is nothing to do.
			if ( wantRematch ) {
				break;
			}

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( remoteJobId != NULL
				     && condorState != REMOVED 
					 && wantResubmit == 0 
					 && doResubmit == 0 ) {
				gmState = GM_HOLD;
				break;
			}
			// Only allow a rematch *if* we are also going to perform a resubmit
			if ( wantResubmit || doResubmit ) {
				ad->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
			}
			if ( wantResubmit ) {
				wantResubmit = 0;
				dprintf(D_ALWAYS,
						"(%d.%d) Resubmitting to Globus because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
			}
			if ( doResubmit ) {
				doResubmit = 0;
				dprintf(D_ALWAYS,
					"(%d.%d) Resubmitting to Globus (last submit failed)\n",
						procID.cluster, procID.proc );
			}
			errorString = "";
			if ( remoteJobId != NULL ) {
				rehashRemoteJobId( this, remoteJobId, NULL );
				free( remoteJobId );
				remoteJobId = NULL;
				UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
								   NULL_JOB_CONTACT );
			}
			JobIdle();
			if ( submitLogged ) {
				JobEvicted();
				if ( !evictLogged ) {
					WriteEvictEventToUserLog( ad );
					evictLogged = true;
				}
			}
			
			if ( wantRematch ) {
				dprintf(D_ALWAYS,
						"(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_REMATCH_CHECK );

				// Set ad attributes so the schedd finds a new match.
				int dummy;
				if ( ad->LookupBool( ATTR_JOB_MATCHED, dummy ) != 0 ) {
					UpdateJobAdBool( ATTR_JOB_MATCHED, 0 );
					UpdateJobAdInt( ATTR_CURRENT_HOSTS, 0 );
				}

				// If we are rematching, we need to forget about this job
				// cuz we wanna pull a fresh new job ad, with a fresh new match,
				// from the all-singing schedd.
				gmState = GM_DELETE;
				break;
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
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state ) {
			reevaluate_state = true;
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
		}

	} while ( reevaluate_state );

	return TRUE;
}

BaseResource *NordugridJob::GetResource()
{
//	return (BaseResource *)myResource;
	return NULL;
}

char *NordugridJob::doSubmit()
{
	char *job_dir = NULL;
	char *job_id = NULL;
	MyString buff;

	ftp_srvr = ftp_lite_open( resourceManagerString, 2811, NULL );
	if ( ftp_srvr == NULL ) {
		errorString = "ftp_lite_open() failed";
		goto doSubmit_error_exit;
	}

//	buff.sprintf( "X509_USER_PROXY=%s", X509Proxy );
//	putenv( strdup( buff.Value() ) );

	if ( ftp_lite_auth_globus( ftp_srvr ) == 0 ) {
		errorString = "ftp_lite_auth_globus() failed";
		goto doSubmit_error_exit;
	}

	if ( ftp_lite_change_dir( ftp_srvr, "/jobs/new" ) == 0 ) {
		errorString = "ftp_lite_change_dir() failed";
		goto doSubmit_error_exit;
	}

	if ( ftp_lite_print_dir( ftp_srvr, &job_dir ) == 0 ) {
		errorString = "ftp_lite_print_dir() failed";
		goto doSubmit_error_exit;
	}

	job_id = strrchr( job_dir, '/' );
	if ( job_id == NULL ) {
		errorString = "strrchr() failed";
		goto doSubmit_error_exit;
	}
	job_id++;

	dprintf(D_FULLDEBUG,"*** job_dir='%s' job_id='%s'\n",job_dir,job_id);
	job_id = strdup( job_id );
	free( job_dir );
	return job_id;

 doSubmit_error_exit:
	if ( job_dir != NULL ) {
		free( job_dir );
	}
	if ( ftp_srvr != NULL ) {
		ftp_lite_close( ftp_srvr );
		ftp_srvr = NULL;
	}

dprintf(D_FULLDEBUG,"*** failure: %s\n",errorString.Value());
	return NULL;
}

int NordugridJob::doCommit()
{
	FILE *ftp_put_fp = NULL;
	MyString *rsl = NULL;
	size_t rsl_len;

	rsl = buildSubmitRSL();
	rsl_len = strlen( rsl->Value() );

	if ( ftp_srvr == NULL ) {
		errorString = "ftp_srvr is NULL";
		goto doCommit_error_exit;
	}

	ftp_put_fp = ftp_lite_put( ftp_srvr, "/jobs/new/job", 0,
							   FTP_LITE_WHOLE_FILE );
	if ( ftp_put_fp == NULL ) {
		errorString = "ftp_lite_put() failed";
		goto doCommit_error_exit;
	}

	if ( fwrite( rsl->Value(), 1, rsl_len, ftp_put_fp ) != rsl_len ) {
		errorString = "fwrite() failed";
		goto doCommit_error_exit;
	}

	fclose( ftp_put_fp );
	ftp_put_fp = NULL;

	if ( ftp_lite_done( ftp_srvr ) == 0 ) {
		errorString = "ftp_lite_done() failed";
		goto doCommit_error_exit;
	}

	ftp_lite_close( ftp_srvr );
	ftp_srvr = NULL;

	delete rsl;

	return 0;

 doCommit_error_exit:
	if ( ftp_put_fp != NULL ) {
		fclose( ftp_put_fp );
	}
	if ( rsl != NULL ) {
		delete rsl;
	}
	if ( ftp_srvr != NULL ) {
		ftp_lite_close( ftp_srvr );
		ftp_srvr = NULL;
	}

dprintf(D_FULLDEBUG,"*** failure: %s\n",errorString.Value());
	return -1;
}

int NordugridJob::doStatus()
{
	int rc;
	MyString dir;
	char *status_buff = NULL;
	int status_len = 0;
	FILE *status_fp = NULL;
	MyString buff;

	if ( ftp_srvr != NULL ) {
		errorString = "ftp_srvr not NULL";
		return -1;
	}

	ftp_srvr = ftp_lite_open( resourceManagerString, 2811, NULL );
	if ( ftp_srvr == NULL ) {
		errorString = "ftp_lite_open() failed";
		goto doStatus_error_exit;
	}

//	buff.sprintf( "X509_USER_PROXY=%s", X509Proxy );
//	putenv( strdup( buff.Value() ) );

	if ( ftp_lite_auth_globus( ftp_srvr ) == 0 ) {
		errorString = "ftp_lite_auth_globus() failed";
		goto doStatus_error_exit;
	}

	dir.sprintf( "/jobs/%s/job.log", remoteJobId );
	if ( ftp_lite_change_dir( ftp_srvr, dir.Value() ) == 0 ) {
		errorString = "ftp_lite_change_dir() failed";
		goto doStatus_error_exit;
	}

	status_fp = ftp_lite_get( ftp_srvr, "status", 0 );
	if ( status_fp == NULL ) {
		errorString = "ftp_lite_get() failed";
		goto doStatus_error_exit;
	}

	status_len = ftp_lite_stream_to_buffer( status_fp, &status_buff );
	if ( status_len == -1 ) {
		errorString = "ftp_lite_stream_to_buffer() failed";
		goto doStatus_error_exit;
	}

	fclose( status_fp );
	status_fp = NULL;

	if ( ftp_lite_done( ftp_srvr ) == 0 ) {
		errorString = "ftp_lite_done() failed";
		goto doStatus_error_exit;
	}

	ftp_lite_close( ftp_srvr );
	ftp_srvr = NULL;

dprintf(D_FULLDEBUG,"*** job status is '%s'\n",status_buff);
	if ( strncmp( status_buff, "ACCEPTED\n", status_len ) == 0 ||
		 strncmp( status_buff, "PREPARING\n", status_len ) == 0 ||
		 strncmp( status_buff, "SUBMITTING\n", status_len ) == 0 ||
		 strncmp( status_buff, "INLRMS\n", status_len ) == 0 ||
		 strncmp( status_buff, "CANCELLING\n", status_len ) == 0 ||
		 strncmp( status_buff, "FINISHING\n", status_len ) == 0 ) {
		rc = IDLE;
	} else if ( strncmp( status_buff, "FINISHED\n", status_len ) == 0 ) {
		rc = COMPLETED;
	} else {
		errorString = "invalid job status";
dprintf(D_FULLDEBUG,"*** invalid job status of '%s'\n",status_buff);
		rc = -1;
	}

	return rc;

 doStatus_error_exit:
	if ( status_buff != NULL ) {
		free( status_buff );
	}
	if ( status_fp != NULL ) {
		fclose( status_fp );
	}
	if ( ftp_srvr != NULL ) {
		ftp_lite_close( ftp_srvr );
		ftp_srvr = NULL;
	}

dprintf(D_FULLDEBUG,"*** failure: %s\n",errorString.Value());
	return -1;
}

int NordugridJob::doRemove()
{
	int rc;
	MyString dir;
	MyString buff;

	if ( ftp_srvr != NULL ) {
		errorString = "ftp_srvr not NULL";
		return -1;
	}

	ftp_srvr = ftp_lite_open( resourceManagerString, 2811, NULL );
	if ( ftp_srvr == NULL ) {
		errorString = "ftp_lite_open() failed";
		goto doRemove_error_exit;
	}

//	buff.sprintf( "X509_USER_PROXY=%s", X509Proxy );
//	putenv( strdup( buff.Value() ) );

	if ( ftp_lite_auth_globus( ftp_srvr ) == 0 ) {
		errorString = "ftp_lite_auth_globus() failed";
		goto doRemove_error_exit;
	}

	dir.sprintf( "/jobs/%s", remoteJobId );
dprintf(D_FULLDEBUG,"***deleting '%s'\n",dir.Value());
	if ( ftp_lite_delete_dir( ftp_srvr, dir.Value() ) == 0 ) {
		errorString = "ftp_lite_delete_dir() failed";
		goto doRemove_error_exit;
	}

	ftp_lite_close( ftp_srvr );
	ftp_srvr = NULL;

	return 0;

 doRemove_error_exit:
	if ( ftp_srvr != NULL ) {
		ftp_lite_close( ftp_srvr );
		ftp_srvr = NULL;
	}

dprintf(D_FULLDEBUG,"*** failure: %s\n",errorString.Value());
	return -1;
}

MyString *NordugridJob::buildSubmitRSL()
{
	int transfer;
	MyString *rsl = new MyString;
	MyString iwd = "";
	MyString riwd = "";
	MyString buff;
	char *attr_value = NULL;
	char *rsl_suffix = NULL;

	if ( ad->LookupString( ATTR_GLOBUS_RSL, &rsl_suffix ) &&
						   rsl_suffix[0] == '&' ) {
		*rsl = rsl_suffix;
		free( rsl_suffix );
		return rsl;
	}

	if ( ad->LookupString(ATTR_JOB_IWD, &attr_value) && *attr_value ) {
		iwd = attr_value;
		int len = strlen(attr_value);
		if ( len > 1 && attr_value[len - 1] != '/' ) {
			iwd += '/';
		}
	} else {
		iwd = "/";
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	//Start off the RSL
	rsl->sprintf( "&(savestate=yes)(action=request)(lrmstype=pbs)(hostname=nostos.cs.wisc.edu)(gmlog=job.log)" );

	//We're assuming all job clasads have a command attribute
	ad->LookupString( ATTR_JOB_CMD, &attr_value );
	*rsl += "(arguments=";
	*rsl += attr_value;
	free( attr_value );
	if ( ad->LookupString(ATTR_JOB_ARGUMENTS, &attr_value) && *attr_value ) {
		*rsl += " ";
		*rsl += attr_value;
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	*rsl += ')';

	if ( rsl_suffix != NULL ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

dprintf(D_FULLDEBUG,"*** RSL='%s'\n",rsl->Value());
	return rsl;
}

#endif // if defined(NORDUGRID_UNIVERSE)
