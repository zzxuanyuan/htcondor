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
#include "condor_config.h"
#include "string_list.h"

#include "mirrorresource.h"
#include "gridmanager.h"

template class List<MirrorJob>;
template class Item<MirrorJob>;
template class HashTable<HashKey, MirrorResource *>;
template class HashBucket<HashKey, MirrorResource *>;

#define HASH_TABLE_SIZE			500

HashTable <HashKey, MirrorResource *>
    MirrorResource::ResourcesByName( HASH_TABLE_SIZE,
									 hashFunction );

int MirrorResource::scheddPollInterval = 300;		// default value

MirrorResource *MirrorResource::FindOrCreateResource( const char * resource_name )
{
	int rc;
	MirrorResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( resource_name ), resource );
	if ( rc != 0 ) {
		resource = new MirrorResource( resource_name );
		ASSERT(resource);
		ResourcesByName.insert( HashKey( resource_name ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}

MirrorResource::MirrorResource( const char *resource_name )
	: BaseResource( resource_name )
{
	scheddPollTid = TIMER_UNSET;
	submitter_id = NULL;
	registeredJobs = new List<MirrorJob>;
	mirrorScheddName = strdup( resource_name );
	gahpA = NULL;
	gahpB = NULL;
	scheddPollActive = false;

	scheddPollTid = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&MirrorResource::DoScheddPoll,
							"MirrorResource::DoScheddPoll", (Service*)this );

	char *gahp_path = param("MIRROR_GAHP");
	if ( gahp_path == NULL ) {
		dprintf( D_ALWAYS, "MIRROR_GAHP not defined in condor config file\n" );
	} else {
		// TODO remove mirrorScheddName from the gahp server key if/when
		//   a gahp server can handle multiple schedds
		sprintf( buff, "MIRROR/%s", mirrorScheddName );
		gahpA = new GahpClient( buff, gahp_path );
		gahpA->setNotificationTimerId( scheddPollTid );
		gahpA->setMode( GahpClient::normal );
		gahpA->setTimeout( gahpCallTimeout );
		gahpB = new GahpClient( buff, gahp_path );
		gahpB->setNotificationTimerId( scheddPollTid );
		gahpB->setMode( GahpClient::normal );
		gahpB->setTimeout( gahpCallTimeout );
		free( gahp_path );
	}
}

MirrorResource::~MirrorResource()
{
	if ( scheddPollTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( scheddPollTid );
	}
	if ( submitter_id != NULL ) {
		free( submitter_id );
	}
	if ( registeredJobs != NULL ) {
		delete registeredJobs;
	}
	if ( gahpA != NULL ) {
		delete gahpA;
	}
	if ( gahpB != NULL ) {
		delete gahpB;
	}
	if ( mirrorScheddName != NULL ) {
		free( mirrorScheddName );
	}
}

bool MirrorResource::IsEmpty()
{
	return registeredJobs->IsEmpty();
}

void MirrorResource::Reconfig()
{
	BaseResource::Reconfig();
}

void MirrorResource::RegisterJob( MirrorJob *job )
{
	registeredJobs->Append( job );
}

void MirrorResource::UnregisterJob( MirrorJob *job )
{
	registeredJobs->Delete( job );

		// TODO: if this is last job, arrange to delete
		//   this object
}

int DoScheddPoll()
{
	int rcA, rcB;

	if ( registeredJobs->IsEmpty() ) {
			// No jobs, so nothing to poll/update
		daemonCore->Reset_Timer( scheddPollTid, scheddPollInterval );
		return 0;
	}

	daemonCore->Reset_Timer( scheddPollTid, TIMER_NEVER );

	if ( scheddPollActive == false ) {
		MyString buff;
		MyString constraint;

		gahpA->setMode( GahpClient::normal );
		gahpB->setMode( GahpClient::normal );

		constraint.sprintf( "%s =?= \"%s\"", "MirrorSubmitterId",
							submitter_id );

		ClassAd update_ad;
		buff.sprintf( "%s = %d", ATTR_MIRROR_LAST_REMOTE_POLL, );
		update_ad->Insert( buff.Value() );

		rcA = gahpA->condor_job_update_constrained( mirrorScheddName,
													constraint.Value(),
													&update_ad );

		if ( rcA != GAHPCLIENT_COMMAND_PENDING ) {
			dprintf( D_ALWAYS, "gahp->condor_job_update_constrained returned %d\n",
					 rcA );
		}

		rcB = gahpB->condor_job_status_constrained( mirrorScheddName,
													constraint.Value(),
													NULL, NULL );

		if ( rcB != GAHPCLIENT_COMMAND_PENDING ) {
			dprintf( D_ALWAYS, "gahp->condor_job_status_constrained returned %d\n",
					 rcB );
		}

		scheddPollActive = true;

	} else {

		int num_status_ads;
		ClassAd *status_ads = NULL;

		gahpA->setMode( GahpClient::results_only );
		gahpB->setMode( GahpClient::results_only );

		rcA = gahpA->condor_job_update_constrained( NULL, NULL, NULL );

		if ( rcA != GAHPCLIENT_COMMAND_PENDING &&
			 rcA != GAHPCLIENT_COMMAND_NOT_SUBMITTED && rcA != 0 ) {
			dprintf( D_ALWAYS, "gahp->condor_job_update_constrained returned %d\n",
					 rcA );
		}

		rcB = gahpB->condor_job_status_constrained( NULL, NULL,
													&num_status_ads,
													&status_ads );

		if ( rcB != GAHPCLIENT_COMMAND_PENDING &&
			 rcB != GAHPCLIENT_COMMAND_NOT_SUBMITTED && rcB != 0 ) {
			dprintf( D_ALWAYS, "gahp->condor_job_status_constrained returned %d\n",
					 rcA );
		}

		if ( rcB == 0 ) {
			for ( int i = 0; i < num_status_ads; i++ ) {
				int cluster, proc;
				MyString job_id_string;
				MirrorJob *job;

				status_ads[i].LookupInteger( ATTR_CLUSTER_ID, cluster );
				status_ads[i].LookupInteger( ATTR_PROC_ID, proc );

				job_id_string.sprintf( "%s/%d.%d", mirrorScheddName, cluster,
									   proc );

				rc = MirrorJobsById.lookup( HashKey( job_id_string.Value() ),
											job );
				if ( rc == 0 ) {
					job->RemoteJobStatusUpdate( &status_ads[i] );
				}
			}
		}

		if ( status_ads != NULL ) {
			delete [] status_ads;
		}

		scheddPollActive = false;

		daemonCore->Reset_Timer( scheddPollTid, scheddPollInterval );
	}

	return 0;
}
