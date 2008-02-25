/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
#include "condor_string.h"	// for strnewp and friends
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "nullfile.h"
#include "filename_tools.h"

#include "globus_utils.h"
#include "gridmanager.h"
#include "amazonjob.h"
#include "condor_config.h"
#include "globusjob.h" // for rsl_stringify()
  
// GridManager job states
// For Amazon Jobs, we only need to keep 13 states
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
#define GM_CREATE_BUCKET				13
#define GM_UPLOAD_IMAGES				14
#define GM_REGISTER_IMAGE				15
#define GM_CREATE_KEYPAIR				16
#define GM_CREATE_SG					17
#define GM_DESTROY_SG					18
#define GM_DESTROY_KEYPAIR				19
#define GM_UNREGISTER_IMAGE				20
#define GM_DESTROY_IMAGE_AND_BUCKET		21

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
	"GM_CREATE_BUCKET",
	"GM_UPLOAD_IMAGES",
	"GM_REGISTER_IMAGE",
	"GM_CREATE_KEYPAIR",
	"GM_CREATE_SG",
	"GM_DESTROY_SG",
	"GM_DESTROY_KEYPAIR",
	"GM_UNREGISTER_IMAGE",
	"GM_DESTROY_IMAGE_AND_BUCKET"
};

#define AMAZON_VM_STATE_RUNNING			"running"
#define AMAZON_VM_STATE_PENDING			"pending"
#define AMAZON_VM_STATE_SHUTTINGDOWN	"shutting-down"
#define AMAZON_VM_STATE_TERMINATED		"terminated"

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
	int tmp_int = param_integer( "GRIDMANAGER_JOB_PROBE_INTERVAL", 5 * 30 ); 
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


int AmazonJob::probeInterval = 3;	// default value
int AmazonJob::submitInterval = 300;	// default value
	
// Since some operations are time-consuming, we will set the timeout value to 5 hours
int AmazonJob::gahpCallTimeout = 21600;	// default value

int AmazonJob::maxConnectFailures = 3;	// default value

// When meet errors in creating bucket & uploading directory, we should retry several times
int AmazonJob::maxReTries = 3;


AmazonJob::AmazonJob( ClassAd *classad )
	: BaseJob( classad )
{
	char buff[4096];
	MyString error_string = "";
	char *gahp_path = NULL;
	ArgList args;
	
	remoteJobId = NULL;
	remoteJobState = "";
	gmState = GM_INIT;
	lastProbeTime = 0;
	probeNow = false;
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

	m_ami_id = NULL;
	m_key_pair = NULL;
	m_group_names = NULL;
	m_key_pair_file_name = NULL ;
	m_dir_name = NULL;
	m_xml_file = NULL;
	m_bucket_name = NULL;
		
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

	args.AppendArg("-f");
	
	gahp = new GahpClient( buff, gahp_path, &args );
	gahp->setNotificationTimerId( evaluateStateTid );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( gahpCallTimeout );

	myResource = AmazonResource::FindOrCreateResource( AMAZON_RESOURCE_NAME, m_access_key_file, m_secret_key_file );
	myResource->RegisterJob( this );

	buff[0] = '\0';
	jobAd->LookupString( ATTR_GRID_JOB_ID, buff );
	if ( strrchr( buff, ' ' ) ) {
		SetRemoteJobId( strrchr( buff, ' ' ) + 1 );
	} else {
		SetRemoteJobId( NULL );
	}
	
	jobAd->LookupString( ATTR_GRID_JOB_STATUS, remoteJobState );

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
	if ( m_access_key_file != NULL ) delete m_access_key_file;
	if ( m_secret_key_file != NULL ) delete m_secret_key_file;
	if ( m_ami_id != NULL ) delete m_ami_id;
	if ( m_key_pair != NULL ) delete m_key_pair;
	if ( m_group_names != NULL ) delete m_group_names;
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
		reevaluate_state = false;
		old_gm_state = gmState;		
		
		switch ( gmState ) 
		{
			case GM_INIT:	// GM_HOLD, GM_START
				// This is the state all jobs start in when the AmazonJob object
				// is first created. Here, we do things that we didn't want to
				// do in the constructor because they could block (the
				// constructor is called while we're connected to the schedd).
				if ( gahp->Startup() == false ) {
					dprintf( D_ALWAYS, "(%d.%d) Error starting GAHP\n", procID.cluster, procID.proc );
					jobAd->Assign( ATTR_HOLD_REASON, "Failed to start GAHP" );
					gmState = GM_HOLD;
					break;
				}
				
				gmState = GM_START;
				break;
											
			case GM_START:	// GM_CLEAR_REQUEST, GM_SUBMITTED

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

				break;
				
			case GM_UNSUBMITTED: // GM_DELETE, GM_CREATE_BUCKET	

				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_DELETE;
				} else {
					gmState = GM_CREATE_BUCKET;
				}
				
				break;
				
			case GM_SUBMIT:	// GM_HOLD, GM_UNSUBMITTED, GM_SUBMIT_SAVE

				if ( (condorState == REMOVED) || (condorState == HELD) ) {
					gmState = GM_UNSUBMITTED;
					break;
				}
				
				if ( numSubmitAttempts >= MAX_SUBMIT_ATTEMPTS ) {
					gmState = GM_HOLD;
					break;
				}
		
				// After a submit, wait at least submitInterval before trying another one.
				if ( now >= lastSubmitAttempt + submitInterval ) {

					// construct input parameters for amazon_vm_start()
					char *instance_id = NULL;
					
					// For a given Amazon Job, in its life cycle, the attributes will not change 					
					
					// m_ami_id/m_key_pair/m_group_names are NOT required variable
					if ( m_ami_id == NULL )			m_ami_id = build_ami_id();
					if ( m_key_pair == NULL )		m_key_pair = build_keypair();
					if ( m_group_names == NULL )	m_group_names = build_groupnames();
					
					// amazon_vm_start() will check the input arguments
					rc = gahp->amazon_vm_start( m_access_key_file, m_secret_key_file, 
												m_ami_id->Value(), m_key_pair->Value(), 
												*m_group_names, instance_id);
				
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						// Every first time this function will come to here, just exit doEvaluateState()
						// and later gahp_client will call this function again. At that time, we can expect
						// the return value will be success and will come to the following statements.
						break;
					}

					lastSubmitAttempt = time(NULL);
					numSubmitAttempts++;

					if ( rc == 0 ) {
						ASSERT( instance_id != NULL );
						SetRemoteJobId( instance_id );
						free( instance_id );
						gmState = GM_SUBMIT_SAVE;
					} else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job submit failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_UNSUBMITTED;
					}

				} else {
					unsigned int delay = 0;
					if ( (lastSubmitAttempt + submitInterval) > now ) {
						delay = (lastSubmitAttempt + submitInterval) - now;
					}				
					daemonCore->Reset_Timer( evaluateStateTid, delay );
				}
				
				break;
			
			case GM_SUBMIT_SAVE:	// GM_SUBMITTED, GM_CANCEL

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
			
			case GM_SUBMITTED:	// GM_CANCEL, GM_PROBE_JOB, GM_DONE_SAVE

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
					
					// change condor job status to Running.
					JobRunning();
				}			

				break;
				
			case GM_DONE_SAVE:	// GM_CLEAR_REQUEST, GM_DESTROY_SG

				if ( condorState != HELD && condorState != REMOVED ) {
					JobTerminated();
					if ( condorState == COMPLETED ) {
						done = requestScheddUpdate( this );
						if ( !done ) {
							break;
						}
					}
				}
				
				if ( condorState == COMPLETED || condorState == REMOVED ) {
					gmState = GM_DESTROY_SG;
				} else {
					// Clear the contact string here because it may not get
					// cleared in GM_CLEAR_REQUEST (it might go to GM_HOLD first).
					if ( remoteJobId != NULL ) {
						SetRemoteJobId( NULL );
					}
					gmState = GM_CLEAR_REQUEST;
				}
			
				break;				
				
			case GM_CLEAR_REQUEST:	// GM_UNSUBMITTED, GM_HOLD, GM_DELETE

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
				
			case GM_PROBE_JOB:	// GM_SUBMITTED, GM_CANCEL

				if ( condorState == REMOVED || condorState == HELD ) {
					gmState = GM_CANCEL;
				} else {
					char * new_status = NULL;
					StringList * returnStatus = new StringList();

					// need to call amazon_vm_status(), amazon_vm_status() will check input arguments
					// The VM status we need is saved in the second string of the returned status StringList
					rc = gahp->amazon_vm_status(m_access_key_file, m_secret_key_file, remoteJobId, *returnStatus );
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					} else if ( rc != 0 ) {
						// What to do about failure?
						errorString = gahp->getErrorString();
						dprintf( D_ALWAYS, "(%d.%d) job probe failed: %s, the condor job should be removed. \n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_CANCEL;
						break;
					} else {
						// VM Status is the second value in the return string list
						returnStatus->rewind();
						// jump to the value I need
						for (int i=0; i<1; i++) {
							returnStatus->next();
						}
						new_status = strdup(returnStatus->next());
						remoteJobState = new_status;
						SetRemoteJobStatus( new_status );
					}

					if ( new_status ) {
						free( new_status );
					}
					
					if ( returnStatus != NULL ) { 
						delete returnStatus;
					}
					
					lastProbeTime = now;
					gmState = GM_SUBMITTED;
				}

				break;				
				
			case GM_CANCEL:	// GM_FAILED, GM_DESTROY_SG

				// need to call amazon_vm_stop(), it will only return STOP operation is success or failed
				// amazon_vm_stop() will check the input arguments
				rc = gahp->amazon_vm_stop(m_access_key_file, m_secret_key_file, remoteJobId);
			
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				} else if ( rc == 0 ) {
					// gmState = GM_FAILED;
					gmState = GM_DESTROY_SG;
				} else {
					// What to do about a failed cancel?
					errorString = gahp->getErrorString();
					dprintf( D_ALWAYS, "(%d.%d) job cancel failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_FAILED;
				}
				
				break;
			

			case GM_CREATE_BUCKET:	// GM_CREATE_KEYPAIR, GM_HOLD, GM_UPLOAD_IMAGES
				{
				// Retry maxReTries times when meet some errors		
				int retry = maxReTries;
				
				// Check if image_names is set.
				// If yes, we need to create a temporary bucket in S3 to save these files
				m_dir_name = build_dirname();

				if ( m_dir_name != NULL ) {
					// we need to create a bucket in S3 where our image file will be saved.
					// The name of this bucket will be same as the group name
					if (!m_bucket_name) 
						m_bucket_name =	(char*)temporary_bucket_name();	
						
					// if the return value is not success, retry to create bucket
					do {

						// call gahp_server function to create a temporary bucket
						rc = gahp->amazon_vm_s3_create_bucket(m_access_key_file, m_secret_key_file, m_bucket_name);
						
						// check if the return value is success or pending
						// if yes, break the loop, else we should retry the operation 3 times.
						if ( (rc==0) || (rc==GAHPCLIENT_COMMAND_NOT_SUBMITTED) || (rc==GAHPCLIENT_COMMAND_PENDING) )
							break;
						else  
							retry--;
						
					} while( retry > 0 );
						
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					} else if ( rc == 0 ) {
						// the bucket has been created successfully in S3
						gmState = GM_UPLOAD_IMAGES;
					} else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job create temporary bucket in S3 failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_HOLD;
					}
									
				} else {
					// ami_id has been set (no need to register image in S3).
					// we can jump to GM_CREATE_KEYPAIR directly and pass
					// the steps for uploading and registering images.
					gmState = GM_CREATE_KEYPAIR;
				}
				
				}
			
				break;
			
			
			case GM_UPLOAD_IMAGES:	// GM_REGISTER_IMAGE, GM_HOLD
				{
				// Retry maxReTries times when meet some errors		
				int retry = maxReTries;
				
				
				// Don't need to check if m_dir_name is set since this job have
				// been done in state GM_CREATE_BUCKET
				
				// call gahp_server function to upload image files
				// Note:
				// amazon_vm_s3_upload_dir() can upload all the image files saved in one directory
				// to the S3. So we don't need to upload image files one by one.
				
				// if the return value is not success, retry to uploading directory
				do {
				
					rc = gahp->amazon_vm_s3_upload_dir(m_access_key_file, m_secret_key_file, m_dir_name->Value(), m_bucket_name);
					
					// check if the return value is success or pending
					// if yes, break the loop, else we should retry the operation 3 times.
					if ( (rc==0) || (rc==GAHPCLIENT_COMMAND_NOT_SUBMITTED) || (rc==GAHPCLIENT_COMMAND_PENDING) )
						break;
					else  
						retry--;
						
				} while( retry > 0 );

				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					// don't do anything, will come to next loop
				} else if ( rc == 0 ) {
					// the images have been successfully uploaded to S3's given bucket
					gmState = GM_REGISTER_IMAGE;						
				} else {
					// What to do about a failed cancel?
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job upload images to S3 failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_HOLD;
				}
				}
				
				break;


			case GM_REGISTER_IMAGE:		// GM_CREATE_KEYPAIR, GM_HOLD

				// find out the XML description file for the uploaded image
				// Note: this XML file should already be uploaded into S3 and m_xml_file should be 
				// the location in the S3.
				m_xml_file = build_xml_file(m_dir_name->Value());
				
				if (m_ami_id == NULL)
					m_ami_id = new MyString();
				
				if (m_xml_file == NULL) {
					dprintf( D_ALWAYS, "(%d.%d) job register image failed: No XML description file\n", procID.cluster, procID.proc);
					gmState = GM_HOLD;
				} else {
					char* ami_id = NULL;
					
					rc = gahp->amazon_vm_register_image(m_access_key_file, m_secret_key_file, m_xml_file, ami_id);
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						// don't do anything, will come to next loop
					} else if ( rc == 0 ) {
						*m_ami_id = strdup(ami_id); // saved ami_id of register image to a global variable 
						gmState = GM_CREATE_KEYPAIR;
						free(ami_id);
					} else {
						// What to do about a failed cancel?
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job upload images to S3 failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_HOLD;
					}
				}		

				break;		
			

			case GM_CREATE_KEYPAIR:	// GM_CREATE_SG, GM_HOLD

				// check if the clients have assigned keypair's name, if not, we should
				// create the a temporary name and temporary output file name and register it.
				// If yes, load it from JobAd's environment
	
				// Before we create the SSH keypair, we should have keypair name and output file name
				if ( m_key_pair == NULL ) {
					m_key_pair = build_keypair();
				}

				// if it is a temporary keypair name, we should create a temporary keypair output file name
				// and then we should create/register this keypair
				if ( strcmp(m_key_pair->Value(), temporary_keypair_name()) == 0 ) {

					// create temporary keypair output file name
					if ( m_key_pair_file_name == NULL ) {
						m_key_pair_file_name = build_keypairfilename();
					}

					// now create and register this keypair by using amazon_vm_create_keypair()
					rc = gahp->amazon_vm_create_keypair(m_access_key_file, m_secret_key_file, 
														m_key_pair->Value(), m_key_pair_file_name->Value());
				
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					} else if (rc == 0) {
						// let's register the security group
						gmState = GM_CREATE_SG;
					} else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job create temporary keypair failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_HOLD;
					}
				}
				else {
					// we don't need to register an existing keypair
					// come to next state directly.
					gmState = GM_CREATE_SG;
					//gmState = GM_SUBMIT; // test only
				}			
				
				break;


			case GM_CREATE_SG:	// GM_SUBMIT, GM_HOLD

				{ // add "{" here in case of  "crosses initialization" error
				
				// check if the clients have assigned a security group name, if not, we should create
				// the a temporary name and register it. If yes, just load it from JobAd's environment
				if (m_group_names == NULL) {
					m_group_names = build_groupnames();
				}
				
				// check if the security group name is temporary one
				m_group_names->rewind();
				// if we have a temporary name, it must be the first element in m_group_names
				char* sg_name = m_group_names->next();

				if (strcmp(sg_name, temporary_security_group()) == 0) {

					// prepare for the groupname and group_description
					const char * group_description = "temporary security group name created by Condor"; 
	
					// now add this temporary group name							
					rc = gahp->amazon_vm_create_group(m_access_key_file, m_secret_key_file, sg_name, group_description);
							
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					} else if (rc == 0) {
						// register the security group successfully
						gmState = GM_SUBMIT;
					} else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job create temporary security group failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_HOLD;
					}
				} else {
					// security group name is provided by client
					// come to next state directly
					gmState = GM_SUBMIT;
				}
				
				}
				
				break; 	
			

			case GM_DESTROY_SG:		// GM_DESTROY_KEYPAIR, GM_FAILED
				{
				// first, we should check if the current group name is temporary group name
				m_group_names->rewind();
				// If we use temporary group name, it must be saved at the first element in the StringList
				char* current_group_name = m_group_names->next(); 
				
				if (strcmp(current_group_name, temporary_security_group()) == 0) {
					// yes, EC2 is using temporary group name
					rc = gahp->amazon_vm_delete_group(m_access_key_file, m_secret_key_file, current_group_name);
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					} else if (rc == 0) {
						// let's destroy the key pair
						gmState = GM_DESTROY_KEYPAIR;
					} else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job destroy temporary security group failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_FAILED;
					}					
				} else {
					// no, EC2 is using the group name provided by client
					// don't need to do any extra work, just come to GM_DESTROY_KEYPAIR directly
					gmState = GM_DESTROY_KEYPAIR;
				}
				
				}
				break;	
			

			case GM_DESTROY_KEYPAIR:	// GM_UNREGISTER_IMAGE, GM_FAILED
				{
				// check if current keypair is a temporary keypair
				if ( strcmp(m_key_pair->Value(), temporary_keypair_name()) == 0 ) {
					
					// now let's destroy the temporary keypair 
					rc = gahp->amazon_vm_destroy_keypair(m_access_key_file, m_secret_key_file, m_key_pair->Value());
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					} else if (rc == 0) {
						// when we registered a temporary keypair, a temporary file will also
						// be created at the local disk. Now we need to remove it.
						rc = remove(m_key_pair_file_name->Value());
						if ( rc != 0 ) {
							dprintf(D_ALWAYS,"(%d.%d) job destroy temporary keypair local file failed.\n", procID.cluster, procID.proc);
							gmState = GM_FAILED;
						} else {
							gmState = GM_UNREGISTER_IMAGE;
						}
					} else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job destroy temporary keypair failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_FAILED;
					}
					
				} else {
					// keypair is provided by client, don't need to do anything here
					gmState = GM_UNREGISTER_IMAGE;
				}
					
				}

				break; 	
			

			case GM_UNREGISTER_IMAGE:	// GM_DELETE, GM_DESTROY_IMAGE_AND_BUCKET
				{
				// check if the ami_id is provided by client or from registeration, if by client,  
				// we can skip states GM_UNREGISTER_IMAGE, GM_DESTROY_IMAGE and GM_DESTROY_BUCKET
				
				// should check if m_dir_name is empty. don't need to check if m_ami_id is empty since after
				// state GM_REGISTER_IMAGE, this variable should have been assigned.
				if ( m_dir_name != NULL ) {
					// should do lots of things: deregister image, download images and destroy bucket
					
					rc = gahp->amazon_vm_deregister_image(m_access_key_file, m_secret_key_file, m_ami_id->Value());
					
					if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
						break;
					} else if (rc == 0) {
						// let's destroy the key pair
						gmState = GM_DESTROY_IMAGE_AND_BUCKET;
					} else {
						errorString = gahp->getErrorString();
						dprintf(D_ALWAYS,"(%d.%d) job deregister image failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
						gmState = GM_FAILED;
					}
				
				} else {
					// ami_id is provided by client, don't need to anything 
					// jump to GM_FAILED directly
					gmState = GM_FAILED;
				}
				
				}
				
				break;	
				
	
			case GM_DESTROY_IMAGE_AND_BUCKET:	// GM_FAILED
				// remove the temporary bucket from S3 after the image files have been deleted from S3
				rc = gahp->amazon_vm_s3_delete_bucket(m_access_key_file, m_secret_key_file, m_bucket_name);
				
				if ( rc == GAHPCLIENT_COMMAND_NOT_SUBMITTED || rc == GAHPCLIENT_COMMAND_PENDING ) {
					break;
				} else if ( rc == 0 ) {
					// the bucket has been created successfully in S3
					gmState = GM_FAILED;
				} else {
					// What to do about a failed cancel?
					errorString = gahp->getErrorString();
					dprintf(D_ALWAYS,"(%d.%d) job destroy temporary bucket in S3 failed: %s\n", procID.cluster, procID.proc, errorString.Value() );
					gmState = GM_FAILED;
				}

				break;	
				

			case GM_HOLD:	// GM_DELETE
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
					}
					if ( holdReason[0] == '\0' ) {
						strncpy( holdReason, "Unspecified gridmanager error", sizeof(holdReason) - 1 );
					}

					JobHeld( holdReason );
				}
			
				gmState = GM_DELETE;
						
				break;
				
			case GM_FAILED:	// GM_CLEAR_REQUEST, GM_DELETE

				SetRemoteJobId( NULL );

				if ( (condorState == REMOVED) || (condorState == COMPLETED) ) {
				//if (condorState == REMOVED) { // for test only
					gmState = GM_DELETE;
				} else {
					gmState = GM_CLEAR_REQUEST;
				}

				break;
				
			case GM_DELETE:

				// The job has completed or been removed. Delete it from the schedd.
				DoneWithJob();
				// This object will be deleted when the update occurs
				
				break;							
			
			default:
				EXCEPT( "(%d.%d) Unknown gmState %d!", procID.cluster, procID.proc, gmState );
				break;
		} // end of switch_case
		
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


void AmazonJob::SetRemoteJobId( const char *job_id )
{
	free( remoteJobId );
	
	if ( job_id ) {
		remoteJobId = strdup( job_id );
	} else {
		remoteJobId = NULL;
	}

	MyString full_job_id;
	if ( job_id ) {
		full_job_id.sprintf( "amazon %s %s", AMAZON_RESOURCE_NAME, job_id );
	}
	BaseJob::SetRemoteJobId( full_job_id.Value() );
}


// private functions to construct ami_id, keypair, keypair output file and groups info from ClassAd

// if ami_id is empty, client must have assigned upload file name value
// otherwise the condor_submit will report an error.
MyString* AmazonJob::build_ami_id()
{
	MyString* ami_id = new MyString();
	char* buffer = NULL;
	
	if ( jobAd->LookupString( ATTR_AMAZON_AMI_ID, &buffer ) ) {
		*ami_id = buffer;
		free (buffer);
	}
	return ami_id;
}


MyString* AmazonJob::build_keypair()
{
	MyString* key_pair = new MyString();
	char* buffer = NULL;
	
	if ( jobAd->LookupString( ATTR_AMAZON_KEY_PAIR, &buffer ) ) {
		*key_pair = buffer;
	} else {
		// If client doesn't assign keypair name, we will create a temporary one
		// Note: keypair name = SSH_ + condor_pool_name + job_id
		*key_pair = temporary_keypair_name();
	}
	
	free (buffer);
	
	return key_pair;
}

MyString* AmazonJob::build_keypairfilename()
{
	MyString* file_name = new MyString();
	char* buffer = NULL;
	
	if ( jobAd->LookupString( ATTR_AMAZON_KEY_PAIR_FILE_NAME, &buffer ) ) {
		// clinet define the location where this SSH keypair file will be written to
		*file_name = buffer;
	} else {
		// If client doesn't assign keypair output file name, we will create a temporary one 
		// Note: keypair output filename = SSHFILE_ condor_pool_name + job_id
		
		// This is a temporary solution, just for testing. In normal situation, we should
		// NOT place this SSH keypair file in the /tmp directory
		*file_name = temporary_keypair_file();
	}
	
	free (buffer);
	
	return file_name;
}

// If upload directory name is empty, client must have assigned ami_id value
// otherwise the condor_submit will report an error.
// Client will save the image files in a directory. One of these file should 
// be an XML file, which is used to describe the image. Later when we try to register
// image file in EC2, we should use this XML file.
MyString* AmazonJob::build_dirname()
{
	MyString* dir_name = new MyString();
	char* buffer = NULL;
	
	if ( jobAd->LookupString( ATTR_AMAZON_UPLOAD_DIR_NAME, &buffer ) ) {
		*dir_name = buffer;
	} else {
		// client doesn't assign directory name
	}
	
	free (buffer);

	return dir_name;	
}


// find out the XML description file from the given directory
char* AmazonJob::build_xml_file(const char* dirname)
{
	StringList* xml_names = new StringList();
	
	if ( suffix_matched_files_in_dir(dirname, *xml_names, "xml", true) == true )
	{
		// we suppose only one XML file exists in the uploading directory
		// No matter how many XML files we got, we only return the first one
		xml_names->rewind();
		char* xml_name = (char*) condor_basename(xml_names->next());
		
		// create the S3 Location
		MyString image_location;	
		image_location.sprintf("/%s/%s", m_bucket_name, xml_name);
		return strdup(image_location.Value());
	}
	else
	{
		// cannot find the XML file, return NULL
		return NULL;
	}
}


StringList* AmazonJob::build_groupnames()
{
	StringList* group_names = NULL;
	char* buffer = NULL;
	
	if ( jobAd->LookupString( ATTR_AMAZON_GROUP_NAME, &buffer ) ) {
		group_names = new StringList( buffer, " " );
	} else {
		// If client doesn't assign a group name, we will create a temporary
		// security group name for it.
		// Note: Name = SG_ + condor_pool_name + job_id
		const char* temp_name = temporary_security_group();
		group_names = new StringList();
		group_names->append(temp_name); // when test, comment this line
	}
	
	free (buffer);
	
	return group_names;
}


const char* AmazonJob::temporary_keypair_name()
{
	// Note: keypair name = SSH_ + condor_pool_name + job_id
	MyString keypair_name;
	
	// construct the temporary keypair name
	keypair_name.sprintf("SSH_%s", get_common_temp_name());
	return strdup(keypair_name.Value());
}


const char* AmazonJob::temporary_keypair_file()
{
	// Note: keypair output file name = SSHFILE_ condor_pool_name + job_id
	MyString keypair_file;

	// construct the temporary keypair name
	// now we save these temporary files at directory "/tmp"
	keypair_file.sprintf("/tmp/SSHFILE_%s", get_common_temp_name());
	return strdup(keypair_file.Value());
}


const char* AmazonJob::temporary_security_group()
{
	// Note: Name = SG_ + condor_pool_name + job_id
	MyString security_group;
	
	// construct the temporary keypair name
	security_group.sprintf("SG_%s", get_common_temp_name());
	return strdup(security_group.Value());
}

const char* AmazonJob::temporary_bucket_name()
{
	// Note: Name = Condor_ + Random characters
	MyString random;
	random.randomlyGenerateHex(6);
	
	// Condor bucket names cannot contain: Captial, *, :, ., etc
	
	MyString bucket_name;
	bucket_name.sprintf("condor_%s", random.Value());
	return strdup(bucket_name.Value());
}


const char* AmazonJob::get_common_temp_name()
{
	MyString temp_name;
	
	// get condor pool name
	char* pool_name = param( "COLLECTOR_HOST" );

	// use "ATTR_GLOBAL_JOB_ID" to get unique global job id
	char* job_id = NULL;
	jobAd->LookupString( ATTR_GLOBAL_JOB_ID, &job_id );

	// construct the temporary name
	temp_name.sprintf("%s_%s", pool_name, job_id);
	return strdup(temp_name.Value());	
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
		if ( strcmp(filename, (const char*)temporary_keypair_file()) != 0 ) {
			return false;
		}
		
		// delete this file
		if (remove(filename) == 0) {
			return true;
		} else {
			return false;
		}
	}
}
