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
#include "nullfile.h"

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
#define GM_STAGE_IN				14
#define GM_STAGE_OUT			15

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
	"GM_PROBE_JOB",
	"GM_STAGE_IN",
	"GM_STAGE_OUT"
};

#define TASK_IN_PROGRESS	1
#define TASK_DONE			2
#define TASK_FAILED			3

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_strcmp _stricmp
#	define file_contains contains_anycase
#else
#	define file_strcmp strcmp
#	define file_contains contains
#endif

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
	bool rc = false;
	int universe;
	char *sub_universe = NULL;
	if ( jobad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) == 1 &&
		 universe == CONDOR_UNIVERSE_GLOBUS ) {
		if ( jobad->LookupString( "SubUniverse", &sub_universe ) == 1 ) {
			if ( stricmp( sub_universe, "nordugrid" ) == 0 ) {
				rc = true;
			}
			free( sub_universe );
		}
	}
	return rc;
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

int NordugridJob::probeInterval = 300;	// default value
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
	stage_list = NULL;

	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
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
					gmState = GM_STAGE_IN;
				} else {
					dprintf(D_ALWAYS,"(%d.%d) job commit failed!\n",
							procID.cluster, procID.proc);
					gmState = GM_CANCEL;
				}
			}
			} break;
		case GM_STAGE_IN: {
			rc = doStageIn();
			if ( rc == TASK_IN_PROGRESS ) {
				break;
			} else if ( rc == TASK_FAILED ) {
				dprintf( D_ALWAYS, "(%d.%d) file stage in failed!\n",
						 procID.cluster, procID.proc );
				gmState = GM_CANCEL;
			} else {
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_SUBMITTED: {
			if ( condorState == COMPLETED ) {
					gmState = GM_STAGE_OUT;
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
		case GM_STAGE_OUT: {
			rc = doStageOut();
			if ( rc == TASK_IN_PROGRESS ) {
				break;
			} else if ( rc == TASK_FAILED ) {
				dprintf( D_ALWAYS, "(%d.%d) file stage out failed!\n",
						 procID.cluster, procID.proc );
				gmState = GM_CANCEL;
			} else {
				gmState = GM_DONE_SAVE;
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

int NordugridJob::doStageIn()
{
	FILE *curr_file_fp = NULL;
	FILE *curr_ftp_fp = NULL;
	char *curr_filename = NULL;

	if ( stage_list == NULL ) {
		char *buf = NULL;
		int transfer_exec = TRUE;

		ad->LookupString( ATTR_TRANSFER_INPUT_FILES, &buf );
		stage_list = new StringList( buf, "," );
		if ( buf != NULL ) {
			free( buf );
		}

		ad->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer_exec );
		if ( transfer_exec ) {
			ad->LookupString( ATTR_JOB_CMD, &buf );
			if ( !stage_list->file_contains( buf ) ) {
				stage_list->append( buf );
			}
			free( buf );
		}

		if ( ad->LookupString( ATTR_JOB_INPUT, &buf ) == 1) {
			// only add to list if not NULL_FILE (i.e. /dev/null)
			if ( ! nullFile(buf) ) {
				if ( !stage_list->file_contains( buf ) ) {
					stage_list->append( buf );			
				}
			}
			free( buf );
		}

		stage_list->rewind();
	}

	if ( ftp_srvr == NULL ) {

		MyString buff;

		ftp_srvr = ftp_lite_open( resourceManagerString, 2811, NULL );
		if ( ftp_srvr == NULL ) {
			errorString = "ftp_lite_open() failed";
			goto doStageIn_error_exit;
		}

		if ( ftp_lite_auth_globus( ftp_srvr ) == 0 ) {
			errorString = "ftp_lite_auth_globus() failed";
			goto doStageIn_error_exit;
		}

		buff.sprintf( "/jobs/%s", remoteJobId );
		if ( ftp_lite_change_dir( ftp_srvr, buff.Value() ) == 0 ) {
			errorString = "ftp_lite_change_dir() failed";
			goto doStageIn_error_exit;
		}

	}

	curr_filename = stage_list->next();

	if ( curr_filename != NULL ) {

		MyString full_filename;
		if ( curr_filename[0] != DIR_DELIM_CHAR ) {
			char *iwd = NULL;
			ad->LookupString( ATTR_JOB_IWD, &iwd );
			full_filename.sprintf( "%s%c%s", iwd, DIR_DELIM_CHAR,
								   curr_filename );
			free( iwd );
		} else {
			full_filename = curr_filename;
		}
		curr_file_fp = fopen( full_filename.Value(), "r" );
		if ( curr_file_fp == NULL ) {
			errorString = "fopen failed";
			goto doStageIn_error_exit;
		}

		curr_ftp_fp = ftp_lite_put( ftp_srvr, basename(curr_filename), 0,
									FTP_LITE_WHOLE_FILE );
		if ( curr_ftp_fp == NULL ) {
			errorString = "ftp_lite_put() failed";
			goto doStageIn_error_exit;
		}

		if ( ftp_lite_stream_to_stream( curr_file_fp, curr_ftp_fp ) == -1 ) {
			errorString = "ftp_lite_stream_to_stream failed";
			goto doStageIn_error_exit;
		}

		fclose( curr_file_fp );
		curr_file_fp = NULL;

		fclose( curr_ftp_fp );
		curr_ftp_fp = NULL;

		if ( ftp_lite_done( ftp_srvr ) == 0 ) {
			errorString = "ftp_lite_done() failed";
			goto doStageIn_error_exit;
		}

		SetEvaluateState();
		return TASK_IN_PROGRESS;
	}

	ftp_lite_close( ftp_srvr );
	ftp_srvr = NULL;

	delete stage_list;
	stage_list = NULL;

	return TASK_DONE;

 doStageIn_error_exit:
	if ( curr_file_fp != NULL ) {
		fclose( curr_file_fp );
	}
	if ( curr_ftp_fp != NULL ) {
		fclose( curr_ftp_fp );
	}
	if ( ftp_srvr != NULL ) {
		ftp_lite_close( ftp_srvr );
		ftp_srvr = NULL;
	}
	if ( stage_list != NULL ) {
		delete stage_list;
		stage_list = NULL;
	}
	return TASK_FAILED;
}

int NordugridJob::doStageOut()
{
	FILE *curr_file_fp = NULL;
	FILE *curr_ftp_fp = NULL;
	char *curr_filename = NULL;

	if ( stage_list == NULL ) {
		char *buf = NULL;

		ad->LookupString( ATTR_TRANSFER_OUTPUT_FILES, &buf );
		stage_list = new StringList( buf, "," );
		if ( buf != NULL ) {
			free( buf );
		}

		if ( ad->LookupString( ATTR_JOB_OUTPUT, &buf ) == 1) {
			// only add to list if not NULL_FILE (i.e. /dev/null)
			if ( ! nullFile(buf) ) {
				if ( !stage_list->file_contains( buf ) ) {
					stage_list->append( buf );			
				}
			}
			free( buf );
		}

		if ( ad->LookupString( ATTR_JOB_ERROR, &buf ) == 1) {
			// only add to list if not NULL_FILE (i.e. /dev/null)
			if ( ! nullFile(buf) ) {
				if ( !stage_list->file_contains( buf ) ) {
					stage_list->append( buf );			
				}
			}
			free( buf );
		}

		stage_list->rewind();
	}

	if ( ftp_srvr == NULL ) {

		MyString buff;

		ftp_srvr = ftp_lite_open( resourceManagerString, 2811, NULL );
		if ( ftp_srvr == NULL ) {
			errorString = "ftp_lite_open() failed";
			goto doStageOut_error_exit;
		}

		if ( ftp_lite_auth_globus( ftp_srvr ) == 0 ) {
			errorString = "ftp_lite_auth_globus() failed";
			goto doStageOut_error_exit;
		}

		buff.sprintf( "/jobs/%s", remoteJobId );
		if ( ftp_lite_change_dir( ftp_srvr, buff.Value() ) == 0 ) {
			errorString = "ftp_lite_change_dir() failed";
			goto doStageOut_error_exit;
		}

	}

	curr_filename = stage_list->next();

	if ( curr_filename != NULL ) {

		MyString full_filename;
		if ( curr_filename[0] != DIR_DELIM_CHAR ) {
			char *iwd = NULL;
			ad->LookupString( ATTR_JOB_IWD, &iwd );
			full_filename.sprintf( "%s%c%s", iwd, DIR_DELIM_CHAR,
								   curr_filename );
			free( iwd );
		} else {
			full_filename = curr_filename;
		}
		curr_file_fp = fopen( full_filename.Value(), "w" );
		if ( curr_file_fp == NULL ) {
			errorString = "fopen failed";
			goto doStageOut_error_exit;
		}

		curr_ftp_fp = ftp_lite_get( ftp_srvr, basename(curr_filename), 0 );
		if ( curr_ftp_fp == NULL ) {
			errorString = "ftp_lite_put() failed";
			goto doStageOut_error_exit;
		}

		if ( ftp_lite_stream_to_stream( curr_ftp_fp, curr_file_fp ) == -1 ) {
			errorString = "ftp_lite_stream_to_stream failed";
			goto doStageOut_error_exit;
		}

		fclose( curr_file_fp );
		curr_file_fp = NULL;

		fclose( curr_ftp_fp );
		curr_ftp_fp = NULL;

		if ( ftp_lite_done( ftp_srvr ) == 0 ) {
			errorString = "ftp_lite_done() failed";
			goto doStageOut_error_exit;
		}

		SetEvaluateState();
		return TASK_IN_PROGRESS;
	}

	ftp_lite_close( ftp_srvr );
	ftp_srvr = NULL;

	delete stage_list;
	stage_list = NULL;

	return TASK_DONE;

 doStageOut_error_exit:
	if ( curr_file_fp != NULL ) {
		fclose( curr_file_fp );
	}
	if ( curr_ftp_fp != NULL ) {
		fclose( curr_ftp_fp );
	}
	if ( ftp_srvr != NULL ) {
		ftp_lite_close( ftp_srvr );
		ftp_srvr = NULL;
	}
	if ( stage_list != NULL ) {
		delete stage_list;
		stage_list = NULL;
	}
	return TASK_FAILED;
}

MyString *NordugridJob::buildSubmitRSL()
{
	int transfer_exec = TRUE;
	MyString *rsl = new MyString;
	MyString buff;
	StringList stage_in_list( NULL, "," );
	StringList stage_out_list( NULL, "," );
	char *attr_value = NULL;
	char *rsl_suffix = NULL;
	char *iwd = NULL;
	char *executable = NULL;

	if ( ad->LookupString( ATTR_GLOBUS_RSL, &rsl_suffix ) &&
						   rsl_suffix[0] == '&' ) {
		*rsl = rsl_suffix;
		free( rsl_suffix );
		return rsl;
	}

	if ( ad->LookupString( ATTR_JOB_IWD, &iwd ) != 1 ) {
		errorString = "ATTR_JOB_IWD not defined";
		if ( rsl_suffix != NULL ) {
			free( rsl_suffix );
		}
		return NULL;
	}

	//Start off the RSL
	rsl->sprintf( "&(savestate=yes)(action=request)(lrmstype=pbs)(hostname=nostos.cs.wisc.edu)(gmlog=job.log)" );

	//We're assuming all job clasads have a command attribute
	ad->LookupString( ATTR_JOB_CMD, &executable );
	ad->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer_exec );

	*rsl += "(arguments=";
	// If we're transferring the executable, strip off the path for the
	// remote machine, since it refers to the submit machine.
	if ( transfer_exec ) {
		*rsl += basename( executable );
	} else {
		*rsl += executable;
	}

	if ( ad->LookupString(ATTR_JOB_ARGUMENTS, &attr_value) && *attr_value ) {
		*rsl += " ";
		*rsl += attr_value;
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	// If we're transferring the executable, tell Nordugrid to set the
	// execute bit on the transferred executable.
	if ( transfer_exec ) {
		*rsl += ")(excutables=";
		*rsl += basename( executable );
	}

	ad->LookupString( ATTR_TRANSFER_INPUT_FILES, &attr_value );
	if ( attr_value != NULL ) {
		stage_in_list.initializeFromString( attr_value );
		free( attr_value );
		attr_value = NULL;
	}

	if ( ad->LookupString( ATTR_JOB_INPUT, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stdin=";
			*rsl += basename(attr_value);
			if ( !stage_in_list.file_contains( attr_value ) ) {
				stage_in_list.append( attr_value );
			}
		}
		free( attr_value );
		attr_value = NULL;
	}

	if ( transfer_exec ) {
		if ( !stage_in_list.file_contains( executable ) ) {
			stage_in_list.append( executable );
		}
	}

	if ( stage_in_list.isEmpty() == false ) {
		char *file;
		stage_in_list.rewind();

		*rsl += ")(inputfiles=";

		while ( (file = stage_in_list.next()) != NULL ) {
			*rsl += "(";
			*rsl += basename(file);
			*rsl += " \"\")";
		}
	}

	ad->LookupString( ATTR_TRANSFER_OUTPUT_FILES, &attr_value );
	if ( attr_value != NULL ) {
		stage_out_list.initializeFromString( attr_value );
		free( attr_value );
		attr_value = NULL;
	}

	if ( ad->LookupString( ATTR_JOB_OUTPUT, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stdout=";
			*rsl += basename(attr_value);
			if ( !stage_out_list.file_contains( attr_value ) ) {
				stage_out_list.append( attr_value );
			}
		}
		free( attr_value );
		attr_value = NULL;
	}

	if ( ad->LookupString( ATTR_JOB_ERROR, &attr_value ) == 1) {
		// only add to list if not NULL_FILE (i.e. /dev/null)
		if ( ! nullFile(attr_value) ) {
			*rsl += ")(stderr=";
			*rsl += basename(attr_value);
			if ( !stage_out_list.file_contains( attr_value ) ) {
				stage_out_list.append( attr_value );
			}
		}
		free( attr_value );
	}

	if ( stage_out_list.isEmpty() == false ) {
		char *file;
		stage_out_list.rewind();

		*rsl += ")(outputfiles=";

		while ( (file = stage_out_list.next()) != NULL ) {
			*rsl += "(";
			*rsl += basename(file);
			*rsl += " \"\")";
		}
	}

	*rsl += ')';

	if ( rsl_suffix != NULL ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

	free( executable );
	free( iwd );

dprintf(D_FULLDEBUG,"*** RSL='%s'\n",rsl->Value());
	return rsl;
}

#endif // if defined(NORDUGRID_UNIVERSE)
