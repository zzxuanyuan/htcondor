/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "env.h"
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "filename_tools.h"
#include "job_lease.h"

#include "gridmanager.h"
#include "creamjob.h"
#include "condor_config.h"
#include "my_username.h"


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
#define GM_PURGE				9
#define GM_DELETE				10
#define GM_CLEAR_REQUEST		11
#define GM_HOLD					12
#define GM_PROXY_EXPIRED		13
#define GM_EXTEND_LIFETIME		14
#define GM_POLL_JOB_STATE		15
#define GM_START				16
#define GM_DELEGATE_PROXY		17

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
	"GM_PURGE",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROXY_EXPIRED",
	"GM_EXTEND_LIFETIME",
	"GM_POLL_JOB_STATE",
	"GM_START",
	"GM_DELEGATE_PROXY",
};

#define CREAM_JOB_STATE_UNSET			""
#define CREAM_JOB_STATE_REGISTERED		"Registered"
#define CREAM_JOB_STATE_PENDING			"Pending"
#define CREAM_JOB_STATE_IDLE			"Idle"
#define CREAM_JOB_STATE_RUNNING			"Running"
#define CREAM_JOB_STATE_REALLY_RUNNING	"ReallyRunning"
#define CREAM_JOB_STATE_CANCELLED		"Cancelled"
#define CREAM_JOB_STATE_HELD			"Held"
#define CREAM_JOB_STATE_ABORTED			"Aborted"
#define CREAM_JOB_STATE_DONE_OK			"DoneOk"
#define CREAM_JOB_STATE_DONE_FAILED		"DoneFailed"
#define CREAM_JOB_STATE_UNKNOWN			"Unknown"
#define CREAM_JOB_STATE_PURGED			"Purged"

const char *ATTR_CREAM_UPLOAD_URL = "CreamUploadUrl";


// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
#define JM_MIN_PROXY_TIME		(minProxy_time + 60)

#define DEFAULT_LEASE_DURATION	12*60*60

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_CREAM_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, remoteState %s: %s %s\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],remoteState.Value(), \
        func,error==GAHPCLIENT_COMMAND_TIMED_OUT?"timed out":"failed")

#define CHECK_PROXY \
{ \
	if ( PROXY_NEAR_EXPIRED( jobProxy ) && gmState != GM_PROXY_EXPIRED ) { \
		dprintf( D_ALWAYS, "(%d.%d) proxy is about to expire\n", \
				 procID.cluster, procID.proc ); \
		gmState = GM_PROXY_EXPIRED; \
		break; \
	} \
}

void CreamJobInit()
{
}

void CreamJobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	CreamJob::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	CreamResource::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	CreamJob::setGahpCallTimeout( tmp_int );
	CreamResource::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	CreamJob::setConnectFailureRetry( tmp_int );

	// Tell all the resource objects to deal with their new config values
	CreamResource *next_resource;

	CreamResource::ResourcesByName.startIterations();

	while ( CreamResource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}
}

bool CreamJobAdMatch( const ClassAd *job_ad ) {
	int universe;
	MyString resource;
	if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
		 universe == CONDOR_UNIVERSE_GRID &&
		 job_ad->LookupString( ATTR_GRID_RESOURCE, resource ) &&
		 strncasecmp( resource.Value(), "cream ", 6 ) == 0 ) {

		return true;
	}
	return false;
}

BaseJob *CreamJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new CreamJob( jobad );
}


int CreamJob::probeInterval = 300;			// default value
int CreamJob::submitInterval = 300;			// default value
int CreamJob::gahpCallTimeout = 300;		// default value
int CreamJob::maxConnectFailures = 3;		// default value

CreamJob::CreamJob( ClassAd *classad )
	: BaseJob( classad )
{
	int bool_value;
	char buff[4096];
	char buff2[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];
	bool job_already_submitted = false;
	MyString error_string = "";
	char *gahp_path = NULL;

	gahpAd = NULL;
	remoteJobId = NULL;
	remoteState = CREAM_JOB_STATE_UNSET;
	localOutput = NULL;
	localError = NULL;
	streamOutput = false;
	streamError = false;
	stageOutput = false;
	stageError = false;
	remoteStateFaultString = 0;
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentRemoteState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	jmProxyExpireTime = 0;
	jmLifetime = 0;
	connect_failure_counter = 0;
	resourceManagerString = NULL;
	myResource = NULL;
	jobProxy = NULL;
	uploadUrl = NULL;
	gahp = NULL;
	delegatedCredentialURI = NULL;
//	gridftpServer = NULL;

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	// TODO This is bad. The job may already be on hold with a valid hold
	//   reason, and here we'll clear it out (and propogate to the schedd).
	if ( jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	jobProxy = AcquireProxy( jobAd, error_string, evaluateStateTid );
	if ( jobProxy == NULL ) {
		if ( error_string == "" ) {
			error_string.sprintf( "%s is not set in the job ad",
								  ATTR_X509_USER_PROXY );
		}
		goto error_exit;
	}

	gahp_path = param("CREAM_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "CREAM_GAHP not defined";
		goto error_exit;
	}
	snprintf( buff, sizeof(buff), "CREAM/%s",
			  jobProxy->subject->subject_name );
	gahp = new GahpClient( buff, gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		const char *token;
		MyString str = buff;

		str.Tokenize();

		token = str.GetNextToken( " ", false );
		if ( !token || stricmp( token, "cream" ) ) {
			error_string.sprintf( "%s not of type cream", ATTR_GRID_RESOURCE );
			goto error_exit;
		}

			/* TODO Make port and '/ce-cream/services/CREAM' optional */
		token = str.GetNextToken( " ", false );
		if ( token && *token ) {
			// If the resource url is missing a scheme, insert one
			if ( strncmp( token, "http://", 7 ) == 0 ||
				 strncmp( token, "https://", 8 ) == 0 ) {
				resourceManagerString = strdup( token );
			} else {
				snprintf( buff2, sizeof(buff2), "https://%s", token );
				resourceManagerString = strdup( buff2 );
			}
		} else {
			error_string.sprintf( "%s missing CREAM Service URL",
								  ATTR_GRID_RESOURCE );
			goto error_exit;
		}

	} else {
		error_string.sprintf( "%s is not set in the job ad",
							  ATTR_GRID_RESOURCE );
		goto error_exit;
	}

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( buff[0] != '\0' ) {
		SetRemoteJobId( strchr( buff, ' ' ) + 1 );
		job_already_submitted = true;
	}

		// Find/create an appropriate CreamResource for this job
	myResource = CreamResource::FindOrCreateResource( resourceManagerString,
													jobProxy->subject->subject_name);
	if ( myResource == NULL ) {
		error_string = "Failed to initialize CreamResource object";
		goto error_exit;
	}

	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this );
	if ( job_already_submitted ) {
		myResource->AlreadySubmitted( this );
	}

/*
	buff[0] = '\0';
	if ( job_already_submitted ) {
		jobAd->LookupString( ATTR_GRIDFTP_URL_BASE, buff );
	}

	gridftpServer = GridftpServer::FindOrCreateServer( jobProxy );

		// TODO It would be nice to register only after going through
		//   GM_CLEAR_REQUEST, so that a ATTR_GRIDFTP_URL_BASE from a
		//   previous submission isn't requested here.
	gridftpServer->RegisterClient( evaluateStateTid, buff[0] ? buff : NULL );
*/

	if ( job_already_submitted &&
		 jobAd->LookupString( ATTR_GLOBUS_DELEGATION_URI, buff ) ) {

		delegatedCredentialURI = strdup( buff );
		myResource->registerDelegationURI( delegatedCredentialURI, jobProxy );
	}

	if ( job_already_submitted ) {
		jobAd->LookupString( ATTR_CREAM_UPLOAD_URL, &uploadUrl );
	}

	gahpErrorString = "";

	iwd[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_IWD, iwd) && *iwd ) {
		int len = strlen(iwd);
		if ( len > 1 && iwd[len - 1] != '/' ) {
			strcat( iwd, "/" );
		}
	} else {
		strcpy( iwd, "/" );
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localOutput = strdup( buff2 );

			bool_value = 1;
			jobAd->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = (bool_value != 0);
			stageOutput = !streamOutput;
		}
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( jobAd->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !jobAd->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localError = strdup( buff2 );

			bool_value = 1;
			jobAd->LookupBool( ATTR_STREAM_ERROR, bool_value );
			streamError = (bool_value != 0);
			stageError = !streamError;
		}
	}

	return;

 error_exit:
		// We must ensure that the code-path from GM_HOLD doesn't depend
		// on any initialization that's been skipped.
	gmState = GM_HOLD;
	if ( !error_string.IsEmpty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string.Value() );
	}
	return;
}

CreamJob::~CreamJob()
{
/*
	if ( gridftpServer ) {
		gridftpServer->UnregisterClient( evaluateStateTid );
	}
*/
	if ( myResource ) {
		myResource->UnregisterJob( this );
	}
	if ( resourceManagerString ) {
		free( resourceManagerString );
	}
	if ( remoteJobId ) {
		free( remoteJobId );
	}
	if ( gahpAd ) {
		delete gahpAd;
	}
	if ( localOutput ) {
		free( localOutput );
	}
	if ( localError ) {
		free( localError );
	}
	if ( jobProxy ) {
		ReleaseProxy( jobProxy, evaluateStateTid );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( delegatedCredentialURI != NULL) {
		free( delegatedCredentialURI );
	}
	if ( uploadUrl != NULL ) {
		free( uploadUrl );
	}
}

void CreamJob::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int CreamJob::doEvaluateState()
{
	bool connect_failure = false;
	int old_gm_state;
	MyString old_remote_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool done;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, creamState %s\n",
			procID.cluster,procID.proc,GMStateNames[gmState],
			remoteState.Value());

	if ( gahp ) {
		if ( !resourceStateKnown || resourcePingPending || resourceDown ) {
			gahp->setMode( GahpClient::results_only );
		} else {
			gahp->setMode( GahpClient::normal );
		}
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_remote_state = remoteState;

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the CreamJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			int err;

			if ( gahp->Initialize( jobProxy ) == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );
				
				jobAd->Assign( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setDelegProxy( jobProxy );

			GahpClient::mode saved_mode = gahp->getMode();
			gahp->setMode( GahpClient::blocking );

			gahp->setMode( saved_mode );

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
			if ( remoteJobId == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
					// TODO we should save the cream job state in the job
					//   ad and use it to set submitLogged and
					//   executeLogged here
				submitLogged = true;
				if ( condorState == RUNNING ) {
					executeLogged = true;
				}

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
				gmState = GM_DELETE;
				break;
/*
			} else if ( gridftpServer->GetErrorMessage() ) {
				errorString = gridftpServer->GetErrorMessage();
				gmState = GM_HOLD;
				break;
			} else if ( gridftpServer->GetUrlBase() ) {
				jobAd->Assign( ATTR_GRIDFTP_URL_BASE, gridftpServer->GetUrlBase() );
				gmState = GM_DELEGATE_PROXY;
*/
			}
gmState = GM_DELEGATE_PROXY;
			} break;
 		case GM_DELEGATE_PROXY: {
			const char *deleg_uri;
			const char *error_msg;
				// TODO What happens if CreamResource can't delegate proxy?
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			}
			if ( delegatedCredentialURI != NULL ) {
				gmState = GM_SUBMIT;
				break;
			}
			if ( (error_msg = myResource->getDelegationError( jobProxy )) ) {
					// There's a problem delegating the proxy
				errorString = error_msg;
				gmState = GM_HOLD;
			}
			deleg_uri = myResource->getDelegationURI( jobProxy );
			if ( deleg_uri == NULL ) {
					// proxy still needs to be delegated. Wait.
				break;
			}
			delegatedCredentialURI = strdup( deleg_uri );
			gmState = GM_SUBMIT;
			jobAd->Assign( ATTR_GLOBUS_DELEGATION_URI,
						   delegatedCredentialURI );
		} break;
		case GM_SUBMIT: {
			// Start a new cream submission for this job.
 			char *job_id = NULL;
			char *upload_url = NULL;
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit(this);
				gmState = GM_UNSUBMITTED;
				break;
			}
			if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
				if ( gahpErrorString == "" ) {
					gahpErrorString = "Attempts to submit failed";
				}
				gmState = GM_HOLD;
				break;
			}
			// After a submit, wait at least submitInterval before trying
			// another one.
			if ( now >= lastSubmitAttempt + submitInterval ) {
				CHECK_PROXY;
				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit(this) == false ) {
					break;
				}
				if ( gahpAd == NULL ) {
					gahpAd = buildSubmitAd();
				}
				if ( RSL == NULL ) {
					myResource->CancelSubmit(this);
					gmState = GM_HOLD;
					break;
				}

				rc = gahp->cream_job_register( 
										resourceManagerString,
										myResource->GetDelegationService(),
										delegatedCredentialURI,
										&job_id, &upload_url );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->SubmitComplete(this);
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				jmProxyExpireTime = jobProxy->expiration_time;
				if ( rc == GLOBUS_SUCCESS ) {
					SetRemoteJobId( job_id );
					free( job_id );
					uploadUrl = upload_url;
					jobAd->Assign( ATTR_CREAM_UPLOAD_URL, uploadUrl );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_register()", rc );
					dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
							procID.cluster, procID.proc,RSL->Value());
					gahpErrorString = gahp->getErrorString();
					myResource->CancelSubmit( this );
					gmState = GM_UNSUBMITTED;
					reevaluate_state = true;
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
			// Save the jobmanager's contact for a new gram submission.
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
			// Now that we've saved the job id, tell cream it can start
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				rc = gahp->cream_job_start( resourceManagerString,
											remoteJobId );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_start()", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
				} else {
						// We don't want an old or zeroed lastProbeTime
						// make us do a probe immediately after submitting
						// the job, so set it to now
					lastProbeTime = time(NULL);
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
			if ( remoteState == CREAM_JOB_STATE_DONE_OK ) {
				gmState = GM_DONE_SAVE;
			} else if ( remoteState == CREAM_JOB_STATE_DONE_FAILED ) {
				gmState = GM_PURGE;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
/*
					// Check that our gridftp server is healthy
				if ( gridftpServer->GetErrorMessage() ) {
					errorString = gridftpServer->GetErrorMessage();
					gmState = GM_HOLD;
					break;
				}
				MyString url_base;
				jobAd->LookupString( ATTR_GRIDFTP_URL_BASE, url_base );
				if ( gridftpServer->GetUrlBase() &&
					 strcmp( url_base.Value(),
							 gridftpServer->GetUrlBase() ) ) {
					gmState = GM_CANCEL;
					break;
				}
*/

/*
				int new_lease;	// CalculateJobLease needs an int
				if ( CalculateJobLease( jobAd, new_lease,
										DEFAULT_LEASE_DURATION ) ) {
					jmLifetime = new_lease;
					gmState = GM_EXTEND_LIFETIME;
					break;
				}
*/
				if ( probeNow || remoteState == CREAM_JOB_STATE_UNSET ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				if ( now >= lastProbeTime + probeInterval ) {
					gmState = GM_POLL_JOB_STATE;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_EXTEND_LIFETIME: {
/*
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				time_t new_lifetime = jmLifetime - now;
				rc = gahp->gt4_set_termination_time( jobContact,
													 new_lifetime );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_CREAM_ERROR("refresh_credentials()",rc);
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					break;
				}
				jmLifetime = new_lifetime;
				UpdateJobLeaseSent( jmLifetime );
				gmState = GM_SUBMITTED;
			}
*/
			} break;
		case GM_POLL_JOB_STATE: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				char *status = NULL;
				char *fault = NULL;
				int exit_code = -1;
				CHECK_PROXY;
				rc = gahp->cream_job_status( resourceManagerString,
											 remoteJobId, &status,
											 &exit_code, &fault );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_status()", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CANCEL;
					if ( status ) {
						free( status );
					}
					break;
				}
				SetRemoteJobState( status, exit_code, fault );
				if ( status ) {
					free( status );
				}
				if ( fault ) {
					free( fault );
				}
				lastProbeTime = time(NULL);
				gmState = GM_SUBMITTED;
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
			// Tell the jobmanager it can clean up and exit.
			CHECK_PROXY;
			rc = gahp->cream_job_purge( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_CREAM_ERROR( "cream_job_purge()", rc );
				gahpErrorString = gahp->getErrorString();
				gmState = GM_CANCEL;
				break;
			}
			myResource->CancelSubmit( this );
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				SetRemoteJobId( NULL );
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				if ( jobContact != NULL ) {
					SetRemoteJobId( NULL );
					remoteState = CREAM_JOB_STATE_UNSET;
					requestScheddUpdate( this );
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( remoteState != CREAM_JOB_STATE_ABORTED &&
				 remoteState != CREAM_JOB_STATE_CANCELLED &&
				 remoteState != CREAM_JOB_STATE_DONE_OK &&
				 remoteState != CREAM_JOB_STATE_DONE_FAILED ) {
				CHECK_PROXY;
				rc = gahp->cream_job_cancel( resourceManagerString,
											 remoteJobId );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_CREAM_ERROR( "cream_job_cancel()", rc );
					gahpErrorString = gahp->getErrorString();
					gmState = GM_CLEAR_REQUEST;
					break;
				}
				myResource->CancelSubmit( this );
				SetRemoteJobId( NULL );
			}
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_PURGE: {
			// The cream server's job state is in a terminal (failed)
			// state. Send a purge command to tell the server it can
			// delete the job from its logs.

			CHECK_PROXY;
			rc = gahp->cream_job_purge( resourceManagerString, remoteJobId );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
				LOG_CREAM_ERROR( "cream_job_purge", rc );
				gahpErrorString = gahp->getErrorString();
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			SetRemoteJobId( NULL );
			myResource->CancelSubmit( this );
			remoteState = CREAM_JOB_STATE_UNSET;
			requestScheddUpdate( this );

			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				//gmState = GM_CLEAR_REQUEST;
				gmState= GM_HOLD;
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

			// If we are doing a rematch, we are simply waiting around
			// for the schedd to be updated and subsequently this cream job
			// object to be destroyed.  So there is nothing to do.
			if ( wantRematch ) {
				break;
			}

			// For now, put problem jobs on hold instead of
			// forgetting about current submission and trying again.
			// TODO: Let our action here be dictated by the user preference
			// expressed in the job ad.
			if ( ( remoteJobId != "" ||
				   remoteState == CREAM_JOB_STATE_ABORTED ||
				   remoteState == CREAM_JOB_STATE_DONE_FAILED ) 
				     && condorState != REMOVED 
					 && wantResubmit == 0 
					 && doResubmit == 0 ) {
				gmState = GM_HOLD;
				break;
			}
			// Only allow a rematch *if* we are also going to perform a resubmit
			if ( wantResubmit || doResubmit ) {
				jobAd->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
			}
			if ( wantResubmit ) {
				wantResubmit = 0;
				dprintf(D_ALWAYS,
						"(%d.%d) Resubmitting to CREAM because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
			}
			if ( doResubmit ) {
				doResubmit = 0;
				dprintf(D_ALWAYS,
					"(%d.%d) Resubmitting to CREAM (last submit failed)\n",
						procID.cluster, procID.proc );
			}
			remoteState = CREAM_JOB_STATE_UNSET;
			remoteStateFaultString = "";
			gahpErrorString = "";
			errorString = "";
			jmLifetime = 0;
			UpdateJobLeaseSent( -1 );
			myResource->CancelSubmit( this );
			if ( remoteJobId != NULL ) {
				SetRemoteJobId( NULL );
			}
			JobIdle();
			if ( submitLogged ) {
				JobEvicted();
				if ( !evictLogged ) {
					WriteEvictEventToUserLog( jobAd );
					evictLogged = true;
				}
			}
			MyString val;
			if ( jobAd->LookupString( ATTR_GRIDFTP_URL_BASE, val ) ) {
				jobAd->AssignExpr( ATTR_GRIDFTP_URL_BASE, "Undefined" );
			}
			
			if ( wantRematch ) {
				dprintf(D_ALWAYS,
						"(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_REMATCH_CHECK );

				// Set ad attributes so the schedd finds a new match.
				int dummy;
				if ( jobAd->LookupBool( ATTR_JOB_MATCHED, dummy ) != 0 ) {
					jobAd->Assign( ATTR_JOB_MATCHED, false );
					jobAd->Assign( ATTR_CURRENT_HOSTS, 0 );
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
				jobAd->LookupString( ATTR_HOLD_REASON, holdReason,
									 sizeof(holdReason) - 1 );
				if ( holdReason[0] == '\0' && errorString != "" ) {
					strncpy( holdReason, errorString.Value(),
							 sizeof(holdReason) - 1 );
				}
				if ( holdReason[0] == '\0' &&
					 !remoteStateFaultString.IsEmpty() ) {

					snprintf( holdReason, 1024, "CREAM error: %s",
							  remoteStateFaultString.Value() );
				}
				if ( holdReason[0] == '\0' && !gahpErrorString.IsEmpty() ) {
					snprintf( holdReason, 1024, "CREAM error: %s",
							  gahpErrorString.Value() );
				}
				if ( holdReason[0] == '\0' ) {
					strncpy( holdReason, "Unspecified gridmanager error",
							 sizeof(holdReason) - 1 );
				}

				JobHeld( holdReason );
			}
			gmState = GM_DELETE;
			} break;
		case GM_PROXY_EXPIRED: {
			// The proxy for this job is either expired or about to expire.
			// If requested, put the job on hold. Otherwise, wait for the
			// proxy to be refreshed, then resume handling the job.
			if ( jobProxy->expiration_time > JM_MIN_PROXY_TIME + now ) {
				gmState = GM_START;
			} else {
				// Do nothing. Our proxy is about to expire.
			}
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
//					old_remote_state.Value()),
//					remoteState.Value());
			enteredCurrentRemoteState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending gahp call, we're not
			// anymore so purge it.
			if ( gahp ) {
				gahp->purgePendingRequests();
			}
			// If we were calling a gahp call that usedgahpAd, we're done
			// with it now, so free it.
			if ( gahpAd ) {
				delete gahpAd;
				gahpAd = NULL;
			}
			connect_failure_counter = 0;
		}

	} while ( reevaluate_state );

	if ( connect_failure && !resourceDown ) {
		if ( connect_failure_counter < maxConnectFailures ) {
				// We are seeing a lot of failures to connect
				// with Globus 2.2 libraries, often due to GSI not able 
				// to authenticate.
			connect_failure_counter++;
			int retry_secs = param_integer(
				"GRIDMANAGER_CONNECT_FAILURE_RETRY_INTERVAL",5);
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure (try #%d), retrying in %d secs\n",
				procID.cluster,procID.proc,connect_failure_counter,retry_secs);
			daemonCore->Reset_Timer( evaluateStateTid, retry_secs );
		} else {
			dprintf(D_FULLDEBUG,
				"(%d.%d) Connection failure, requesting a ping of the resource\n",
				procID.cluster,procID.proc);
			RequestPing();
		}
	}

	return TRUE;
}

BaseResource *CreamJob::GetResource()
{
	return (BaseResource *)myResource;
}

void CreamJob::SetRemoteJobId( const char *job_id )
{
	free( remoteJobId );
	if ( job_id ) {
		remoteJobId = strdup( job_id );
	} else {
		remoteJobId = NULL;
	}

	MyString full_job_id;
	if ( job_id ) {
		full_job_id.sprintf( "cream %s %s", resourceManagerString, job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}

void CreamJob::SetRemoteJobState( const char *new_state, int exit_code,
								  const char *failure_reason )
{
	MyString new_state_str = new_state;

		// TODO verify that the string is a valid state name

	if ( new_state_str != remoteState ) {
		dprintf( D_FULLDEBUG, "(%d.%d) cream state change: %s -> %s\n",
				 procID.cluster, procID.proc, remoteState->Value(),
				 new_state_str.Value() );

		if ( ( new_state_str == CREAM_JOB_STATE_RUNNING ||
			   new_state_str == CREAM_JOB_STATE_REALLY_RUNNING ) &&
			 condorState == IDLE ) {
			JobRunning();
		}

		if ( new_state_str == CREAM_JOB_STATE_HELD &&
			 condorState == RUNNING ) {
			JobIdle();
		}

		// TODO When do we consider the submission successful or not:
		//   when Register works, when Start() works, or when the job
		//   state moves to IDLE?
		if ( remoteState == CREAM_JOB_STATE_UNSET &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state_str != CREAM_JOB_STATE_ABORTED ) {
					// The request was successfuly submitted. Write it to
					// the user-log
				if ( !submitLogged ) {
					WriteGridSubmitEventToUserLog( jobAd );
					submitLogged = true;
				}
			}
		}

		remoteState = new_state_str;
		enteredCurrentRemoteState = time(NULL);

		// TODO handle jobs that exit via a signal
		if ( remoteState == CREAM_JOB_STATE_DONE_OK ) {
			jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, false );
			jobAd->Assign( ATTR_ON_EXIT_CODE, exit_code );
		}

		requestScheddUpdate( this );

		SetEvaluateState();
	}
}

// Build submit classad
ClassAd *CreamJob::buildSubmitAd()
{
}

