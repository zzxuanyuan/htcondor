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
#include "gt4job.h"
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
#define GM_PROBE_JOBMANAGER		17
#define GM_START				18
#define GM_GENERATE_ID			19
#define GM_DELEGATE_PROXY		20
#define GM_DELEGATE_PROXY_SAVE	21
#define GM_SUBMIT_ID_SAVE		22

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
	"GM_PROBE_JOBMANAGER",
	"GM_START",
	"GM_GENERATE_ID",
	"GM_DELEGATE_PROXY",
	"GM_DELEGATE_PROXY_SAVE",
	"GM_SUBMIT_ID_SAVE"
};

// TODO: once we can set the jobmanager's proxy timeout, we should either
// let this be set in the config file or set it to
// GRIDMANAGER_MINIMUM_PROXY_TIME + 60
#define JM_MIN_PROXY_TIME		(minProxy_time + 60)

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

#define LOG_GLOBUS_ERROR(func,error) \
    dprintf(D_ALWAYS, \
		"(%d.%d) gmState %s, globusState %d: %s returned Globus error %d\n", \
        procID.cluster,procID.proc,GMStateNames[gmState],globusState, \
        func,error)

#define CHECK_PROXY \
{ \
	if ( PROXY_NEAR_EXPIRED( myProxy ) && gmState != GM_PROXY_EXPIRED ) { \
		dprintf( D_ALWAYS, "(%d.%d) proxy is about to expire\n", \
				 procID.cluster, procID.proc ); \
		gmState = GM_PROXY_EXPIRED; \
		break; \
	} \
}

//////////////////////from gridmanager.C
#define HASH_TABLE_SIZE			500

static bool WriteGT4SubmitEventToUserLog( ClassAd *job_ad );
static bool WriteGT4SubmitFailedEventToUserLog( ClassAd *job_ad,
												int failure_code );
static bool WriteGT4ResourceUpEventToUserLog( ClassAd *job_ad );
static bool WriteGT4ResourceDownEventToUserLog( ClassAd *job_ad );

template class HashTable<HashKey, GT4Job *>;
template class HashBucket<HashKey, GT4Job *>;

// TODO need to get rid of this
static GahpClient GahpMain;

HashTable <HashKey, GT4Job *> GT4JobsByContact( HASH_TABLE_SIZE,
												hashFunction );

const char *
gt4JobId( const char *contact )
{
/*
	static char buff[1024];
	char *first_end;
	char *second_begin;

	ASSERT( strlen(contact) < sizeof(buff) );

	first_end = strrchr( contact, ':' );
	ASSERT( first_end );

	second_begin = strchr( first_end, '/' );
	ASSERT( second_begin );

	strncpy( buff, contact, first_end - contact );
	strcpy( buff + ( first_end - contact ), second_begin );

	return buff;
*/
	return contact;
}

static
void
rehashJobContact( GT4Job *job, const char *old_contact,
				  const char *new_contact )
{
	if ( old_contact ) {
		GT4JobsByContact.remove(HashKey(gt4JobId(old_contact)));
	}
	if ( new_contact ) {
		GT4JobsByContact.insert(HashKey(gt4JobId(new_contact)), job);
	}
}

void
gt4GramCallbackHandler( void *user_arg, char *job_contact, int state,
					 int errorcode )
{
	int rc;
	GT4Job *this_job;

	// Find the right job object
	rc = GT4JobsByContact.lookup( HashKey( gt4JobId(job_contact) ), this_job );
	if ( rc != 0 || this_job == NULL ) {
		dprintf( D_ALWAYS, 
			"gt4GramCallbackHandler: Can't find record for globus job with "
			"contact %s on globus state %d, errorcode %d, ignoring\n",
			job_contact, state, errorcode );
		return;
	}

	dprintf( D_ALWAYS, "(%d.%d) gram callback: state %d, errorcode %d\n",
			 this_job->procID.cluster, this_job->procID.proc, state,
			 errorcode );

	this_job->GramCallback( state, errorcode );
}

/////////////////////////added for reorg
void GT4JobInit()
{
}

void GT4JobReconfig()
{
	int tmp_int;

	tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 60 );
	GT4Job::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	GT4Resource::setProbeInterval( tmp_int );

	tmp_int = param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 );
	GT4Job::setGahpCallTimeout( tmp_int );
	GT4Resource::setGahpCallTimeout( tmp_int );

	tmp_int = param_integer("GRIDMANAGER_CONNECT_FAILURE_RETRY_COUNT",3);
	GT4Job::setConnectFailureRetry( tmp_int );

	// Tell all the resource objects to deal with their new config values
	GT4Resource *next_resource;

	GT4ResourcesByName.startIterations();

	while ( GT4ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}
}

const char *GT4JobAdConst = "JobUniverse =?= 9 && (JobGridType == \"gt4\") =?= True";

bool GT4JobAdMustExpand( const ClassAd *jobad )
{
	int must_expand = 0;

	jobad->LookupBool(ATTR_JOB_MUST_EXPAND, must_expand);
	if ( !must_expand ) {
		char resource_name[800];
		jobad->LookupString(ATTR_GLOBUS_RESOURCE, resource_name);
		if ( strstr(resource_name,"$$") ) {
			must_expand = 1;
		}
	}

	return must_expand != 0;
}

BaseJob *GT4JobCreate( ClassAd *jobad )
{
	return (BaseJob *)new GT4Job( jobad );
}
////////////////////////////////////////
/*
static
const char *rsl_stringify( const MyString& src )
{
	int src_len = src.Length();
	int src_pos = 0;
	int var_pos1;
	int var_pos2;
	int quote_pos;
	static MyString dst;

	if ( src_len == 0 ) {
		dst = "''";
	} else {
		dst = "";
	}

	while ( src_pos < src_len ) {
		var_pos1 = src.find( "$(", src_pos );
		var_pos2 = var_pos1 == -1 ? -1 : src.find( ")", var_pos1 );
		quote_pos = src.find( "'", src_pos );
		if ( var_pos2 == -1 && quote_pos == -1 ) {
			dst += "'";
			dst += src.Substr( src_pos, src.Length() - 1 );
			dst += "'";
			src_pos = src.Length();
		} else if ( var_pos2 == -1 ||
					(quote_pos != -1 && quote_pos < var_pos1 ) ) {
			if ( src_pos < quote_pos ) {
				dst += "'";
				dst += src.Substr( src_pos, quote_pos - 1 );
				dst += "'#";
			}
			dst += '"';
			while ( src[quote_pos] == '\'' ) {
				dst += "'";
				quote_pos++;
			}
			dst += '"';
			if ( quote_pos < src_len ) {
				dst += '#';
			}
			src_pos = quote_pos;
		} else {
			if ( src_pos < var_pos1 ) {
				dst += "'";
				dst += src.Substr( src_pos, var_pos1 - 1 );
				dst += "'#";
			}
			dst += src.Substr( var_pos1, var_pos2 );
			if ( var_pos2 + 1 < src_len ) {
				dst += '#';
			}
			src_pos = var_pos2 + 1;
		}
	}

	return dst.Value();
}

static
const char *rsl_stringify( const char *string )
{
	static MyString src;
	src = string;
	return rsl_stringify( src );
}
*/
int GT4Job::probeInterval = 300;			// default value
int GT4Job::submitInterval = 300;			// default value
int GT4Job::restartInterval = 60;			// default value
int GT4Job::gahpCallTimeout = 300;			// default value
int GT4Job::maxConnectFailures = 3;			// default value
int GT4Job::outputWaitGrowthTimeout = 15;	// default value

GT4Job::GT4Job( ClassAd *classad )
	: BaseJob( classad )
{
	int bool_value;
	char buff[4096];
	char buff2[_POSIX_PATH_MAX];
	char iwd[_POSIX_PATH_MAX];
	bool job_already_submitted = false;
	char *error_string = NULL;

	RSL = NULL;
	callbackRegistered = false;
	jobContact = NULL;
	localOutput = NULL;
	localError = NULL;
	streamOutput = false;
	streamError = false;
	stageOutput = false;
	stageError = false;
	globusStateErrorCode = 0;
	globusStateBeforeFailure = 0;
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
	restartingJM = false;
	restartWhen = 0;
	gmState = GM_INIT;
	globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
	resourcePingPending = false;
	jmUnreachable = false;
	jmDown = false;
	lastProbeTime = 0;
	probeNow = false;
	enteredCurrentGmState = time(NULL);
	enteredCurrentGlobusState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	submitFailureCode = 0;
	lastRestartReason = 0;
	lastRestartAttempt = 0;
	numRestartAttempts = 0;
	numRestartAttemptsThisSubmit = 0;
	jmProxyExpireTime = 0;
	connect_failure_counter = 0;
	outputWaitLastGrowth = 0;
	// HACK!
	retryStdioSize = true;
	resourceManagerString = NULL;
	jobmanagerType = NULL;
	myResource = NULL;
	myProxy = NULL;
	gassServerUrl = NULL;
	gramCallbackContact = NULL;
	gahp = NULL;
	submit_id = NULL;
	delegatedCredentialURI = NULL;

	// In GM_HOLD, we assme HoldReason to be set only if we set it, so make
	// sure it's unset when we start.
	if ( ad->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		UpdateJobAd( ATTR_HOLD_REASON, "UNDEFINED" );
	}

	char *gahp_path = param("GT4_GAHP");
	if ( gahp_path == NULL ) {
		error_string = "GT4_GAHP not defined";
		goto error_exit;
	}
	gahp = new GahpClient( "GT4", gahp_path );
	free( gahp_path );

	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	buff[0] = '\0';
	ad->LookupString( ATTR_X509_USER_PROXY, buff );
	if ( buff[0] != '\0' ) {
		myProxy = AcquireProxy( buff, evaluateStateTid );
		if ( myProxy == NULL ) {
			dprintf( D_ALWAYS, "(%d.%d) error acquiring proxy!\n",
					 procID.cluster, procID.proc );
		}
	} else {
		dprintf( D_ALWAYS, "(%d.%d) %s not set in job ad!\n",
				 procID.cluster, procID.proc, ATTR_X509_USER_PROXY );
	}

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBUS_RESOURCE, buff );
	if ( buff[0] != '\0' ) {
		resourceManagerString = strdup( buff );
	} else {
		error_string = "GT4Resource is not set in the job ad";
		goto error_exit;
	}

////////////////from gridmanager.C
{
	const char *canonical_name = GT4Resource::CanonicalName( resourceManagerString );
	int rc;
	ASSERT(canonical_name);
	rc = GT4ResourcesByName.lookup( HashKey( canonical_name ),
								 myResource );

	if ( rc != 0 ) {
		myResource = new GT4Resource( canonical_name );
		ASSERT(myResource);
		GT4ResourcesByName.insert( HashKey( canonical_name ),
								myResource );
	} else {
		ASSERT(myResource);
	}
}
//////////////////////////////////

	resourceDown = false;
	resourceStateKnown = false;
//	myResource = resource;
	// RegisterJob() may call our NotifyResourceUp/Down(), so be careful.
	myResource->RegisterJob( this, job_already_submitted );

	buff[0] = '\0';
	ad->LookupString( ATTR_GLOBUS_CONTACT_STRING, buff );
	if ( buff[0] != '\0' && strcmp( buff, NULL_JOB_CONTACT ) != 0 ) {
		rehashJobContact( this, jobContact, buff );
		jobContact = strdup( buff );
		job_already_submitted = true;
	}

	if (ad->LookupString ( ATTR_GLOBUS_JOBMANAGER_TYPE, buff )) {
		jobmanagerType = strdup( buff );
	}

	if (ad->LookupString ( ATTR_GLOBUS_SUBMIT_ID, buff )) {
		submit_id = strdup ( buff );
	}


	useGridJobMonitor = true;

	ad->LookupInteger( ATTR_GLOBUS_STATUS, globusState );

	globusError = GLOBUS_SUCCESS;

	iwd[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_IWD, iwd) && *iwd ) {
		int len = strlen(iwd);
		if ( len > 1 && iwd[len - 1] != '/' ) {
			strcat( iwd, "/" );
		}
	} else {
		strcpy( iwd, "/" );
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_OUTPUT, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !ad->LookupBool( ATTR_TRANSFER_OUTPUT, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localOutput = strdup( buff2 );

			bool_value = 1;
			ad->LookupBool( ATTR_STREAM_OUTPUT, bool_value );
			streamOutput = (bool_value != 0);
			stageOutput = !streamOutput;
		}
	}

	buff[0] = '\0';
	buff2[0] = '\0';
	if ( ad->LookupString(ATTR_JOB_ERROR, buff) && *buff &&
		 strcmp( buff, NULL_FILE ) ) {

		if ( !ad->LookupBool( ATTR_TRANSFER_ERROR, bool_value ) ||
			 bool_value ) {

			if ( buff[0] != '/' ) {
				strcat( buff2, iwd );
			}

			strcat( buff2, buff );
			localError = strdup( buff2 );

			bool_value = 1;
			ad->LookupBool( ATTR_STREAM_ERROR, bool_value );
			streamError = (bool_value != 0);
			stageError = !streamError;
		}
	}

	return;

 error_exit:
		// We must ensure that the code-path from GM_HOLD doesn't depend
		// on any initialization that's been skipped.
	gmState = GM_HOLD;
	if ( error_string ) {
		UpdateJobAdString( ATTR_HOLD_REASON, error_string );
	}
	return;
}

GT4Job::~GT4Job()
{
	if ( myResource ) {
		myResource->UnregisterJob( this );
		// Should the GT4Resource be responsible for doing this?...
		if ( myResource->IsEmpty() ) {
			GT4ResourcesByName.remove( HashKey( myResource->ResourceName() ) );
			delete myResource;
		}
	}
	if ( resourceManagerString ) {
		free( resourceManagerString );
	}
	if ( jobContact ) {
		rehashJobContact( this, jobContact, NULL );
		free( jobContact );
	}
	if ( RSL ) {
		delete RSL;
	}
	if ( localOutput ) {
		free( localOutput );
	}
	if ( localError ) {
		free( localError );
	}
	if ( myProxy ) {
		ReleaseProxy( myProxy, evaluateStateTid );
	}
	if ( gassServerUrl ) {
		free( gassServerUrl );
	}
	if ( gramCallbackContact ) {
		free( gramCallbackContact );
	}
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( submit_id != NULL ) {
		free( submit_id );
	}
	if ( jobmanagerType != NULL ) {
		free( jobmanagerType );
	}
	if ( delegatedCredentialURI != NULL) {
		free( delegatedCredentialURI );
	}

}

void GT4Job::Reconfig()
{
	BaseJob::Reconfig();
	gahp->setTimeout( gahpCallTimeout );
}

int GT4Job::doEvaluateState()
{
	bool connect_failure_jobmanager = false;
	bool connect_failure_gatekeeper = false;
	int old_gm_state;
	int old_globus_state;
	bool reevaluate_state = true;
	time_t now;	// make sure you set this before every use!!!

	bool done;
	int rc;
	int status;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS,
			"(%d.%d) doEvaluateState called: gmState %s, globusState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],globusState);

	if ( gahp ) {
		// We don't include jmDown here because we don't want it to block
		// connections to the gatekeeper (particularly restarts) and any
		// state that contacts to the jobmanager should be jumping to
		// GM_RESTART instead.
		if ( !resourceStateKnown || resourcePingPending || resourceDown ) {
			gahp->setMode( GahpClient::results_only );
		} else {
			gahp->setMode( GahpClient::normal );
		}
	}

	do {
		reevaluate_state = false;
		old_gm_state = gmState;
		old_globus_state = globusState;

		switch ( gmState ) {
		case GM_INIT: {
			// This is the state all jobs start in when the GlobusJob object
			// is first created. Here, we do things that we didn't want to
			// do in the constructor because they could block (the
			// constructor is called while we're connected to the schedd).
			int err;

			if ( gahp->Initialize( myProxy ) == false ) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );
				
				UpdateJobAdString( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setDelegProxy( myProxy );

			gahp->setMode( GahpClient::blocking );

			err = gahp->gt4_gram_client_callback_allow( gt4GramCallbackHandler,
														NULL,
														&gramCallbackContact );
			if ( err != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS,
						 "(%d.%d) Error enabling GRAM callback, err=%d\n", 
						 procID.cluster, procID.proc, err );
				UpdateJobAdString( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			err = gahp->globus_gass_server_superez_init( &gassServerUrl, 0 );
			if ( err != GLOBUS_SUCCESS ) {
				dprintf( D_ALWAYS, "(%d.%d) Error enabling GASS server, err=%d\n",
						 procID.cluster, procID.proc, err );
				UpdateJobAdString( ATTR_HOLD_REASON, "Failed to initialize GAHP" );
				gmState = GM_HOLD;
				break;
			}

			gahp->setMode( GahpClient::normal );

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
			if ( jobContact == NULL ) {
				gmState = GM_CLEAR_REQUEST;
			} else if ( wantResubmit || doResubmit ) {
				gmState = GM_CLEAR_REQUEST;
			} else {
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					submitLogged = true;
				}
				if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
					 globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
					executeLogged = true;
				}

				gmState = GM_REGISTER;
			}
			} break;
		case GM_REGISTER: {
			// Register for callbacks from an already-running jobmanager.
			CHECK_PROXY;
			rc = gahp->gt4_gram_client_job_callback_register( jobContact,
														gramCallbackContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_callback_register()", rc );
				globusError = rc;
				gmState = GM_CANCEL;
				break;
			}
				// Now handle the case of we got GLOBUS_SUCCESS...
			callbackRegistered = true;
			probeNow = true;
			//gmState = GM_STDIO_UPDATE;
//			gmState = GM_CANCEL;
			gmState = GM_SUBMITTED;
			} break;
		case GM_STDIO_UPDATE: {
/*
			// Update an already-running jobmanager to send its I/O to us
			// instead a previous incarnation.
			CHECK_PROXY;
			if ( RSL == NULL ) {
				RSL = buildStdioUpdateRSL();
			}
			rc = gahp->globus_gram_client_job_signal( jobContact,
								GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE,
								RSL->Value(), &status, &error );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ||
				 rc == GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION ||
				 rc == GAHPCLIENT_COMMAND_TIMED_OUT ) {
				connect_failure_jobmanager = true;
				break;
			}
			if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL ) {
				// the job completed or failed while we were not around -- now
				// the jobmanager is sitting in a state where all it will permit
				// is a status query or a commit to exit.  switch into 
				// GM_SUBMITTED state and do an immediate probe to figure out
				// if the state is done or failed, and move on from there.
				probeNow = true;
				gmState = GM_SUBMITTED;
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(STDIO_UPDATE)", rc );
				globusError = rc;
				gmState = GM_STOP_AND_RESTART;
				break;
			}
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				gmState = GM_SUBMIT_COMMIT;
			} else {
				gmState = GM_SUBMITTED;
			}
*/
			gmState = GM_CANCEL;
			} break;
 		case GM_UNSUBMITTED: {
			// There are no outstanding gram submissions for this job (if
			// there is one, we've given up on it).
			if ( condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else if ( condorState == HELD ) {
				gmState = GM_DELETE;
				break;
			} else {
				gmState = GM_GENERATE_ID;
			}
			} break;
 		case GM_DELEGATE_PROXY: {
/* Don't worry about delegating for now
			rc = gahp->gt4_gram_client_delegate_credentials (resourceManagerString,
															 &delegatedCredentialURI);

			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}

			if ( rc == 0 ) {
				UpdateJobAdString ( ATTR_GLOBUS_DELEGATION_URI,
									delegatedCredentialURI );
				gmState = GM_DELEGATE_PROXY_SAVE;
			} else {
				dprintf(D_ALWAYS,"(%d.%d) Delegation Error (rc=%d): %s\n",
						procID.cluster, procID.proc, rc,
						gahp->getErrorString());

				UpdateJobAdString( ATTR_HOLD_REASON, "Failed to delegate credential" );
				gmState = GM_HOLD;
			}
*/
gmState=GM_DELEGATE_PROXY_SAVE;
			} break;
		case GM_DELEGATE_PROXY_SAVE: {
				// Save the delegation URI
/* Don't worry about delegation for now
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_SUBMIT;
			}
*/
gmState=GM_SUBMIT;
		} break;
		case GM_GENERATE_ID: {

			if (submit_id) {
				gmState = GM_SUBMIT_ID_SAVE;
				break;
			}

			if (!gahp->gt4_generate_submit_id (&submit_id)) {
				dprintf( D_ALWAYS, "(%d.%d) Error initializing GAHP\n",
						 procID.cluster, procID.proc );
				
				UpdateJobAdString( ATTR_HOLD_REASON, "Failed to generate submit ID" );
				gmState = GM_HOLD;
				break;
			} else {
				UpdateJobAdString( ATTR_GLOBUS_SUBMIT_ID, submit_id );
				gmState = GM_SUBMIT_ID_SAVE;
			}
		} break;
		case GM_SUBMIT_ID_SAVE: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				done = requestScheddUpdate( this );
				if ( !done ) {
					break;
				}
				gmState = GM_DELEGATE_PROXY;
			}
		} break;
		case GM_SUBMIT: {
			// Start a new gram submission for this job.
 			char *job_contact = NULL;
			if ( condorState == REMOVED || condorState == HELD ) {
				myResource->CancelSubmit(this);
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
				CHECK_PROXY;
				// Once RequestSubmit() is called at least once, you must
				// CancelRequest() once you're done with the request call
				if ( myResource->RequestSubmit(this) == false ) {
					break;
				}
				if ( RSL == NULL ) {
					RSL = buildSubmitRSL();
				}
				if ( RSL == NULL ) {
					gmState = GM_HOLD;
					break;
				}
				
				if (!jobmanagerType) {
					jobmanagerType = strdup ( "Fork" );
				}

				rc = gahp->gt4_gram_client_job_create( 
													  submit_id,
										resourceManagerString,
										jobmanagerType,
										gramCallbackContact,
										RSL->Value(),
										gassServerUrl,
										&job_contact );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				myResource->SubmitComplete(this);
				lastSubmitAttempt = time(NULL);
				numSubmitAttempts++;
				jmProxyExpireTime = myProxy->expiration_time;
				if ( rc == GLOBUS_SUCCESS ) {
					callbackRegistered = true;
					rehashJobContact( this, jobContact, job_contact );
						// job_contact was strdup()ed for us. Now we take
						// responsibility for free()ing it.
					jobContact = job_contact;
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   job_contact );
					gmState = GM_SUBMIT_SAVE;
				} else {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_create()", rc );
					dprintf(D_ALWAYS,"(%d.%d)    RSL='%s'\n",
							procID.cluster, procID.proc,RSL->Value());
					submitFailureCode = globusError = rc;
					WriteGT4SubmitFailedEventToUserLog( ad,
														submitFailureCode );
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
			// Now that we've saved the jobmanager's contact, commit the
			// gram job submission.
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				rc = gahp->gt4_gram_client_job_start( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_start()", rc );
					globusError = rc;
					WriteGT4SubmitFailedEventToUserLog( ad, globusError );
					gmState = GM_CANCEL;
				} else {
					gmState = GM_SUBMITTED;
				}
			}
			} break;
		case GM_SUBMITTED: {
			// The job has been submitted (or is about to be by the
			// jobmanager). Wait for completion or failure, and probe the
			// jobmanager occassionally to make it's still alive.
			if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ) {
				gmState = GM_DONE_SAVE;
			} else if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				gmState = GM_FAILED;
			} else if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				if ( GetCallbacks() == true ) {
					reevaluate_state = true;
					break;
				}
/* Don't worry about delegation for now
				if ( jmProxyExpireTime < myProxy->expiration_time ) {
					gmState = GM_REFRESH_PROXY;
					break;
				}
*/
				now = time(NULL);
				if ( lastProbeTime < enteredCurrentGmState ) {
					lastProbeTime = enteredCurrentGmState;
				}
				if ( probeNow ) {
					lastProbeTime = 0;
					probeNow = false;
				}
				if ( now >= lastProbeTime + probeInterval ) {
					gmState = GM_PROBE_JOBMANAGER;
					break;
				}
				unsigned int delay = 0;
				if ( (lastProbeTime + probeInterval) > now ) {
					delay = (lastProbeTime + probeInterval) - now;
				}				
				daemonCore->Reset_Timer( evaluateStateTid, delay );
			}
			} break;
		case GM_REFRESH_PROXY: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				rc = gahp->gt4_gram_client_refresh_credentials(
																jobContact );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR("refresh_credentials()",rc);
					globusError = rc;
					gmState = GM_CANCEL;
					break;
				}
				jmProxyExpireTime = myProxy->expiration_time;
				gmState = GM_SUBMITTED;
			}
			} break;
		case GM_PROBE_JOBMANAGER: {
			if ( condorState == REMOVED || condorState == HELD ) {
				gmState = GM_CANCEL;
			} else {
				CHECK_PROXY;
				rc = gahp->gt4_gram_client_job_status( jobContact,
													  &status );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "gt4_gram_client_job_status()", rc );
					globusError = rc;
					gmState = GM_CANCEL;
					break;
				}
				UpdateGlobusState( status, 0 );
				ClearCallbacks();
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
			rc = gahp->gt4_gram_client_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
				// unhandled error
				LOG_GLOBUS_ERROR( "gt4_gram_client_job_destroy()", rc );
				globusError = rc;
				gmState = GM_CANCEL;
				break;
			}
			if ( condorState == COMPLETED || condorState == REMOVED ) {
				gmState = GM_DELETE;
			} else {
				// Clear the contact string here because it may not get
				// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
				if ( jobContact != NULL ) {
					rehashJobContact( this, jobContact, NULL );
					free( jobContact );
					myResource->CancelSubmit( this );
					jobContact = NULL;
					UpdateJobAdString( ATTR_GLOBUS_CONTACT_STRING,
									   NULL_JOB_CONTACT );
					requestScheddUpdate( this );
					jmDown = false;
				}
				gmState = GM_CLEAR_REQUEST;
			}
			} break;
		case GM_CANCEL: {
			// We need to cancel the job submission.
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
				CHECK_PROXY;
				rc = gahp->gt4_gram_client_job_destroy( jobContact );
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
					 rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
				if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
					LOG_GLOBUS_ERROR( "globus_gram_client_job_cancel()", rc );
					globusError = rc;
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
		case GM_FAILED: {
			// The jobmanager's job state has moved to FAILED. Send a
			// commit if necessary and take appropriate action.

			// Sending a COMMIT_END here means we no longer care
			// about this job submission. Either we know the job
			// isn't pending/running or the user has told us to
			// forget lost job submissions.
			CHECK_PROXY;
			rc = gahp->gt4_gram_client_job_destroy( jobContact );
			if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED ||
				 rc == GAHPCLIENT_COMMAND_PENDING ) {
				break;
			}
			if ( rc != GLOBUS_SUCCESS ) {
					// unhandled error
				LOG_GLOBUS_ERROR( "globus_gram_client_job_signal(COMMIT_END)", rc );
				globusError = rc;
				gmState = GM_CLEAR_REQUEST;
				break;
			}

			rehashJobContact( this, jobContact, NULL );
			free( jobContact );
			myResource->CancelSubmit( this );
			jobContact = NULL;
			jmDown = false;
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
			// We are done with the job. Propagate any remaining updates
			// to the schedd, then delete this object.
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
			if ( (jobContact != NULL || (globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED && globusStateErrorCode != GLOBUS_GRAM_PROTOCOL_ERROR_JOB_UNSUBMITTED)) 
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
			if ( globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED;
				UpdateJobAdInt( ATTR_GLOBUS_STATUS, globusState );
			}
			globusStateErrorCode = 0;
			globusError = 0;
			lastRestartReason = 0;
			numRestartAttemptsThisSubmit = 0;
			errorString = "";
			ClearCallbacks();
			// HACK!
			retryStdioSize = true;
			if ( jobContact != NULL ) {
				rehashJobContact( this, jobContact, NULL );
				free( jobContact );
				myResource->CancelSubmit( this );
				jobContact = NULL;
				jmDown = false;
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
			DeleteOutput();
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
			if ( jobContact &&
				 globusState != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN ) {
				globusState = GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN;
				UpdateJobAdInt( ATTR_GLOBUS_STATUS, globusState );
				//UpdateGlobusState( GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNKNOWN, 0 );
			}
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
				if ( holdReason[0] == '\0' && globusStateErrorCode != 0 ) {
					snprintf( holdReason, 1024, "Globus error %d: %s",
							  globusStateErrorCode,
							  "" );
				}
				if ( holdReason[0] == '\0' && globusError != 0 ) {
					snprintf( holdReason, 1024, "Globus error %d: %s", globusError,
							  "" );
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
			now = time(NULL);
			if ( myProxy->expiration_time > JM_MIN_PROXY_TIME + now ) {
				gmState = GM_START;
			} else {
				// Do nothing. Our proxy is about to expire.
			}
			} break;
		default:
			EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster,procID.proc,
					gmState );
		}

		if ( gmState != old_gm_state || globusState != old_globus_state ) {
			reevaluate_state = true;
		}
		if ( globusState != old_globus_state ) {
//			dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
//					procID.cluster, procID.proc,
//					GlobusJobStatusName(old_globus_state),
//					GlobusJobStatusName(globusState));
			enteredCurrentGlobusState = time(NULL);
		}
		if ( gmState != old_gm_state ) {
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state],
					GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
			// If we were waiting for a pending globus call, we're not
			// anymore so purge it.
			if ( gahp ) {
				gahp->purgePendingRequests();
			}
			// If we were calling a globus call that used RSL, we're done
			// with it now, so free it.
			if ( RSL ) {
				delete RSL;
				RSL = NULL;
			}
			connect_failure_counter = 0;
		}

	} while ( reevaluate_state );

	if ( ( connect_failure_jobmanager || connect_failure_gatekeeper ) && 
		 !resourceDown ) {
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
			if ( connect_failure_jobmanager ) {
				jmUnreachable = true;
			}
			resourcePingPending = true;
			myResource->RequestPing( this );
		}
	}

	return TRUE;
}

void GT4Job::NotifyResourceDown()
{
	resourceStateKnown = true;
	if ( resourceDown == false ) {
		WriteGT4ResourceDownEventToUserLog( ad );
	}
	resourceDown = true;
	jmUnreachable = false;
	resourcePingPending = false;
	// set downtime timestamp?
	SetEvaluateState();
}

void GT4Job::NotifyResourceUp()
{
	resourceStateKnown = true;
	if ( resourceDown == true ) {
		WriteGT4ResourceUpEventToUserLog( ad );
	}
	resourceDown = false;
	if ( jmUnreachable ) {
		jmDown = true;
	}
	jmUnreachable = false;
	resourcePingPending = false;
	SetEvaluateState();
}

bool GT4Job::AllowTransition( int new_state, int old_state )
{

	// Prevent non-transitions or transitions that go backwards in time.
	// The jobmanager shouldn't do this, but notification of events may
	// get re-ordered (callback and probe results arrive backwards).
    if ( new_state == old_state ||
		 new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED ||
		 old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ||
		 old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ||
		 ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED) ||
		 ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED &&
		   old_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_IN) ||
		 ( old_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT &&
		   new_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE &&
		   new_state != GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) ) {
		return false;
	}

	return true;
}


void GT4Job::UpdateGlobusState( int new_state, int new_error_code )
{
	bool allow_transition;

	allow_transition = AllowTransition( new_state, globusState );

	if ( allow_transition ) {
		// where to put logging of events: here or in EvaluateState?
		dprintf(D_FULLDEBUG, "(%d.%d) globus state change: %s -> %s\n",
				procID.cluster, procID.proc,
				GlobusJobStatusName(globusState),
				GlobusJobStatusName(new_state));

		if ( ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
			   new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ) &&
			 condorState == IDLE ) {
			JobRunning();
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED &&
			 condorState == RUNNING ) {
			JobIdle();
		}

		if ( globusState == GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED &&
			 !submitLogged && !submitFailedLogged ) {
			if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
					// TODO: should SUBMIT_FAILED_EVENT be used only on
					//   certain errors (ones we know are submit-related)?
				submitFailureCode = new_error_code;
				if ( !submitFailedLogged ) {
					WriteGT4SubmitFailedEventToUserLog( ad,
														submitFailureCode );
					submitFailedLogged = true;
				}
			} else {
					// The request was successfuly submitted. Write it to
					// the user-log and increment the globus submits count.
				int num_globus_submits = 0;
				if ( !submitLogged ) {
					WriteGT4SubmitEventToUserLog( ad );
					submitLogged = true;
				}
				ad->LookupInteger( ATTR_NUM_GLOBUS_SUBMITS,
								   num_globus_submits );
				num_globus_submits++;
				UpdateJobAdInt( ATTR_NUM_GLOBUS_SUBMITS, num_globus_submits );
			}
		}
		if ( (new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_STAGE_OUT ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE ||
			  new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED)
			 && !executeLogged ) {
			WriteExecuteEventToUserLog( ad );
			executeLogged = true;
		}

		if ( new_state == GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED ) {
			globusStateBeforeFailure = globusState;
		} else {
			UpdateJobAdInt( ATTR_GLOBUS_STATUS, new_state );
		}

		globusState = new_state;
		globusStateErrorCode = new_error_code;
		enteredCurrentGlobusState = time(NULL);

		requestScheddUpdate( this );

		SetEvaluateState();
	}
}

void GT4Job::GramCallback( int new_state, int new_error_code )
{
	if ( AllowTransition(new_state,
						 callbackGlobusState ?
						 callbackGlobusState :
						 globusState ) ) {

		callbackGlobusState = new_state;
		callbackGlobusStateErrorCode = new_error_code;

		SetEvaluateState();
	}
}

bool GT4Job::GetCallbacks()
{
	if ( callbackGlobusState != 0 ) {
		UpdateGlobusState( callbackGlobusState,
						   callbackGlobusStateErrorCode );

		callbackGlobusState = 0;
		callbackGlobusStateErrorCode = 0;
		return true;
	} else {
		return false;
	}
}

void GT4Job::ClearCallbacks()
{
	callbackGlobusState = 0;
	callbackGlobusStateErrorCode = 0;
}

BaseResource *GT4Job::GetResource()
{
	return (BaseResource *)myResource;
}


// Build submit RSL.. er... XML
MyString *GT4Job::buildSubmitRSL()
{
	MyString *rsl = new MyString;
	MyString iwd = "";
	MyString riwd = "";
	MyString buff;
	char *attr_value = NULL;
	char *rsl_suffix = NULL;

	char * gt4_location = param ("GT4_LOCATION");

		// Once we add streaming support, remove this
	if ( streamOutput || streamError ) {
		errorString = "Streaming not supported";
		return NULL;
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
/*
	rsl->sprintf( "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<job xmlns:gram=\"http://www.globus.org/namespaces/2004/06/job\" \
xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" \
xsi:schemaLocation=\"http://www.globus.org/namespaces/2004/06/job \
%s/share/schema/gram/job_description.xsd\">", gt4_location);
*/
	rsl->sprintf( "<job>" );


	//We're assuming all job clasads have a command attribute
	//First look for executable in the spool area.
	char *spooldir = param("SPOOL");
	if ( spooldir ) {
		char *source = gen_ckpt_name(spooldir,procID.cluster,ICKPT,0);
		free(spooldir);
		if ( access(source,F_OK | X_OK) >= 0 ) {
				// we can access an executable in the spool dir
			attr_value = strdup(source);
		}
	}
	if ( attr_value == NULL ) {
			// didn't find any executable in the spool directory,
			// so use what is explicitly stated in the job ad
		ad->LookupString( ATTR_JOB_CMD, &attr_value );
	}
	
    //*rsl += "(executable=";
	*rsl += printXMLParam ("executable", attr_value);
	free (attr_value);
	attr_value = NULL;
	
/*
  We don't do GASS_URL appendage here (GAHP does it)
  So don't worry about this part
  (this comment smells of BS)

	if ( !ad->LookupBool( ATTR_TRANSFER_EXECUTABLE, transfer ) || transfer ) {
		buff = "$(GRIDMANAGER_GASS_URL)/";
		if ( attr_value[0] != '/' ) {
			buff += iwd;
		}
		buff += attr_value;
	} else {
		buff = attr_value;
	}
	*rsl += rsl_stringify( buff.Value() );
	free( attr_value );
	attr_value = NULL;*/


	if ( ad->LookupString(ATTR_JOB_REMOTE_IWD, &attr_value) && *attr_value ) {
		*rsl += printXMLParam ("directory", attr_value);

		riwd = attr_value;
	} else {
		// The user didn't specify a remote IWD, so tell the jobmanager to
		// create a scratch directory in its default location and make that
		// the remote IWD.

		// Note that ${SCRATCH_DIR} is no longer supported by globus
		// Instead we'll upload an empty directory to be our scratch dir
		
		ASSERT (submit_id);	// append submit_id for uniqueness, fool
		riwd.sprintf ("${GLOBUS_USER_HOME}/job_%s", submit_id);

		*rsl += printXMLParam ("directory", riwd.Value());
	}

	if ( riwd[riwd.Length()-1] != '/' ) {
		riwd += '/';
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ad->LookupString(ATTR_JOB_ARGUMENTS, &attr_value) && *attr_value ) {
		*rsl += printXMLParam ("arguments", attr_value);
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ad->LookupString(ATTR_JOB_INPUT, &attr_value) && *attr_value &&
		 strcmp( attr_value, NULL_FILE ) ) {
		
		*rsl += printXMLParam ("stdin", attr_value);
	}
	if ( attr_value != NULL ) {
		free( attr_value );
		attr_value = NULL;
	}

//	if ( streamOutput ) {
//		*rsl += printXMLParam ("stdout", localOutput );
//	} else {
		if ( stageOutput ) {
			*rsl += printXMLParam ("stdout", "$(GLOBUS_CACHED_STDOUT)");
		} else {
			if ( ad->LookupString(ATTR_JOB_OUTPUT, &attr_value) &&
				 *attr_value && strcmp( attr_value, NULL_FILE ) ) {
				
				*rsl += printXMLParam ("stdout", attr_value);
			}
			if ( attr_value != NULL ) {
				free( attr_value );
				attr_value = NULL;
			}
		}
//	}

//	if ( streamError ) {
//		*rsl += printXMLParam ("stderr", localError);
//	} else {
		if ( stageError ) {
			*rsl += printXMLParam ("stderr", "$(GLOBUS_CACHED_STDERR)");
		} else {
			if ( ad->LookupString(ATTR_JOB_ERROR, &attr_value) &&
				 *attr_value && strcmp( attr_value, NULL_FILE ) ) {
				*rsl +=  printXMLParam ("stderr", attr_value );
			}
			if ( attr_value != NULL ) {
				free( attr_value );
				attr_value = NULL;
			}
		}
//	}

	// First upload an emtpy dummy directory
	// This will be the job's sandbox directory

	const char * file_in_header = "<fileStageIn><transfer>";
	const char * file_in_footer = "</transfer></fileStageIn>";

/* Only need dummy dir if user didn't specify RemoteIwd
	*rsl += file_in_header;
	buff.sprintf( "gsiftp://nostos.cs.wisc.edu%d", getDummyJobScratchDir() );
	*rsl += printXMLParam ("sourceUrl",
						   buff.Value());
	buff.sprintf( "file:///%s", riwd.Value() );
	*rsl += printXMLParam ("destinationUrl",
						   buff.Value()); // remote job dir
	*rsl += file_in_footer;
*/

		// Now deal with any other files we might wish to transfer
	if ( ad->LookupString(ATTR_TRANSFER_INPUT_FILES, &attr_value) &&
		 *attr_value ) {
		StringList filelist( attr_value, "," );
		if ( !filelist.isEmpty() ) {
			char *filename;
			   
			filelist.rewind();
			while ( (filename = filelist.next()) != NULL ) {

               // append the sandbox dir to the destination name 
               // of each file that we'll stage in, fool
			 
				buff.sprintf ("file://%s/%s",
							  riwd.Value(),
							  basename (filename));
				*rsl += file_in_header;
				*rsl += printXMLParam ("sourceUrl", 
										filename);
				*rsl += printXMLParam ("destinationUrl", 
									   buff.Value());
				*rsl += file_in_footer;

/*
  // this is no longer needed, as GAHP does substitution

				// append file pairs to rsl
				*rsl += '(';
				buff = "$(GRIDMANAGER_GASS_URL)";
				if ( filename[0] != '/' ) {
					buff += iwd;
				}
				buff += filename;
				*rsl += rsl_stringify( buff );
				*rsl += ' ';
				buff = riwd;
				buff += basename( filename );
				*rsl += rsl_stringify( buff );
				*rsl += ')';*/
			}
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ( ad->LookupString(ATTR_TRANSFER_OUTPUT_FILES, &attr_value) &&
		   *attr_value ) || stageOutput || stageError ) {
		StringList filelist( attr_value, "," );
		if ( !filelist.isEmpty() || stageOutput || stageError ) {
			const char * stage_out_header = "<fileStageOut><transfer>";
			const char * stage_out_footer = "</transfer></fileStageOut>";


			char *filename;

			if ( stageOutput ) {
				*rsl += stage_out_header;
				*rsl += printXMLParam ("sourceFile", 
									   "$(GLOBUS_CACHED_STDOUT)");
				*rsl += printXMLParam ("destinationFile", 
									   localOutput);
				*rsl += stage_out_footer;
			}

			if ( stageError ) {
				*rsl += stage_out_header;
				*rsl += printXMLParam ("sourceFile", 
									   "$(GLOBUS_CACHED_STDERRT)");
				*rsl += printXMLParam ("destinationFile", 
									   localError);
				*rsl += stage_out_footer;
			}

			filelist.rewind();
			while ( (filename = filelist.next()) != NULL ) {
				buff.sprintf ("file://%s/%s",
							  riwd.Value(),
							  basename (filename));
				*rsl += stage_out_header;
				*rsl += printXMLParam ("sourceFile", 
									   buff.Value());
				*rsl += printXMLParam ("destinationFile", 
									   filename);

				*rsl += stage_out_footer;
			}
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

	if ( ad->LookupString(ATTR_JOB_ENVIRONMENT, &attr_value) && *attr_value ) {
		Environ env_obj;
		env_obj.add_string(attr_value);
		char **env_vec = env_obj.get_vector();
		int i = 0;
		const char * envrionment_header = "<environment>";
		const char * environment_footer = "</environment>";

		while (env_vec[i]) {
			char *equals = strchr(env_vec[i],'=');
			if ( !equals ) {
				// this environment entry has no equals sign!?!?
				continue;
			}
			
			*rsl += envrionment_header;
			*rsl += printXMLParam ("name", env_vec[i]);
			*rsl += printXMLParam ("value", equals + 1);
			*rsl += environment_footer;

			i++;
		}
	}
	if ( attr_value ) {
		free( attr_value );
		attr_value = NULL;
	}

		/*

		// Not sure how to deal with this in GT4:

	buff.sprintf( ")(proxy_timeout=%d", JM_MIN_PROXY_TIME );
	*rsl += buff;

	buff.sprintf( ")"
				  "(remote_io_url=$(GRIDMANAGER_GASS_URL))",
				  JM_COMMIT_TIMEOUT );
	*rsl += buff;

	*/
	
	if ( ad->LookupString( ATTR_GLOBUS_RSL, &rsl_suffix ) ) {
		*rsl += rsl_suffix;
		free( rsl_suffix );
	}

		// Start the job on hold
	*rsl += printXMLParam ("holdState", "Pending" );

	*rsl += "</job>";

	free (gt4_location);

	return rsl;
}

void GT4Job::DeleteOutput()
{
	int rc;
	struct stat fs;

	mode_t old_umask = umask(0);

	if ( streamOutput ) {
		rc = stat( localOutput, &fs );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) stat(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, errno );
			fs.st_mode = S_IRWXU;
		}
		fs.st_mode &= S_IRWXU | S_IRWXG | S_IRWXO;
		rc = unlink( localOutput );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) unlink(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, errno );
		}
		rc = creat( localOutput, fs.st_mode );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) creat(%s,%d) failed, errno=%d\n",
					 procID.cluster, procID.proc, localOutput, fs.st_mode,
					 errno );
		} else {
			close( rc );
		}
	}

	if ( streamError ) {
		rc = stat( localError, &fs );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) stat(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, errno );
			fs.st_mode = S_IRWXU;
		}
		fs.st_mode &= S_IRWXU | S_IRWXG | S_IRWXO;
		rc = unlink( localError );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) unlink(%s) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, errno );
		}
		rc = creat( localError, fs.st_mode );
		if ( rc < 0 ) {
			dprintf( D_ALWAYS, "(%d.%d) creat(%s,%d) failed, errno=%d\n",
					 procID.cluster, procID.proc, localError, fs.st_mode,
					 errno );
		} else {
			close( rc );
		}
	}

	umask( old_umask );
}

static bool
WriteGT4SubmitEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	int version;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus submit record to user logfile\n",
			 cluster, proc );

	GlobusSubmitEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact = strnewp(contact);

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_CONTACT_STRING, contact,
						   sizeof(contact) - 1 );
	event.jmContact = strnewp(contact);

	version = 0;
	job_ad->LookupInteger( ATTR_GLOBUS_GRAM_VERSION, version );
	event.restartableJM = version >= GRAM_V_1_5;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_SUBMIT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

static bool
WriteGT4SubmitFailedEventToUserLog( ClassAd *job_ad, int failure_code )
{
	int cluster, proc;
	char buf[1024];

	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing submit-failed record to user logfile\n",
			 cluster, proc );

	GlobusSubmitFailedEvent event;

	snprintf( buf, 1024, "%d %s", failure_code,
			  "" );
	event.reason =  strnewp(buf);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_SUBMIT_FAILED event\n",
				 cluster, proc);
		return false;
	}

	return true;
}

static bool
WriteGT4ResourceUpEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus up record to user logfile\n",
			 cluster, proc );

	GlobusResourceUpEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact =  strnewp(contact);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_RESOURCE_UP event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

static bool
WriteGT4ResourceDownEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char contact[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing globus down record to user logfile\n",
			 cluster, proc );

	GlobusResourceDownEvent event;

	contact[0] = '\0';
	job_ad->LookupString( ATTR_GLOBUS_RESOURCE, contact,
						   sizeof(contact) - 1 );
	event.rmContact =  strnewp(contact);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_GLOBUS_RESOURCE_DOWN event\n",
				 cluster, proc );
		return false;
	}

	return true;
}


const char * 
GT4Job::printXMLParam (const char * name, const char * value) {
	static MyString buff;
		// TODO should perform escaping of special characters in value
	buff.sprintf ("<%s>%s</%s>", name, value, name);
	return buff.Value();
}

// Create an per-user empty directory, or return one if
// one already exist. The directory will be created under
// gridmanger scratch dir. NOT thread-safe.
const char*
GT4Job::getDummyJobScratchDir() {
	static MyString dirname;
	dirname.sprintf ("%s%cempty_dir_u%d", // <scratch>/empty_dir_u<uid>
					 GridmanagerScratchDir, 
					 DIR_DELIM_CHAR,
					 geteuid());
	
	struct stat stat_buff;
	if (!(stat(dirname.Value(), &stat_buff))) {
		if ( mkdir (dirname.Value(), 0700) < 0 ) {
			dprintf (D_ALWAYS, "Unable to create scratch directory %s\n", 
					 dirname.Value());
			return NULL;
		}
	}

	return dirname.Value();
}
