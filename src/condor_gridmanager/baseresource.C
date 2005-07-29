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

#include "globusresource.h"
#include "gridmanager.h"

#define DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE	5
#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100

int BaseResource::probeInterval = 300;	// default value
int BaseResource::probeDelay = 15;		// default value

BaseResource::BaseResource( const char *resource_name )
{
	resourceName = strdup( resource_name );

	resourceDown = false;
	firstPingDone = false;
	pingActive = false;
	pingTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&BaseResource::Ping,
								"BaseResource::Ping", (Service*)this );
	lastPing = 0;
	lastStatusChange = 0;

	submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;

	updateLeasesTimerId = daemonCore->Register_Timer( 0,
								(TimerHandlercpp)&BaseResource::UpdateLeases,
								"BaseResource::UpdateLeases", (Service*)this );
	lastUpdateLeases = 0;
	updateLeasesActive = false;
	leaseAttrsSynched = false;
	updateLeasesCmdActive = false;
}

BaseResource::~BaseResource()
{
 	daemonCore->Cancel_Timer( pingTimerId );
	daemonCore->Cancel_Timer( updateLeasesTimerId );
	if ( resourceName != NULL ) {
		free( resourceName );
	}
}

void BaseResource::Reconfig()
{
	int tmp_int;
	char *param_value;

	tmp_int = param_integer( "GRIDMANAGER_RESOURCE_PROBE_INTERVAL", 5 * 60 );
	setProbeInterval( tmp_int );

	submitLimit = -1;
	param_value = param( "GRIDMANAGER_MAX_PENDING_SUBMITS_PER_RESOURCE" );
	if ( param_value == NULL ) {
		// Check old parameter name
		param_value = param( "GRIDMANAGER_MAX_PENDING_SUBMITS" );
	}
	if ( param_value != NULL ) {
		char *tmp1;
		char *tmp2;
		StringList limits( param_value );
		limits.rewind();
		if ( limits.number() > 0 ) {
			submitLimit = atoi( limits.next() );
			while ( (tmp1 = limits.next()) && (tmp2 = limits.next()) ) {
				if ( strcmp( tmp1, resourceName ) == 0 ) {
					submitLimit = atoi( tmp2 );
				}
			}
		}
		free( param_value );
	}
	if ( submitLimit <= 0 ) {
		submitLimit = DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE;
	}

	jobLimit = -1;
	param_value = param( "GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE" );
	if ( param_value != NULL ) {
		char *tmp1;
		char *tmp2;
		StringList limits( param_value );
		limits.rewind();
		if ( limits.number() > 0 ) {
			jobLimit = atoi( limits.next() );
			while ( (tmp1 = limits.next()) && (tmp2 = limits.next()) ) {
				if ( strcmp( tmp1, resourceName ) == 0 ) {
					jobLimit = atoi( tmp2 );
				}
			}
		}
		free( param_value );
	}
	if ( jobLimit <= 0 ) {
		jobLimit = DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE;
	}

	// If the jobLimit was widened, move jobs from Wanted to Allowed and
	// signal them
	while ( submitsAllowed.Length() < jobLimit &&
			submitsWanted.Length() > 0 ) {
		BaseJob *wanted_job = submitsWanted.Head();
		submitsWanted.Delete( wanted_job );
		submitsAllowed.Append( wanted_job );
		wanted_job->SetEvaluateState();
	}

	// If the submitLimit was widened, move jobs from Queued to In-Progress
	while ( submitsInProgress.Length() < submitLimit &&
			submitsQueued.Length() > 0 ) {
		BaseJob *queued_job = submitsQueued.Head();
		submitsQueued.Delete( queued_job );
		submitsInProgress.Append( queued_job );
		queued_job->SetEvaluateState();
	}
}

char *BaseResource::ResourceName()
{
	return resourceName;
}

void BaseResource::RegisterJob( BaseJob *job )
{
	registeredJobs.Append( job );

	if ( firstPingDone == true ) {
		if ( resourceDown ) {
			job->NotifyResourceDown();
		} else {
			job->NotifyResourceUp();
		}
	}
}

void BaseResource::UnregisterJob( BaseJob *job )
{
	CancelSubmit( job );

	pingRequesters.Delete( job );
	registeredJobs.Delete( job );

	if ( IsEmpty() ) {
		delete this;
	}
}

bool BaseResource::IsEmpty()
{
	return registeredJobs.IsEmpty();
}

void BaseResource::RequestPing( BaseJob *job )
{
	pingRequesters.Append( job );

	daemonCore->Reset_Timer( pingTimerId, 0 );
}

bool BaseResource::RequestSubmit( BaseJob *job )
{
	bool already_allowed = false;
	BaseJob *jobptr;

	submitsQueued.Rewind();
	while ( submitsQueued.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsInProgress.Rewind();
	while ( submitsInProgress.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return true;
		}
	}

	submitsWanted.Rewind();
	while ( submitsWanted.Next( jobptr ) ) {
		if ( jobptr == job ) {
			return false;
		}
	}

	submitsAllowed.Rewind();
	while ( submitsAllowed.Next( jobptr ) ) {
		if ( jobptr == job ) {
			already_allowed = true;
			break;
		}
	}

	if ( already_allowed == false ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			EXCEPT("In BaseResource for %s, SubmitsWanted is not empty and SubmitsAllowed is not full\n",resourceName);
		}
		if ( submitsAllowed.Length() < jobLimit ) {
			submitsAllowed.Append( job );
			// proceed to see if submitLimit applies
		} else {
			submitsWanted.Append( job );
			return false;
		}
	}

	if ( submitsInProgress.Length() < submitLimit &&
		 submitsQueued.Length() > 0 ) {
		EXCEPT("In BaseResource for %s, SubmitsQueued is not empty and SubmitsToProgress is not full\n",resourceName);
	}
	if ( submitsInProgress.Length() < submitLimit ) {
		submitsInProgress.Append( job );
		return true;
	} else {
		submitsQueued.Append( job );
		return false;
	}
}

void BaseResource::SubmitComplete( BaseJob *job )
{
	if ( submitsInProgress.Delete( job ) ) {
		if ( submitsInProgress.Length() < submitLimit &&
			 submitsQueued.Length() > 0 ) {
			BaseJob *queued_job = submitsQueued.Head();
			submitsQueued.Delete( queued_job );
			submitsInProgress.Append( queued_job );
			queued_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsQueued if the job wasn't in
		// submitsInProgress.
		submitsQueued.Delete( job );
	}

	return;
}

void BaseResource::CancelSubmit( BaseJob *job )
{
	if ( submitsAllowed.Delete( job ) ) {
		if ( submitsAllowed.Length() < jobLimit &&
			 submitsWanted.Length() > 0 ) {
			BaseJob *wanted_job = submitsWanted.Head();
			submitsWanted.Delete( wanted_job );
			submitsAllowed.Append( wanted_job );
			wanted_job->SetEvaluateState();
		}
	} else {
		// We only have to check submitsWanted if the job wasn't in
		// submitsAllowed.
		submitsWanted.Delete( job );
	}

	SubmitComplete( job );

	return;
}

void BaseResource::AlreadySubmitted( BaseJob *job )
{
	submitsAllowed.Append( job );
}

int BaseResource::Ping()
{
	BaseJob *job;

	// Don't perform a ping if we have no requesters and the resource is up
	if ( pingRequesters.IsEmpty() && resourceDown == false &&
		 firstPingDone == true && pingActive == false ) {
		daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );
		return TRUE;
	}

	// Don't start a new ping too soon after the previous one. If the
	// resource is up, the minimum time between pings is probeDelay. If the
	// resource is down, the minimum time between pings is probeInterval.
	int delay;
	if ( resourceDown == false ) {
		delay = (lastPing + probeDelay) - time(NULL);
	} else {
		delay = (lastPing + probeInterval) - time(NULL);
	}
	if ( delay > 0 ) {
		daemonCore->Reset_Timer( pingTimerId, delay );
		return TRUE;
	}

	daemonCore->Reset_Timer( pingTimerId, TIMER_NEVER );

	time_t ping_delay;
	bool ping_complete;
	bool ping_succeeded;
	DoPing( ping_delay, ping_complete, ping_succeeded );

	if ( ping_delay ) {
		daemonCore->Reset_Timer( pingTimerId, ping_delay );
		return TRUE;
	}

	if ( !ping_complete ) {
		pingActive = true;
		return TRUE;
	}

	pingActive = false;
	lastPing = time(NULL);

	if ( ping_succeeded != resourceDown && firstPingDone == true ) {
		// State of resource hasn't changed. Notify ping requesters only.
		dprintf(D_ALWAYS,"resource %s is still %s\n",resourceName,
				ping_succeeded?"up":"down");

		pingRequesters.Rewind();
		while ( pingRequesters.Next( job ) ) {
			pingRequesters.DeleteCurrent();
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}
	} else {
		// State of resource has changed. Notify every job.
		dprintf(D_ALWAYS,"resource %s is now %s\n",resourceName,
				ping_succeeded?"up":"down");

		resourceDown = !ping_succeeded;
		lastStatusChange = lastPing;

		firstPingDone = true;

		registeredJobs.Rewind();
		while ( registeredJobs.Next( job ) ) {
			if ( resourceDown ) {
				job->NotifyResourceDown();
			} else {
				job->NotifyResourceUp();
			}
		}

		pingRequesters.Rewind();
		while ( pingRequesters.Next( job ) ) {
			pingRequesters.DeleteCurrent();
		}
	}

	if ( resourceDown ) {
		daemonCore->Reset_Timer( pingTimerId, probeInterval );
	}

	return 0;
}

void BaseResource::DoPing( time_t& ping_delay, bool& ping_complete,
						   bool& ping_succeeded )
{
	ping_delay = 0;
	ping_complete = true;
	ping_succeeded = true;
}

bool CalculateLease( const ClassAd *job_ad, time_t &new_renewal,
					 int &new_duration )
{
	time_t renewal_received = -1;
	int duration_received = -1;
	time_t renewal_sent = -1;
	int duration_sent = -1;
	bool last_renewal_failed = false;

	job_ad->LookupInteger( ATTR_LAST_JOB_LEASE_RENEWAL_RECEIVED,
						   (int)renewal_received );
	job_ad->LookupInteger( ATTR_JOB_LEASE_DURATION_RECEIVED,
						   duration_received );
	job_ad->LookupInteger( ATTR_LAST_JOB_LEASE_RENEWAL, (int)renewal_sent );
	job_ad->LookupInteger( ATTR_JOB_LEASE_DURATION, duration_sent );
	job_ad->LookupBool( ATTR_LAST_JOB_LEASE_RENEWAL_FAILED,
						last_renewal_failed );

		// If we didn't received a lease, there's no lease to renew
	if ( renewal_received == -1 || duration_received == -1 ) {
		return false;
	}

		// If the lease we sent expires within 10 seconds of the lease we
		// received, don't renew it.
	if ( renewal_sent + duration_sent + 10 >=
		 renewal_received + duration_received ) {
		return false;
	}

	if ( new_renewal == 0 ) {
		new_renewal = time(NULL);
	}

	if ( last_renewal_failed ) {
		new_duration = ( renewal_sent + duration_sent ) - new_renewal;
	} else {
		new_duration = ( renewal_received + duration_received ) - new_renewal;
	}

	return true;
}

int BaseResource::UpdateLeases()
{
	// Don't start a new lease update too soon after the previous one.
	int delay;
	delay = (lastUpdateLeases + 60) - time(NULL);
	if ( delay > 0 ) {
		daemonCore->Reset_Timer( updateLeasesTimerId, delay );
		return TRUE;
	}

	daemonCore->Reset_Timer( updateLeasesTimerId, TIMER_NEVER );

	if ( updateLeasesActive == false ) {
		newLeaseRenewTime = time(NULL);
		BaseJob *curr_job;
		registeredJobs.Rewind();
		while ( registeredJobs.Next( curr_job ) ) {
			int new_duration;
			if ( CalculateLease( curr_job->jobAd, newLeaseRenewTime,
								 new_duration ) ) {
				curr_job->jobAd->Assign( ATTR_LAST_JOB_LEASE_RENEWAL,
										 (int)newLeaseRenewTime );
				curr_job->jobAd->Assign( ATTR_JOB_LEASE_DURATION,
										 new_duration );
				requestScheddUpdate( curr_job );
				leaseUpdates.Append( curr_job );
			}
		}
		if ( leaseUpdates.IsEmpty() ) {
			lastUpdateLeases = time(NULL);
			daemonCore->Reset_Timer( updateLeasesTimerId, 60 );
		} else {
			requestScheddUpdateNotification( updateLeasesTimerId );
			updateLeasesActive = true;
			leaseAttrsSynched = false;
		}
		return TRUE;
	}

	if ( leaseAttrsSynched == false ) {
		bool still_dirty = false;
		BaseJob *curr_job;
		leaseUpdates.Rewind();
		while ( leaseUpdates.Next( curr_job ) ) {
			bool exists1, exists2;
			bool dirty1, dirty2;
			curr_job->jobAd->GetDirtyFlag( ATTR_LAST_JOB_LEASE_RENEWAL, &exists1,
										   &dirty1 );
			curr_job->jobAd->GetDirtyFlag( ATTR_JOB_LEASE_DURATION, &exists2,
										   &dirty2 );
			if ( !exists1 || !exists2 ) {
					// What!? The attribute disappeared? Forget about renewing
					// the lease then
				dprintf( D_ALWAYS, "Lease attributes disappeared for job %d.%d, ignoring it\n",
						 curr_job->procID.cluster, curr_job->procID.proc );
				leaseUpdates.DeleteCurrent();
			}
			if ( dirty1 || dirty2 ) {
				still_dirty = true;
				requestScheddUpdate( curr_job );
			}
		}
		if ( still_dirty ) {
			requestScheddUpdateNotification( updateLeasesTimerId );
			return TRUE;
		}
	}

	leaseAttrsSynched = true;

	time_t update_delay;
	bool update_complete;
	SimpleList<PROC_ID> update_succeeded;
	DoUpdateLeases( update_delay, update_complete, update_succeeded );

	if ( update_delay ) {
		daemonCore->Reset_Timer( updateLeasesTimerId, update_delay );
		return TRUE;
	}

	if ( !update_complete ) {
		updateLeasesCmdActive = true;
		return TRUE;
	}

	updateLeasesCmdActive = false;
	lastUpdateLeases = time(NULL);

	BaseJob *curr_job;
	leaseUpdates.Rewind();
	while ( leaseUpdates.Next( curr_job ) ) {
		bool curr_renewal_failed;
		bool last_renewal_failed = false;
		if ( update_succeeded.IsMember( curr_job->procID ) ) {
			curr_renewal_failed = false;
		} else {
			curr_renewal_failed = true;
		}
		curr_job->jobAd->LookupBool( ATTR_LAST_JOB_LEASE_RENEWAL_FAILED,
									 last_renewal_failed );
		if ( curr_renewal_failed != last_renewal_failed ) {
			curr_job->jobAd->Assign( ATTR_LAST_JOB_LEASE_RENEWAL_FAILED,
									 curr_renewal_failed );
			requestScheddUpdate( curr_job );
		}
		leaseUpdates.DeleteCurrent();
	}

	return 0;
}

void BaseResource::DoUpdateLeases( time_t& update_delay,
								   bool& update_complete,
								   SimpleList<PROC_ID>& update_succeeded )
{
	update_delay = 0;
	update_complete = true;
}
