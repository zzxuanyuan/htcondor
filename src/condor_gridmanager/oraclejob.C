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
#include "oraclejob.h"
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

template class HashTable<HashKey, OracleJob *>;
template class HashBucket<HashKey, OracleJob *>;

HashTable <HashKey, OracleJob *> JobsByRemoteId( HASH_TABLE_SIZE,
												 hashFunction );

void
rehashRemoteJobId( OracleJob *job, const char *old_id,
				   const char *new_id )
{
	if ( old_id ) {
		JobsByRemoteId.remove(HashKey(old_id));
	}
	if ( new_id ) {
		JobsByRemoteId.insert(HashKey(new_id), job);
	}
}

void OracleJobInit()
{
}

void OracleJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	OracleJob::setProbeInterval( tmp_int );

//	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
//	OracleResource::setProbeInterval( tmp_int );

//	// Tell all the resource objects to deal with their new config values
//	OracleResource *next_resource;
//
//	ResourcesByName.startIterations();
//
//	while ( ResourcesByName.iterate( next_resource ) != 0 ) {
//		next_resource->Reconfig();
//	}
}

bool OracleJobAdMatch( const ClassAd *jobad )
{
	int universe;
	if ( jobad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) == 0 ||
		 universe != CONDOR_UNIVERSE_GLOBUS ||
		 jobad->LookupBool( "OracleJob", universe ) == 0 ) {
		return false;
	} else {
		return true;
	}
}

bool OracleJobAdMustExpand( const ClassAd *jobad )
{
	int must_expand = 0;

	jobad->LookupBool(ATTR_JOB_MUST_EXPAND, must_expand);

	return must_expand != 0;
}

int OracleJob::probeInterval = 300;		// default value
int OracleJob::submitInterval = 300;	// default value

OracleJob::OracleJob( ClassAd *classad )
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
//	myResource = NULL;

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

OracleJob::~OracleJob()
{
	if ( remoteJobId ) {
		rehashRemoteJobId( this, remoteJobId, NULL );
		free( remoteJobId );
	}
	if (daemonCore) {
		daemonCore->Cancel_Timer( evaluateStateTid );
	}
}

void OracleJob::Reconfig()
{
	BaseJob::Reconfig();
}

int OracleJob::doEvaluateState()
{
	int old_gm_state;
	bool reevaluate_state = true;
	int schedd_actions = 0;
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
			// This is the state all jobs start in when the GlobusJob object
			// is first created. If we think there's a running jobmanager
			// out there, we try to register for callbacks (in GM_REGISTER).
			// The one way jobs can end up back in this state is if we
			// attempt a restart of a jobmanager only to be told that the
			// old jobmanager process is still alive.
			errorString = "";
			if ( remoteJobId == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CANCEL;
			} else {
				submitLogged = true;
				if ( condorState == RUNNING ||
					 condorState == COMPLETED ) {
					executeLogged = true;
				}

				// TODO What about uncommitted jobs?
				probeNow = true;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_UNSUBMITTED: {
			// There are no outstanding gram submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				addScheddUpdateAction( this, UA_FORGET_JOB, GM_UNSUBMITTED );
				// This object will be deleted when the update occurs
				break;
			} else {
				gmState = GM_SUBMIT;
			}
			} break;
		case GM_SUBMIT: {
			// Start a new gram submission for this job.
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

				rc = do_submit();

				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;

				if ( rc >= 0 ) {
					char job_id[16];
					sprintf( job_id, "%d", rc );
					rehashRemoteJobId( this, remoteJobId, job_id );
					remoteJobId = strdup( job_id );
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
			// Save the jobmanager's contact for a new gram submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				done = addScheddUpdateAction( this, UA_UPDATE_JOB_AD,
											GM_SUBMIT_SAVE );
				if ( !done ) {
					break;
				}
				gmState = GM_SUBMIT_COMMIT;
			}
			} break;
		case GM_SUBMIT_COMMIT: {
			// Now that we've saved the jobmanager's contact, commit the
			// gram job submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				rc = do_commit();
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
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
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
				rc = do_status();
				if ( rc < 0 ) {
					// What to do about failure?
					dprintf( D_ALWAYS, "(%d.%d) job probe failed!\n",
							 procID.cluster, procID.proc );
				} else if ( rc != condorState ) {
					condorState = rc;
					UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
					addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );
					if ( rc == RUNNING || rc == COMPLETED && !executeLogged ) {
						WriteExecuteEventToUserLog( ad );
						executeLogged = true;
					}
				}
				lastProbeTime = now;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_DONE_SAVE: {
			// Report job completion to the schedd.
			if ( condorState != HELD && condorState != REMOVED ) {
				if ( condorState != COMPLETED ) {
					condorState = COMPLETED;
					UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
				}
				done = addScheddUpdateAction( this, UA_UPDATE_JOB_AD,
											  GM_DONE_SAVE );
				if ( !done ) {
					break;
				}
			}
			gmState = GM_DONE_COMMIT;
			} break;
		case GM_DONE_COMMIT: {
			// Tell the jobmanager it can clean up and exit.

			// For now, there's nothing to clean up
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
					addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			rc = do_remove();
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
			// The jobmanager's job state has moved to FAILED. Send a
			// commit if necessary and take appropriate action.
			rehashRemoteJobId( this, remoteJobId, NULL );
			free( remoteJobId );
			remoteJobId = NULL;
			UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
							   NULL_JOB_CONTACT );
			addScheddUpdateAction( this, UA_UPDATE_JOB_AD, 0 );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_DELETE: {
			// The job has completed or been removed. Delete it from the
			// schedd.
			schedd_actions = UA_DELETE_FROM_SCHEDD | UA_FORGET_JOB;
			if ( condorState == REMOVED ) {
//				schedd_actions |= UA_LOG_ABORT_EVENT;
				if ( !abortLogged ) {
					WriteAbortEventToUserLog( ad );
					abortLogged = true;
				}
			} else if ( condorState == COMPLETED ) {
//				schedd_actions |= UA_LOG_TERMINATE_EVENT;
				if ( !terminateLogged ) {
					WriteTerminateEventToUserLog( ad );
					email_terminate_event(ad);
					terminateLogged = true;
				}
			}
			addScheddUpdateAction( this, schedd_actions, GM_DELETE );
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
			schedd_actions = 0;
			errorString = "";
			if ( remoteJobId != NULL ) {
				rehashRemoteJobId( this, remoteJobId, NULL );
				free( remoteJobId );
				remoteJobId = NULL;
				UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
								   NULL_JOB_CONTACT );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			if ( condorState == RUNNING ) {
				condorState = IDLE;
				UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
				schedd_actions |= UA_UPDATE_JOB_AD;
			}
			if ( submitLogged ) {
//				schedd_actions |= UA_LOG_EVICT_EVENT;
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
					schedd_actions |= UA_UPDATE_JOB_AD;
				}

				// If we are rematching, we need to forget about this job
				// cuz we wanna pull a fresh new job ad, with a fresh new match,
				// from the all-singing schedd.
				schedd_actions |= UA_FORGET_JOB;
			}
			
			// If there are no updates to be done when we first enter this
			// state, addScheddUpdateAction will return done immediately
			// and not waste time with a needless connection to the
			// schedd. If updates need to be made, they won't show up in
			// schedd_actions after the first pass through this state
			// because we modified our local variables the first time
			// through. However, since we registered update events the
			// first time, addScheddUpdateAction won't return done until
			// they've been committed to the schedd.
			done = addScheddUpdateAction( this, schedd_actions,
										  GM_CLEAR_REQUEST );
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
			condorState = HELD;
			UpdateJobAdInt( ATTR_JOB_STATUS, condorState );
			schedd_actions = UA_HOLD_JOB | UA_FORGET_JOB | UA_UPDATE_JOB_AD;
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
			UpdateJobAdString( ATTR_HOLD_REASON, holdReason );
			addScheddUpdateAction( this, schedd_actions, GM_HOLD );
			// This object will be deleted when the update occurs
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

BaseResource *OracleJob::GetResource()
{
//	return (BaseResource *)myResource;
	return NULL;
}

char* OracleJob::run_java(char **args)
{
	static char reply[1024];
	MyString command;
	char *param_value;
	int rc;

#ifndef WIN32
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask,SIGCHLD);
	sigprocmask(SIG_SETMASK,&mask,0);
#endif

	*reply = '\0';

	param_value = param("CLASSPATH");
	if ( param_value == NULL ) {
		EXCEPT("CLASSPATH undefined!");
	}
	command = "java -classpath ";
	command += param_value;
	free(param_value);

	param_value = param("ORACLE_INTERFACE");
	if ( param_value == NULL ) {
		EXCEPT("ORACLE_INTERFACE undefined!");
	}
	command += " ";
	command += param_value;
	free(param_value);

	char *tmp;
	StringList list( resourceManagerString );
	list.rewind();
	while ( (tmp = list.next()) ) {
		command += " ";
		command += tmp;
	}

	char **tmp2 = args;
	while ( *tmp2 != NULL ) {
		command += " '";
		command += *tmp2;
		command += "'";
		tmp2++;
	}

	command += " >/tmp/temp_output";
command+=" 2>/tmp/temp_error.";
command+=args[0];

dprintf(D_ALWAYS,"*** command=%s\n",command.Value());
	rc = system( command.Value() );
	if ( rc == -1 ) {
		dprintf(D_ALWAYS,"system(%s) failed, errno=%d\n",command.Value(),
				errno);
		return reply;
	}
	dprintf(D_FULLDEBUG,"system() returned %d\n",rc);

	FILE *output = fopen("/tmp/temp_output","r");
	if (output==NULL) {
		dprintf(D_ALWAYS,"fopen failed\n");
		unlink("/tmp/temp_output");
		return reply;
	}

	if ( fgets(reply,sizeof(reply),output) == NULL ) {
		dprintf(D_ALWAYS,"fgets returned NULL\n");
		fclose(output);
		unlink("/tmp/temp_output");
		return reply;
	}

	if ( *reply && reply[strlen(reply)-1] == '\n' ) {
		reply[strlen(reply)-1] = '\0';
	}

	fclose(output);
	unlink("/tmp/temp_output");

	dprintf(D_ALWAYS,"Got reply '%s' from OracleJobInterface\n",reply);

	return reply;
}

int OracleJob::do_submit()
{
	char **args;
	char *reply;

	args = (char**)malloc(2*sizeof(char*));
	args[0] = "submit";
	args[1] = NULL;

	reply = run_java( args );

	free(args);

	if ( *reply == '\0' || strcmp(reply,"error")==0 ) {
		dprintf(D_ALWAYS,"Submit failed!\n");
		return -1;
	} else {
		return atoi(reply);
	}
}

int OracleJob::do_commit()
{
	char **args;
	char *reply;
	char buff[8192] = "";

	args = (char**)malloc(4*sizeof(char*));
	args[0] = "commit";
	args[1] = remoteJobId;
	ad->LookupString(ATTR_JOB_ARGUMENTS, buff);
	args[2] = buff;
	args[3] = NULL;

	reply = run_java( args );

	free(args);

	if ( *reply != '\0' ) {
		dprintf(D_ALWAYS,"Commit failed, reply='%s'!\n",reply);
		return -1;
	} else {
		return 0;
	}
}

int OracleJob::do_status()
{
	char **args;
	char *reply;

	args = (char**)malloc(3*sizeof(char*));
	args[0] = "status";
	args[1] = remoteJobId;
	args[2] = NULL;

	reply = run_java( args );

	free(args);

	if ( strcmp( reply, "IDLE" ) == 0 ) {
		return IDLE;
	} else if ( strcmp( reply, "RUNNING" ) == 0 ) {
		return RUNNING;
	} else if ( strcmp( reply, "COMPLETED" ) == 0 ) {
		return COMPLETED;
	} else {
		dprintf(D_ALWAYS,"Status retuned unknown value '%s'\n",reply);
		return -1;
	}
}

int OracleJob::do_remove()
{
	char **args;
	char *reply;
	char buff[8192] = "";

	args = (char**)malloc(3*sizeof(char*));
	args[0] = "remove";
	args[1] = remoteJobId;
	args[2] = NULL;

	reply = run_java( args );

	free(args);

	if ( *reply != '\0' ) {
		dprintf(D_ALWAYS,"Remove failed, reply='%s'!\n",reply);
		return -1;
	} else {
		return 0;
	}
}
