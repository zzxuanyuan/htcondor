/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

  
#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "nullfile.h"
#include "filename_tools.h"

#include "gridmanager.h"
#include "amazonjob.h"
#include "condor_config.h"
  
#define GM_INIT							0
#define GM_UNSUBMITTED					1
#define GM_SUBMIT						2
#define GM_SUBMIT_SAVE					3
#define GM_SUBMITTED					4
#define GM_DONE_SAVE					5
#define GM_CANCEL						6
#define GM_FAILED						7
#define GM_DELETE						8
#define GM_CLEAR_REQUEST				9
#define GM_HOLD							10
#define GM_PROBE_JOB					11
#define GM_START						12
#define GM_CREATE_KEYPAIR				13
#define GM_DESTROY_KEYPAIR				14
#define GM_RECOVERY						15
#define GM_BEFORE_SSH_KEYPAIR			16
#define GM_AFTER_SSH_KEYPAIR			17
#define GM_BEFORE_STARTVM				18
#define GM_AFTER_STARTVM				19
#define GM_NEED_CHECK_VM				20
#define GM_NEED_CHECK_KEYPAIR			21

static char *GMStateNames[] = {
	"GM_INIT",
	"GM_UNSUBMITTED",
	"GM_SUBMIT",
	"GM_SUBMIT_SAVE",
	"GM_SUBMITTED",
	"GM_DONE_SAVE",
	"GM_CANCEL",
	"GM_FAILED",
	"GM_DELETE",
	"GM_CLEAR_REQUEST",
	"GM_HOLD",
	"GM_PROBE_JOB",
	"GM_START",
	"GM_CREATE_KEYPAIR",
	"GM_DESTROY_KEYPAIR",
	"GM_RECOVERY",
	"GM_BEFORE_SSH_KEYPAIR",
	"GM_AFTER_SSH_KEYPAIR",
	"GM_BEFORE_STARTVM",
	"GM_AFTER_STARTVM",
	"GM_NEED_CHECK_VM",
	"GM_NEED_CHECK_KEYPAIR"
};

#define AMAZON_VM_STATE_RUNNING			"running"
#define AMAZON_VM_STATE_PENDING			"pending"
#define AMAZON_VM_STATE_SHUTTINGDOWN	"shutting-down"
#define AMAZON_VM_STATE_TERMINATED		"terminated"

#define AMAZON_SUBMIT_UNDEFINED		-1
#define AMAZON_SUBMIT_BEFORE_SSH	0
#define AMAZON_SUBMIT_AFTER_SSH		1
#define AMAZON_SUBMIT_BEFORE_VM		2
#define AMAZON_SUBMIT_AFTER_VM		3
#define AMAZON_SHUTDOWN_VM			100

#define AMAZON_REMOVE_EMPTY			0
#define AMAZON_REMOVE_BEFORE_SSH	1
#define AMAZON_REMOVE_AFTER_SSH		2
#define AMAZON_REMOVE_BEFORE_VM		3
#define AMAZON_REMOVE_AFTER_VM		4

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_strcmp _stricmp
#	define file_contains contains_anycase
#else
#	define file_strcmp strcmp
#	define file_contains contains
#endif

#define AMAZON_LOG_DIR ".amazon_log"

// TODO: Let the maximum submit attempts be set in the job ad or, better yet,
// evalute PeriodicHold expression in job ad.
#define MAX_SUBMIT_ATTEMPTS	1

void AmazonJobInit()
{
}


void AmazonJobReconfig()
{
	// change interval time for 5 minute
	int tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 60 * 5 ); 
	AmazonJob::setProbeInterval( tmp_int );
		
	// Tell all the resource objects to deal with their new config values
	AmazonResource *next_resource;

	AmazonResource::ResourcesByName.startIterations();

	while ( AmazonResource::ResourcesByName.iterate( next_resource ) != 0 ) {
		next_resource->Reconfig();
	}	
}


bool AmazonJobAdMatch( const ClassAd *job_ad )
{
	int universe;
	MyString resource;
	
	job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe );
	job_ad->LookupString( ATTR_GRID_RESOURCE, resource );

	if ( (universe == CONDOR_UNIVERSE_GRID) && (strncasecmp( resource.Value(), "amazon", 6 ) == 0) ) 
	{
		return true;
	}
	return false;
}


BaseJob* AmazonJobCreate( ClassAd *jobad )
{
	return (BaseJob *)new AmazonJob( jobad );
}

int AmazonJob::gahpCallTimeout = 600;
int AmazonJob::probeInterval = 300;
int AmazonJob::submitInterval = 300;
int AmazonJob::maxConnectFailures = 3;
int AmazonJob::funcRetryInterval = 15;
int AmazonJob::pendingWaitTime = 15;
int AmazonJob::maxRetryTimes = 3;

AmazonJob::AmazonJob( ClassAd *classad )
	: BaseJob( classad )
{
dprintf( D_ALWAYS, "================================>  AmazonJob::AmazonJob 1 \n");
	char buff[16385]; // user data can be 16K, this is 16K+1
	MyString error_string = "";
	char *gahp_path = NULL;
	char *gahp_log = NULL;
	char *gahp_min_workers = NULL;
	char *gahp_debug = NULL;
	ArgList args;
	
	remoteJobId = NULL;
	remoteJobState = "";
	gmState = GM_INIT;
	lastProbeTime = 0;
	enteredCurrentGmState = time(NULL);
	lastSubmitAttempt = 0;
	numSubmitAttempts = 0;
	myResource = NULL;
	gahp = NULL;
	
	// check the access_key_file
	buff[0] = '\0';
	jobAd->LookupString( ATTR_AMAZON_ACCESS_KEY, buff );
	m_access_key_file = strdup(buff);
	
	if ( strlen(m_access_key_file) == 0 ) {
		error_string = "Access key file not defined";
		goto error_exit;
	}

	// check the secret_key_file
	buff[0] = '\0';
	jobAd->LookupString( ATTR_AMAZON_SECRET_KEY, buff );
	m_secret_key_file = strdup(buff);
	
	if ( strlen(m_secret_key_file) == 0 ) {
		error_string = "Secret key file not defined";
		goto error_exit;
	}

		// XXX: Buffer Overflow if the user_data is > 16K? This code
		// should be unprivileged.

		// XXX: It is bad to assume the buff is initialized to 0s,
		// always use memset? All this code should be changed to get
		// at the attribute in a better way.

	memset(buff, 0, 16385);
	m_user_data = NULL;
	m_user_data_file = NULL;	
	
	// if user assigns both user_data and user_data_file, only user_data_file
	// will be used.
	if ( jobAd->LookupString( ATTR_AMAZON_USER_DATA_FILE, buff ) ) {
		m_user_data_file = strdup(buff);	
	} else {
		if ( jobAd->LookupString( ATTR_AMAZON_USER_DATA, buff ) ) {
			m_user_data = strdup(buff);
		}
	}
	
	// get VM instance type
	memset(buff, 0, 16385);
	m_instance_type = NULL; // if clients don't assign this value in condor submit file,
							// we should set the default value to NULL and gahp_server
							// will start VM in Amazon using m1.small mode.
	if ( jobAd->LookupString( ATTR_AMAZON_INSTANCE_TYPE, buff ) ) {
		m_instance_type = strdup(buff);	
	}
	
	m_group_names = NULL;
	m_vm_check_times = 0;
	m_keypair_check_times = 0;

	// for SSH keypair output file
	{
	char* buffer = NULL;
	
	// Notice:
	// 	we can have two kinds of SSH keypair output file names or the place where the 
	// output private file should be written to, 
	// 	1. the name assigned by client in the condor submit file with attribute "AmazonKeyPairFileName"
	// 	2. if there is no attribute "AmazonKeyPairFileName" in the condor submit file, we 
	// 	   should discard this private file by writing to NULL_FILE
	if ( jobAd->LookupString( ATTR_AMAZON_KEY_PAIR_FILE_NAME, &buffer ) ) {
		// clinet define the location where this SSH keypair file will be written to
		m_key_pair_file_name = buffer;
	} else {
		// If client doesn't assign keypair output file name, we should discard it by 
		// writing this private file to /dev/null
		m_key_pair_file_name = NULL_FILE;
	}
	free (buffer);
	}

	// set the default value/status for current submit step
	m_submit_step = AMAZON_SUBMIT_UNDEFINED;
		
	// In GM_HOLD, we assume HoldReason to be set only if we set it, so make
	// sure it's unset when we start (unless the job is already held).
	if ( condorState != HELD && jobAd->LookupString( ATTR_HOLD_REASON, NULL, 0 ) != 0 ) {
		jobAd->AssignExpr( ATTR_HOLD_REASON, "Undefined" );
	}

	gahp_path = param( "AMAZON_GAHP" );
	if ( gahp_path == NULL ) {
		error_string = "AMAZON_GAHP not defined";
		goto error_exit;
	}

	snprintf( buff, sizeof(buff), AMAZON_RESOURCE_NAME ); // for client's ID

	gahp_log = param( "AMAZON_GAHP_LOG" );
	if ( gahp_log == NULL ) {
		dprintf(D_ALWAYS, "Warning: No AMAZON_GAHP_LOG defined\n");
	} else {
		args.AppendArg("-f");
		args.AppendArg(gahp_log);
		free(gahp_log);
	}

	args.AppendArg("-w");
	gahp_min_workers = param( "AMAZON_GAHP_WORKER_MIN_NUM" );
	if (!gahp_min_workers) {
		args.AppendArg("1");
	} else {
		args.AppendArg(gahp_min_workers);
	}

		// FIXME: Change amazon-gahp to accept AMAZON_GAHP_WORKER_MAX_NUM

	args.AppendArg("-d");
	gahp_debug = param( "AMAZON_GAHP_DEBUG" );
	if (!gahp_min_workers) {
		args.AppendArg("D_ALWAYS");
	} else {
		args.AppendArg(gahp_debug);
	}

	gahp = new GahpClient( buff, gahp_path, &args );
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	myResource = AmazonResource::FindOrCreateResource( AMAZON_RESOURCE_NAME, m_access_key_file, m_secret_key_file );
	myResource->RegisterJob( this );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( buff[0] ) {
		const char *token;
		MyString str = buff;

		str.Tokenize();

		token = str.GetNextToken( " ", false );
		if ( !token || stricmp( token, "amazon" ) ) {
			error_string.sprintf( "%s not of type amazon",
								  ATTR_GRID_JOB_ID );
			goto error_exit;
		}

		token = str.GetNextToken( " ", false );
		if ( token ) {
			m_key_pair = token;
		}

		token = str.GetNextToken( " ", false );
		if ( token ) {
			remoteJobId = strdup( token );
		}
	}
	
	jobAd->LookupString( ATTR_GRID_JOB_STATUS, remoteJobState );

	// JEF: Increment a GMSession attribute for use in letting the job
	// ad crash the gridmanager on request
	if ( jobAd->Lookup( "CrashGM" ) != NULL ) {
		int session = 0;
		jobAd->LookupInteger( "GMSession", session );
		session++;
		jobAd->Assign( "GMSession", session );
	}

	return;

 error_exit:
	gmState = GM_HOLD;
	if ( !error_string.IsEmpty() ) {
		jobAd->Assign( ATTR_HOLD_REASON, error_string.Value() );
	}
	
	return;
}

AmazonJob::~AmazonJob()
{
	if ( myResource ) myResource->UnregisterJob( this );
	if ( remoteJobId ) free( remoteJobId );
	
	if ( gahp != NULL ) delete gahp;
	free (m_access_key_file);
	free (m_secret_key_file);
	free (m_user_data);
	if (m_group_names != NULL) delete m_group_names;
	free(m_user_data_file);
	free(m_instance_type);
}


void AmazonJob::Reconfig()
{
	BaseJob::Reconfig();
}


int AmazonJob::doEvaluateState()
{
	int old_gm_state;
	bool reevaluate_state = true;
	time_t now = time(NULL);

	bool done;
	int rc;

	daemonCore->Reset_Timer( evaluateStateTid, TIMER_NEVER );

    dprintf(D_ALWAYS, "(%d.%d) doEvaluateState called: gmState %s, condorState %d\n",
			procID.cluster,procID.proc,GMStateNames[gmState],condorState);

	if ( gahp ) {
		if ( !resourceStateKnown || resourcePingPending || resourceDown ) {
			gahp->setMode( GahpClient::results_only );
		} else {
			gahp->setMode( GahpClient::normal );
		}
	}
	
	do {
		
		char *gahp_error_code = NULL;

		// JEF: Crash the gridmanager if requested by the job
		int should_crash = 0;
		jobAd->Assign( "GMState", gmState );
		jobAd->SetDirtyFlag( "GMState", false );
		if ( jobAd->EvalBool( "CrashGM", NULL, should_crash ) && should_crash ) {
			EXCEPT( "Crashing gridmanager at the request of job %d.%d",
					procID.cluster, procID.proc );
		}

		reevaluate_state = false;
		old_gm_state = gmState;
		
		switch ( gmState ) 
		{
			case GM_INIT:
				// This is the state all jobs start in when the AmazonJob object
				// is first created. Here, we do things that we didn't want to
				// do in the constructor because they could block (the
				// constructor is called while we're connected to the schedd).

				// JEF: Save GMSession to the schedd if needed
				if ( requestScheddUpdate( this ) == false ) {
					break;
				}

				if ( gahp->Startup() == false ) {
					dprintf( D_ALWAYS, "(%d.%d) Error starting GAHP\n", procID.cluster, procID.proc );
					jobAd->Assign( ATTR_HOLD_REASON, "Failed to start GAHP" );
					gmState = GM_HOLD;
					break;
				}
				
				//gmState = GM_START;  // for test only
				gmState = GM_RECOVERY;
				break;
				
			/*
			*** Design for Failure Recovery ***
			
			To implement failure recovery, we will use the environmental variable "GridJobID" to 
			record the submitting step of the Amazon Job.
			
			A new state call GM_RECOVERY will be added and will be placed between GM_INIT and GM_START.
			
			1. before registering SSH keypair		// test OK
				GridJobID = "ssh_keypair_name"
				In this step, we should check in EC2, if the given ssh_keypair has been registered or not
				
			2. after registering SSH_keypair		// test OK
				GridJobID = "ssh_keypair_name ssh_done"
				 
			3. before starting VM
				GridJobID = "ssh_keypair_name ssh_done vm_starting"
				In this step, we should check in EC2, if the given VM has been started or not. But based  
				on the current implementation, we just re-start VM. 
				
			4. after starting VM
				GridJobID = "ssh_keypair_name ssh_done vm_starting vm_instance_id"
				
			5. after we start to shutdown the VM 
				GridJobID = "ssh_keypair_name ssh_done vm_starting vm_instance_id"
				In this step, we also need to check the current condor state should be HOLD or REMOVE
				When GridManager finds a given Amazon Job's state is this, the gmState will be redirected
				to GM_CANCEL.
				
			To save the log information into AmazonRecoverySteps, we should use SetSubmitStepInfo(). To save them to 
			the schedd, we should use requestScheddUpdate(). But requestScheddUpdate() works a little like 
			the Amazon Gahp commands, its first return value is always false and we should use break to keep
			watching on its return value. So the four places where we will calling 
			SetSubmitStepInfo()/requestScheddUpdate(), we should use four new states for them:
			
			GM_BEFORE_SSH_KEYPAIR
			GM_AFTER_SSH_KEYPAIR
			GM_BEFORE_STARTVM
			GM_AFTER_STARTVM
			AMAZON_SHUTDOWN_VM
			
			In the future, for every new gahp function which need to be logged, we should add two new states
			for it, one is before and one is after. Some of these adjacent states can be merged, but to make
			our implementation easy to understand and graceful, we will not merge them.
			
			*** End of Design for Failure Recovery ***
			*/
			
			case GM_RECOVERY:
				{
				
				// Now we should first check the value of "GridJobID"
				char* submit_status = NULL;
				
				if ( jobAd->LookupString( "AmazonRecoverySteps", &submit_status ) ) {

					// checking which step this job has reached
					StringList * submit_steps = new StringList(submit_status, " ");
					submit_steps->rewind();
					m_submit_step = submit_steps->number() - 1;	// get the size of StatusList
					
					// check if we have reached shut-down stage, this requires us to check
					// current condor state is REMOVED
					if ( condorState == REMOVED ) {
						m_submit_step = AMAZON_SHUTDOWN_VM;
					}

					switch( m_submit_step ) 
					{
						case AMAZON_SUBMIT_UNDEFINED:
							
							// Normally we should not come into this branch, in this situation we should
							// goto GM_START directly since there is no submitting status information
							gmState = GM_START;
							break;
								
						case AMAZON_SUBMIT_BEFORE_SSH:
							
							{
							// get the SSH keypair name
							submit_steps->rewind();
							char* existing_ssh_keypair = strdup(submit_steps->next());

							// check if this SSH keypair has already been registered in EC2
							StringList * returnKeys = new StringList();
							
							rc = gahp->amazon_vm_keypair_names(m_access_key_file, m_secret_key_file,
															   *returnKeys, gahp_error_code);

							if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
								break;
							}
							
							// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
							// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
							// processing error code received
							if ( gahp_error_code == NULL ) {
								// go ahead
							} else {
								// print out the received error code
								print_error_code(gahp_error_code, "amazon_vm_keypair_names()");
					
								// change Job's status to HOLD since we meet the errors we cannot proceed
								gmState = GM_HOLD;
								break;
							}
							
							if (rc == 0) {
								// now we should check if this SSH keypair has already been registered in EC2
								bool is_registered = false;
								int size = returnKeys->number();
								returnKeys->rewind();
								for (int i=0; i<size; i++) {
									if (strcmp(existing_ssh_keypair, returnKeys->next()) == 0) {
										is_registered = true;
										break;
									}
								}

								if ( is_registered ) {

									dprintf(D_ALWAYS,"(%d.%d) job don't need to re-register temporary SSH keypair! \n", procID.cluster, procID.proc );				
									
									// this job has registered SSH keypair successfully, we can do the next job
									gmState = GM_AFTER_SSH_KEYPAIR;
																		
									// save SSH keypair to global variable
									m_key_pair = existing_ssh_keypair;
									
								} else {
									
									dprintf(D_ALWAYS,"(%d.%d) job need to re-register temporary SSH keypair! \n", procID.cluster, procID.proc );
									
									// we have to redo the registering SSH keypair
									gmState = GM_BEFORE_SSH_KEYPAIR;
									// Since SetSubmitStepInfo() will automatically ignore the duplicate info
									// we don't need to remove any existing logs here.
								}
							} else {
								errorString = gahp->getErrorString();
								dprintf(D_ALWAYS,"(%d.%d) job create temporary keypair failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
								gmState = GM_HOLD;
							}
							
							free(existing_ssh_keypair);
							delete returnKeys;	

							}
							
							break;							
							
						case AMAZON_SUBMIT_AFTER_SSH:
							{
							// set some global variables we needed			
							submit_steps->rewind();
							m_key_pair = submit_steps->next();
							
							// double check the information saved is correct
							char* ssh_done = strdup(submit_steps->next());	

							if ( strcmp(ssh_done, "ssh_done") != 0 ) {
								// normally we should not come into this branch
								dprintf(D_ALWAYS,"(%d.%d) job setup submit steps failed in: %s\n", 
										procID.cluster, procID.proc, ssh_done );
								gmState = GM_HOLD;
							} else {
								// in this situation, we have registered the SSH keypair, keep doing it
								gmState = GM_BEFORE_STARTVM;
								// don't need to clean the submitting log here
							}
							
							free( ssh_done );
								
							}
							
							break;
							
						case AMAZON_SUBMIT_BEFORE_VM:
							{
							// we should also get the SSH keypair which will be used in vm_start()
							submit_steps->rewind();
							m_key_pair = submit_steps->next();

							// check if the VM has been started successfully
							StringList returnStatus;
							
							rc = gahp->amazon_vm_vm_keypair_all(m_access_key_file, m_secret_key_file,
															    returnStatus, gahp_error_code);

							if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
								break;
							}
							
							// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
							// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
							// processing error code received
							if ( gahp_error_code == NULL ) {
								// go ahead
							} else {
								// print out the received error code
								print_error_code(gahp_error_code, "amazon_vm_vm_keypair_all()");
					
								// change Job's status to HOLD since we meet the errors we cannot proceed
								gmState = GM_HOLD;
								break;
							}

							if (rc == 0) {

								// now we should check, corresponding to a given SSH keypair, in EC2, is there
								// existing any running VM instances? If these are some ones, we just return the
								// first one we found.
								bool is_running = false;
								char* instance_id = NULL;
								char* keypair_name = NULL;
								
								int size = returnStatus.number();
								returnStatus.rewind();
								
								for (int i=0; i<size/2; i++) {
									
									instance_id = returnStatus.next();
									keypair_name = returnStatus.next();

									if (strcmp(m_key_pair.Value(), keypair_name) == 0) {
										is_running = true;
										break;
									}
								}
								
								if ( is_running ) {

									// there is a running VM instance corresponding to the given SSH keypair
									myResource->AlreadySubmitted( this );
									gmState = GM_AFTER_STARTVM;
									// save the instance ID which will be used when delete VM instance
									SetInstanceId( instance_id );
									
								} else {
									// we shoudl re-start the VM again with the corresponding SSH keypair
									gmState = GM_BEFORE_STARTVM;
								}
							} else {
								errorString = gahp->getErrorString();
								dprintf(D_ALWAYS,"(%d.%d) job create temporary keypair failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
								gmState = GM_HOLD;
							}
											
							}							

							break;
							
							
						case AMAZON_SUBMIT_AFTER_VM:
							{
							// save the SSH keypair which will be used later (removing the VM, SSH keypair)
							submit_steps->rewind();
							m_key_pair = submit_steps->next();
							
							// VM instance ID should also be saved to global variable
							submit_steps->rewind();
							for (int i=0; i<AMAZON_SUBMIT_AFTER_VM; i++)
								submit_steps->next();
							SetInstanceId( submit_steps->next() );
														
							myResource->AlreadySubmitted( this );
							gmState = GM_SUBMIT_SAVE;
							}
							
							break;
							
							
						case AMAZON_SHUTDOWN_VM:
							{
							// we should redo the shutdown process based on the information we saved
							
							// first we should check how many steps we have already done
							int steps_done = submit_steps->number();
							
							// In some situations the gridmanger will stopped by client before it can
							// successfully start the VM in EC2. In this process, if the gridmanager is
							// crashed, we should recover the stopping process based on the recovery record
							// saved in the RecoverySteps.
							
							switch (steps_done) {
								
								case AMAZON_REMOVE_EMPTY:

									// we didn't write any recovery record yet. It means we didn't
									// register SSH_key and start VM so just stop and exit
									gmState = GM_FAILED;
									break;
									
								case AMAZON_REMOVE_BEFORE_SSH:
								case AMAZON_REMOVE_AFTER_SSH:
									{
									// we have recoreded "SSH key" and/or "ssh_done". In this situation, we
									// should try to remove the SSH key but don't do anything for VM since there
									// is no started VM yet 
									
									// get the SSH keypair and corresponding output file name
									submit_steps->rewind();
									m_key_pair = submit_steps->next();
									
									// Notice: unregister a non-existing SSH keypair will return success
									gmState = GM_DESTROY_KEYPAIR;
									}
									
									break;
									
								case AMAZON_REMOVE_BEFORE_VM:
									{
									// we have recorded "SSH key" and "ssh_done" and we also recorded "vm_starting".
									// In this situation we should based on SSH key to find out if there is a running
									// VM in amazon. If no, we just remove the SSH key, if yes, we should stop the VM
									
									// get the SSH keypair and corresponding output file name
									submit_steps->rewind();
									m_key_pair = submit_steps->next();
									
									// now try to find if there is running VM corresponding to this SSH key
									StringList returnStatus;
							
									rc = gahp->amazon_vm_vm_keypair_all(m_access_key_file, m_secret_key_file,
															    		returnStatus, gahp_error_code);

									if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
										break;
									}
							
									// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
									// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
									// processing error code received
									if ( gahp_error_code == NULL ) {
										// go ahead
									} else {
										// print out the received error code
										print_error_code(gahp_error_code, "amazon_vm_vm_keypair_all()");
					
										// change Job's status to HOLD since we meet the errors we cannot proceed
										gmState = GM_HOLD;
										break;
									}

									if (rc == 0) {

										// now we should check, corresponding to a given SSH keypair, in EC2, is there
										// existing any running VM instances? If these are some ones, we just return the
										// first one we found.
										bool is_running = false;
										char* instance_id = NULL;
										char* keypair_name = NULL;
								
										int size = returnStatus.number();
										returnStatus.rewind();
								
										for (int i=0; i<size/2; i++) {
									
											instance_id = returnStatus.next();
											keypair_name = returnStatus.next();

											if (strcmp(m_key_pair.Value(), keypair_name) == 0) {
												is_running = true;
												break;
											}
										}
								
										if ( is_running ) {
											
											// save the instance ID which will be used when delete VM instance
											myResource->AlreadySubmitted( this );
											SetInstanceId( instance_id );
											
											// there is a running VM instance corresponding to the given SSH keypair
											// we should remove it and its corresponding SSH key
											// Notice: even when we are deleting a non-existing VM, it will return success 
											gmState = GM_CANCEL;
									
										} else {
											// No running VM corresponding to this SSH key. We only need to remove the SSH key
											gmState = GM_DESTROY_KEYPAIR;
										}
									} else {
										errorString = gahp->getErrorString();
										dprintf(D_ALWAYS,"(%d.%d) job create temporary keypair failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
										gmState = GM_HOLD;
									}

									}
									
									break;
									
								case AMAZON_REMOVE_AFTER_VM:
									{
									// we have recorded "SSH key" and "ssh_done". We also recorded "vm_starting" and instance id.
									// In this situation, we should remove everything.
									
									// get the SSH keypair and corresponding output file name
									submit_steps->rewind();
									m_key_pair = submit_steps->next();
									
									// get the running VM's instance id
									submit_steps->rewind();
									for (int i=0; i<AMAZON_SUBMIT_AFTER_VM; i++) {
										submit_steps->next();
									}
									
									myResource->AlreadySubmitted( this );
									SetInstanceId( submit_steps->next() );
									
									// now try to delete VM and SSH key
									// Notice: even when we are deleting a non-existing VM, it will return success 
									gmState = GM_CANCEL;									
									}
									
									break;
									
								default:
									
									// there should be some errors when reach this branch
									gmState = GM_HOLD;
									
									break;
							} // end of switch(steps_done)
						
							}
							
							break;						
						
						default:
							gmState = GM_HOLD;							
							break;
					}
					
				} else {
					// the GridJonID is empty, it is a new job, don't need to do any recovery work
					gmState = GM_START;
				}

				}
				
				break;
				
				
			case GM_BEFORE_SSH_KEYPAIR:
				// Fail Recovery: before register SSH keypair

				// First we should set the value of SSH keypair. In normal situation, this
				// name should be dynamically created. 
				if ( m_key_pair == "" ) {
					m_key_pair = build_keypair();
				}

				// Save this temporarily created SSH keypair to the submitting log
				SetKeypairId( m_key_pair.Value() );
				SetSubmitStepInfo(m_key_pair.Value());

				done = requestScheddUpdate( this );
				
				if ( done ) {
					gmState = GM_CREATE_KEYPAIR; 
				}

				break;
				

			case GM_AFTER_SSH_KEYPAIR:

				// Fail Recovery: after register SSH keypair
				SetSubmitStepInfo("ssh_done");
				done = requestScheddUpdate( this );
				if ( done ) {
					gmState = GM_BEFORE_STARTVM; 
				}
				break;
				

			case GM_BEFORE_STARTVM:

				// Fail Recovery: before starting VM
				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_DESTROY_KEYPAIR;
					break;
				}

				SetSubmitStepInfo("vm_starting");
				done = requestScheddUpdate( this );
				if ( done ) {
					gmState = GM_SUBMIT; 
				}
				break;


			case GM_AFTER_STARTVM:

				// Fail Recovery: after starting VM
				SetSubmitStepInfo(remoteJobId);
				done = requestScheddUpdate( this );
				if ( done ) {
					gmState = GM_SUBMIT_SAVE; 
				}				
				break;

											
			case GM_START:

				errorString = "";
				
				if ( remoteJobId == NULL ) {
					gmState = GM_CLEAR_REQUEST;
				} 
				else {
					submitLogged = true;
					if ( condorState == RUNNING || condorState == COMPLETED ) {
						executeLogged = true;
					}
					gmState = GM_SUBMITTED;
				}
				
				// test the gridmanager is crashed before any logs have been recorded
				// stopcode(); // test only
				
				break;
				
			case GM_UNSUBMITTED:

				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_BEFORE_SSH_KEYPAIR;
				}
				
				break;
				
			case GM_SUBMIT:
				
				// test for AMAZON_SUBMIT_BEFORE_VM (the VM doesn't start successfully)
				// need to re-start the VM again.
				// stopcode(); // test only

				if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
					gmState = GM_HOLD;
					break;
				}
				
							
				// After a submit, wait at least submitInterval before trying another one.
				if ( now >= lastSubmitAttempt + submitInterval ) {
	
					// Once RequestSubmit() is called at least once, you must
					// CancelSubmit() once you're done with the request call
					if ( myResource->RequestSubmit( this ) == false ) {
						// If we haven't started the START_VM call yet,
						// we can abort the submission here for held and
						// removed jobs.
						if ( (condorState == REMOVED) ||
							 (condorState == HELD) ) {

							myResource->CancelSubmit( this );
							gmState = GM_DESTROY_KEYPAIR;
						}
						break;
					}

					// construct input parameters for amazon_vm_start()
					char* instance_id = NULL;
					
					// For a given Amazon Job, in its life cycle, the attributes will not change 					
					
					
					m_ami_id = build_ami_id();
					if ( m_key_pair == "" ) {
						m_key_pair = build_keypair();
					}
					if ( m_group_names == NULL )	m_group_names = build_groupnames();
					
					// amazon_vm_start() will check the input arguments
					rc = gahp->amazon_vm_start( m_access_key_file, m_secret_key_file, 
												m_ami_id.Value(), m_key_pair.Value(), 
												m_user_data, m_user_data_file, m_instance_type, 
												*m_group_names, instance_id, gahp_error_code);
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						// Every first time this function will come to here, just exit doEvaluateState()
						// and later gahp_client will call this function again. At that time, we can expect
						// the return value will be success and will come to the following statements.
						break;
					}
					
					// The upper limit of instances is 20, so when the client reaches this limit, we should
					// wait and retry for several times before we change its job's status to HOLD. Current, 
					// only command VM_START will meet this problem. Other commands will not meet the error
					// code = "InstanceLimitExceeded".
				
					// Everytime when we execute the commands, we will get two answers. The first one is 
					// the quick one answer without any uesful information. The second one is the one we
					// need. So each time the first one will always be success and when we get this answer
					// we will try to clean the timer (if we have setup). This is why we place the codes 
					// for "InstanceLimitExceeded" scenario after GAHPCLIENT_COMMAND_NOT_SUBMITTED and
					// GAHPCLIENT_COMMAND_PENDING.
	
					// processing error code received
					if ( gahp_error_code == NULL ) {
						
						// go ahead since the operation is successful
		
					} else if ( strcmp(gahp_error_code, "InstanceLimitExceeded" ) == 0 ) {
						
						// meet the resource limitation (maximum 20 instances)
						// should retry this command later
						daemonCore->Reset_Timer( evaluateStateTid, submitInterval );
						break;
				
					} else if ( strcmp(gahp_error_code, "NEED_CHECK_VM_START" ) == 0 ) {
						
						// get an error code from gahp server said that we should check if 
						// the VM has been started successfully in EC2
						
						// Maxmium retry times is 3, if exceeds this limitation, we will go to HOLD state
						if ( m_vm_check_times++ == maxRetryTimes ) {
							gmState = GM_HOLD;
						} else {
							gmState = GM_NEED_CHECK_VM;
						}
						
						break;							
										
					} else {
						// received the errors we cannot processed					
						// print out the received error code
						print_error_code(gahp_error_code, "amazon_vm_start()");
						
						// change Job's status to HOLD since we meet the errors we cannot proceed
						gmState = GM_HOLD;
						break;
					}
					
					// to process other return values of this command
					myResource->SubmitComplete( this );
					lastSubmitAttempt = time(NULL);
					numSubmitAttempts++;
	
					if ( rc == 0 ) {
						
						// test for AMAZON_SUBMIT_BEFORE_VM (the VM does start successfully)
						// Don't need to re-start the VM again.
						// stopcode(); // test only

						ASSERT( instance_id != NULL );
						SetInstanceId( instance_id );
						WriteGridSubmitEventToUserLog(jobAd);
						free( instance_id );
											
						// gmState = GM_SUBMIT_SAVE; 
						gmState = GM_AFTER_STARTVM;
						
					} else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_UNSUBMITTED;
					}
					
				} else {
					if ( (condorState == REMOVED) || (condorState == HELD) ) {
						gmState = GM_DESTROY_KEYPAIR;
						break;
					}

					unsigned int delay = 0;
					if ( (lastSubmitAttempt + submitInterval) > now ) {
						delay = (lastSubmitAttempt + submitInterval) - now;
					}				
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}

				break;
				
			
			case GM_NEED_CHECK_VM:
				
				{
					
				// check if the VM has been started successfully
				StringList returnStatus;
							
				rc = gahp->amazon_vm_vm_keypair_all(m_access_key_file, m_secret_key_file,
												    returnStatus, gahp_error_code);

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
				}
							
				// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
				// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
				// processing error code received
				if ( gahp_error_code == NULL ) {
					// go ahead
				} else {
					// print out the received error code
					print_error_code(gahp_error_code, "amazon_vm_vm_keypair_all()");
					
					// change Job's status to HOLD since we meet the errors we cannot proceed
					gmState = GM_HOLD;
					break;
				}

				if (rc == 0) {

					// now we should check, corresponding to a given SSH keypair, in EC2, is there
					// existing any running VM instances? If these are some ones, we just return the
					// first one we found.
					bool is_running = false;
					char* instance_id = NULL;
					char* keypair_name = NULL;
								
					int size = returnStatus.number();
					returnStatus.rewind();
								
					for (int i=0; i<size/2; i++) {
									
						instance_id = returnStatus.next();
						keypair_name = returnStatus.next();

						if (strcmp(m_key_pair.Value(), keypair_name) == 0) {
							is_running = true;
							break;
						}
					}
								
					if ( is_running ) {

						// there is a running VM instance corresponding to the given SSH keypair
						myResource->SubmitComplete( this );
						gmState = GM_AFTER_STARTVM;
						// save the instance ID which will be used when delete VM instance
						SetInstanceId( instance_id );
									
					} else {
						// we shoudl re-start the VM again with the corresponding SSH keypair
						myResource->CancelSubmit( this );
						gmState = GM_BEFORE_STARTVM;
					}
				} else {
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job need check VM operation failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_HOLD;
				}

				}				
				
				break;			
			
			
			case GM_NEED_CHECK_KEYPAIR:
			
				{
				// check if this SSH keypair has already been registered in EC2
				StringList * returnKeys = new StringList();
							
				rc = gahp->amazon_vm_keypair_names(m_access_key_file, m_secret_key_file,
												   *returnKeys, gahp_error_code);

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
							
				// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
				// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
				// processing error code received
				if ( gahp_error_code == NULL ) {
					// go ahead
				} else {
					// print out the received error code
					print_error_code(gahp_error_code, "amazon_vm_keypair_names()");
					
					// change Job's status to HOLD since we meet the errors we cannot proceed
					gmState = GM_HOLD;
					break;
				}
							
				if (rc == 0) {
					// now we should check if this SSH keypair has already been registered in EC2
					bool is_registered = false;
					int size = returnKeys->number();
					returnKeys->rewind();
					for (int i=0; i<size; i++) {
						// this is not failure recovery, we can find keypair from m_key_pair
						if (strcmp(m_key_pair.Value(), returnKeys->next()) == 0) {
							is_registered = true;
							break;
						}
					}

					if ( is_registered ) {
						// we have registered SSH keypair successfully
						gmState = GM_AFTER_SSH_KEYPAIR;
					} else {
						// we have to redo the registering SSH keypair
						gmState = GM_BEFORE_SSH_KEYPAIR;
					}
				} else {
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job need check keypair operation failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_HOLD;
				}

				delete returnKeys;	

				}
				
				break;

			
			case GM_SUBMIT_SAVE:

				// test for AMAZON_SUBMIT_AFTER_VM (the VM does start successfully)
				// stopcode(); // test only

				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_CANCEL;
				} 
				else {
					done = requestScheddUpdate( this );
					if ( !done ) {
						// the information hasn't been changed yet, let's wait for it by redoing doEvaluateState()					
						break;
					}					
					gmState = GM_SUBMITTED;
				}

				break;
				
			
			case GM_SUBMITTED:

				if ( remoteJobState == AMAZON_VM_STATE_TERMINATED ) {
					gmState = GM_DONE_SAVE;
				} 

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CANCEL;
				} 
				else {
					if ( lastProbeTime < enteredCurrentGmState ) {
						lastProbeTime = enteredCurrentGmState;
					}
					
					// if current state isn't "running", we should check its state
					// every "funcRetryInterval" seconds. Otherwise the interval should
					// be "probeInterval" seconds.  
					int interval = probeInterval;
					if ( remoteJobState != AMAZON_VM_STATE_RUNNING ) {
						interval = funcRetryInterval;
					}
					
					if ( now >= lastProbeTime + interval ) {
						gmState = GM_PROBE_JOB;
						break;
					}
					
					unsigned int delay = 0;
					if ( (lastProbeTime + interval) > now ) {
						delay = (lastProbeTime + interval) - now;
					}
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}			

				break;
				
				
			case GM_DONE_SAVE:

				if ( condorState != HELD && condorState != REMOVED ) {
					JobTerminated();
					if ( condorState == COMPLETED ) {
						done = requestScheddUpdate( this );
						if ( !done ) {
							break;
						}
					}
				}
				
				myResource->CancelSubmit( this );
				if ( condorState == COMPLETED || condorState == REMOVED ) {
					gmState = GM_DESTROY_KEYPAIR;
				} else {
					// Clear the contact string here because it may not get
					// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
					if ( remoteJobId != NULL ) {
						SetInstanceId( NULL );
						SetKeypairId( NULL );
					}
					gmState = GM_CLEAR_REQUEST;
				}
			
				break;
						
				
			case GM_CLEAR_REQUEST:

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
				if ( remoteJobId != NULL && condorState != REMOVED 
					 && wantResubmit == 0 && doResubmit == 0 ) {
					gmState = GM_HOLD;
					break;
				}

				// Only allow a rematch *if* we are also going to perform a resubmit
				if ( wantResubmit || doResubmit ) {
					jobAd->EvalBool(ATTR_REMATCH_CHECK,NULL,wantRematch);
				}

				if ( wantResubmit ) {
					wantResubmit = 0;
					dprintf(D_ALWAYS, "(%d.%d) Resubmitting to Globus because %s==TRUE\n",
						procID.cluster, procID.proc, ATTR_GLOBUS_RESUBMIT_CHECK );
				}

				if ( doResubmit ) {
					doResubmit = 0;
					dprintf(D_ALWAYS, "(%d.%d) Resubmitting to Globus (last submit failed)\n",
						procID.cluster, procID.proc );
				}

				errorString = "";
				myResource->CancelSubmit( this );
				if ( remoteJobId != NULL ) {
					SetInstanceId( NULL );
					SetKeypairId( NULL );
				}

				JobIdle();

				if ( submitLogged ) {
					JobEvicted();
					if ( !evictLogged ) {
						WriteEvictEventToUserLog( jobAd );
						evictLogged = true;
					}
				}

				if ( wantRematch ) {
					dprintf(D_ALWAYS, "(%d.%d) Requesting schedd to rematch job because %s==TRUE\n",
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

				if ( remoteJobState != "" ) {
					remoteJobState = "";
					SetRemoteJobStatus( NULL );
				}

				submitLogged = false;
				executeLogged = false;
				submitFailedLogged = false;
				terminateLogged = false;
				abortLogged = false;
				evictLogged = false;
				gmState = GM_UNSUBMITTED;

				break;				
				
			case GM_PROBE_JOB:

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CANCEL;
				} else {
					MyString new_status;
					MyString public_dns;
					StringList returnStatus;

					// need to call amazon_vm_status(), amazon_vm_status() will check input arguments
					// The VM status we need is saved in the second string of the returned status StringList
					rc = gahp->amazon_vm_status(m_access_key_file, m_secret_key_file, remoteJobId, returnStatus, gahp_error_code );
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					}
					
					// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
					// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
					// processing error code received
					if ( gahp_error_code == NULL ) {
						// go ahead
					} else {
						// print out the received error code
						print_error_code(gahp_error_code, "amazon_vm_status()");
						
						// change Job's status to HOLD
						gmState = GM_HOLD;
						break;
					}
					
					if ( rc != 0 ) {
						// What to do about failure?
						errorString = gahp->getErrorString();
						dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s, the condor job should be removed. \n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_CANCEL;
						break;
					} else {
						if ( returnStatus.number() == 0 ) {
							// The instance has been purged, act like we
							// got back 'terminated'
							returnStatus.append( remoteJobId );
							returnStatus.append( AMAZON_VM_STATE_TERMINATED );
						}

						// VM Status is the second value in the return string list
						returnStatus.rewind();
						// jump to the value I need
						for (int i=0; i<1; i++) {
							returnStatus.next();
						}
						new_status = returnStatus.next();
						
						// if amazon VM's state is "running" or beyond,
						// change condor job status to Running.
						if ( new_status != remoteJobState &&
							 ( new_status == AMAZON_VM_STATE_RUNNING ||
							   new_status == AMAZON_VM_STATE_SHUTTINGDOWN ||
							   new_status == AMAZON_VM_STATE_TERMINATED ) ) {
							JobRunning();
						}
												
						remoteJobState = new_status;
						SetRemoteJobStatus( new_status.Value() );
										
						
						returnStatus.rewind();
						int size = returnStatus.number();
						// only when status changed to running, can we have the public dns name
						// at this situation, the number of return value is larger than 4
						if (size >=4 ) {
							for (int i=0; i<3; i++) {
								returnStatus.next();							
							}
							public_dns = returnStatus.next();
							SetRemoteVMName( public_dns.Value() );
						}
					}

					lastProbeTime = now;
					gmState = GM_SUBMITTED;
				}

				break;				
				
			case GM_CANCEL:

				// need to call amazon_vm_stop(), it will only return STOP operation is success or failed
				// amazon_vm_stop() will check the input arguments
				rc = gahp->amazon_vm_stop(m_access_key_file, m_secret_key_file, remoteJobId, gahp_error_code);
			
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				} 
				
				// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
				// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
				// processing error code received
				if ( gahp_error_code == NULL ) {
					// go ahead
				} else {
					// print out the received error code
					print_error_code(gahp_error_code, "amazon_vm_stop()");
					
					// change Job's status to CANCEL
					gmState = GM_HOLD;
					break;
				}
				
				if ( rc == 0 ) {
					// gmState = GM_FAILED;
					gmState = GM_DESTROY_KEYPAIR;
				} else {
					// What to do about a failed cancel?
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_HOLD;
				}
				
				break;
				

			case GM_CREATE_KEYPAIR:
				{
				// In state GM_BEFORE_SSH_KEYPAIR we should already create temporary key pair name
				// maybe we also need to create a temporary output file name
				
				// test for AMAZON_SUBMIT_BEFORE_SSH (the SSH didn't register successfully)
				// need to re-register again.
				// stopcode(); // test only
					
				// now create and register this keypair by using amazon_vm_create_keypair()
				rc = gahp->amazon_vm_create_keypair(m_access_key_file, m_secret_key_file, 
													m_key_pair.Value(), m_key_pair_file_name.Value(), gahp_error_code);

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}
					
				// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
				// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
				// processing error code received
				if ( gahp_error_code == NULL ) {
					// go ahead
				} else if ( strcmp(gahp_error_code, "NEED_CHECK_SSHKEY" ) == 0 ) {
						
					// get an error code from gahp server said that we should check if 
					// the SSH keypair has been registered successfully in EC2
						
					// Maxmium retry times is 3, if exceeds this limitation, we will go to HOLD state
					if ( m_keypair_check_times++ == maxRetryTimes ) {
						gmState = GM_HOLD;
					} else {
						gmState = GM_NEED_CHECK_KEYPAIR;
					}
						
					break;	
				
				} else {
					// print out the received error code
					print_error_code(gahp_error_code, "amazon_vm_create_keypair()");
				
					// change Job's status to CANCEL
					gmState = GM_HOLD;
					break;
				}
					
				if (rc == 0) {
					// let's register the security group
					gmState = GM_AFTER_SSH_KEYPAIR;
					
					// test for AMAZON_SUBMIT_BEFORE_SSH (the SSH registered successfully)
					// don't need to re-register again.
					// stopcode(); // test only
									
				} else {
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job create temporary keypair failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_HOLD;
					break;
				}
								
				}				
				
				break;


			case GM_DESTROY_KEYPAIR:
				{
				// Yes, now let's destroy the temporary keypair 
				rc = gahp->amazon_vm_destroy_keypair(m_access_key_file, m_secret_key_file, m_key_pair.Value(), gahp_error_code);

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				}

				// error_code should be checked after the return value of GAHPCLIENT_COMMAND_NOT_SUBMITTED
				// and GAHPCLIENT_COMMAND_PENDING. But before all the other return values.
					
				// processing error code received
				if ( gahp_error_code == NULL ) {
					// go ahead
				} else {
					// print out the received error code
					print_error_code(gahp_error_code, "amazon_vm_destroy_keypair()");
				
					// change Job's status to CANCEL
					gmState = GM_HOLD;
					break;
				}

				if (rc == 0) {
					// remove temporary keypair local output file
					if ( remove_keypair_file(m_key_pair_file_name.Value()) ) {
						gmState = GM_FAILED;
					} else {
						dprintf(D_ALWAYS,"(%d.%d) job destroy temporary keypair local file failed.\n", procID.cluster, procID.proc);
						gmState = GM_FAILED;
					}
					
				} else {
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job destroy temporary keypair failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_HOLD;
				}
									
				}

				break; 	
				

			case GM_HOLD:
				// Put the job on hold in the schedd.
				// If the condor state is already HELD, then someone already
				// HELD it, so don't update anything else.
				if ( condorState != HELD ) {

					// Set the hold reason as best we can
					// TODO: set the hold reason in a more robust way.
					char holdReason[1024];
					holdReason[0] = '\0';
					holdReason[sizeof(holdReason)-1] = '\0';
					jobAd->LookupString( ATTR_HOLD_REASON, holdReason, sizeof(holdReason) - 1 );
					if ( holdReason[0] == '\0' && errorString != "" ) {
						strncpy( holdReason, errorString.Value(), sizeof(holdReason) - 1 );
					} else if ( holdReason[0] == '\0' ) {
						strncpy( holdReason, "Unspecified gridmanager error", sizeof(holdReason) - 1 );
					}

					JobHeld( holdReason );
				}
			
				gmState = GM_DELETE;
				
				break;
				
				
			case GM_FAILED:

				myResource->CancelSubmit( this );
				SetInstanceId( NULL );
				SetKeypairId( NULL );

				if ( (condorState == REMOVED) || (condorState == COMPLETED) ) {
				//if (condorState == REMOVED) { // for test only
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}

				break;
				
				
			case GM_DELETE:
				
				// set remote job id to null so that schedd should remove it
				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					SetInstanceId( NULL );
					SetKeypairId( NULL );
				}
				myResource->CancelSubmit( this );
				
				// The job has completed or been removed. Delete it from the schedd.
				DoneWithJob();
				// This object will be deleted when the update occurs
				
				break;							
				
			
			default:
				EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster, procID.proc, gmState );
				break;
		} // end of switch_case
		
			// This string is used for gahp calls, but is never needed beyond
			// this point. This should really be a MyString.
		free( gahp_error_code );
		gahp_error_code = NULL;

		if ( gmState != old_gm_state ) {
			reevaluate_state = true;
			dprintf(D_FULLDEBUG, "(%d.%d) gm state change: %s -> %s\n",
					procID.cluster, procID.proc, GMStateNames[old_gm_state], GMStateNames[gmState]);
			enteredCurrentGmState = time(NULL);
		}
		
	} // end of do_while
	while ( reevaluate_state );	

	return TRUE;
}


BaseResource* AmazonJob::GetResource()
{
	return (BaseResource *)myResource;
}


// steup the public name of amazon remote VM, which can be used the clients 
void AmazonJob::SetRemoteVMName(const char * name)
{
	if ( name ) {
		jobAd->Assign( ATTR_AMAZON_REMOTE_VM_NAME, name );
	} else {
		jobAd->AssignExpr( ATTR_AMAZON_REMOTE_VM_NAME, "Undefined" );
	}
	
	requestScheddUpdate( this );
}


void AmazonJob::SetKeypairId( const char *keypair_id )
{
	if ( keypair_id == NULL ) {
		m_key_pair = "";
	} else {
		m_key_pair = keypair_id;
	}
	SetRemoteJobId( m_key_pair.Value(), remoteJobId );
}

void AmazonJob::SetInstanceId( const char *instance_id )
{
	free( remoteJobId );
	if ( instance_id ) {
		remoteJobId = strdup( instance_id );
	} else {
		remoteJobId = NULL;
	}
	SetRemoteJobId( m_key_pair.Value(), remoteJobId );
}

// SetRemoteJobId() is used to set the value of global variable "remoteJobID"
void AmazonJob::SetRemoteJobId( const char *keypair_id, const char *instance_id )
{
	MyString full_job_id;
	if ( keypair_id ) {
		full_job_id.sprintf( "amazon %s", keypair_id );
		if ( instance_id ) {
			full_job_id.sprintf_cat( " %s", instance_id );
		}
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}


// This function will change the global value of GridJobID, but will not change the value of "remoteJobID"
void AmazonJob::SetSubmitStepInfo(const char * info)
{
	char * exist_info = NULL;
	char * new_info = strdup(info);
	MyString updated_info;

	// the new submit step information will be appended to the existing info
	if ( jobAd->LookupString( "AmazonRecoverySteps", &exist_info ) ) {
		
		// should check if the last log in exist_info is same with the new one
		StringList * logs = new StringList(exist_info, " ");
		int size = logs->number();
		logs->rewind();

		for (int i=0; i<(size-1); i++) {
			logs->next();
		}
		char* last_log = strdup(logs->next());

		if (strcmp(last_log, info) == 0) {
			// if yes, don't need to add this new log.
		} else {
			// appending the new log item to the existing one
			updated_info.sprintf("%s %s", exist_info, new_info);
			jobAd->Assign(ATTR_AMAZON_RECOVERY_STEPS, updated_info.Value());
		}
		
		delete logs;
		free( last_log );
		
	} else {
		jobAd->Assign(ATTR_AMAZON_RECOVERY_STEPS, new_info);
	}
	
	requestScheddUpdate( this );
	
	free( exist_info );
	free( new_info );			
}


// private functions to construct ami_id, keypair, keypair output file and groups info from ClassAd

// if ami_id is empty, client must have assigned upload file name value
// otherwise the condor_submit will report an error.
MyString AmazonJob::build_ami_id()
{
	MyString ami_id;
	char* buffer = NULL;
	
	if ( jobAd->LookupString( ATTR_AMAZON_AMI_ID, &buffer ) ) {
		ami_id = buffer;
		free (buffer);
	}
	return ami_id;
}


MyString AmazonJob::build_keypair()
{
	// Build a name for the ssh keypair that will be unique to this job.
	// Our pattern is SSH_<collector name>_<GlobalJobId>

	// get condor pool name
	// In case there are multiple collectors, strip out the spaces
	// If there's no collector, insert a dummy name
	char* pool_name = param( "COLLECTOR_HOST" );
	if ( pool_name ) {
		StringList collectors( pool_name );
		free( pool_name );
		pool_name = collectors.print_to_string();
	} else {
		pool_name = strdup( "NoPool" );
	}

	// use "ATTR_GLOBAL_JOB_ID" to get unique global job id
	MyString job_id;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, job_id );

	MyString key_pair;
	key_pair.sprintf( "SSH_%s_%s", pool_name, job_id.Value() );

	free( pool_name );
	return key_pair;
}

StringList* AmazonJob::build_groupnames()
{
	StringList* group_names = NULL;
	char* buffer = NULL;
	
	// Notice:
	// Based on the meeting in 04/01/2008, now we will not create any temporary security groups
	// 1. clients assign ATTR_AMAZON_GROUP_NAME in condor_submit file, then we will use those 
	//    security group names.
	// 2. clients don't assign ATTR_AMAZON_GROUP_NAME in condor_submit file, then we will use
	//    the default security group (by just keeping group_names is empty).
	
	if ( jobAd->LookupString( ATTR_AMAZON_GROUP_NAME, &buffer ) ) {
		group_names = new StringList( strdup(buffer), " " );
	} else {
		group_names = new StringList();
	}
	
	free (buffer);
	
	return group_names;
}


// After keypair is destroyed, we need to call this function. In temporary keypair
// scenario, we should delete the temporarily created keypair output file.
bool AmazonJob::remove_keypair_file(const char* filename)
{
	if (filename == NULL) {
		// not create temporary keypair output file
		// return success directly.
		return true;
	} else {
		// check if the file name is what we should create
		if ( strcmp(filename, NULL_FILE) == 0 ) {
			// no need to delete it since it is /dev/null
			return true;
		} else {
			if (remove(filename) == 0) 	
				return true;
			else 
				return false;
		}
	}
}


// print out the error code received from grid_manager
void AmazonJob::print_error_code( const char* error_code,
								  const char* function_name )
{
	dprintf( D_ALWAYS, "Receiving error code = %s from function %s !", error_code, function_name );	
}

// test only, used to corrupt grid_manager
void AmazonJob::stopcode()
{
	int *p = NULL;
	*p = 0;	
}
