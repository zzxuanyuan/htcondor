/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "format_time.h"  // for format_time and friends
#include "daemon.h"
#include "dc_schedd.h"
#include "condor_email.h"

#include "gridmanager.h"
#include "gahp-client.h"

#include "globusjob.h"

#if defined(ORACLE_UNIVERSE)
#include "oraclejob.h"
#endif

#if defined(NORDUGRID_UNIVERSE)
#include "nordugridjob.h"
#endif

#include "gt3job.h"

#define QMGMT_TIMEOUT 15

#define UPDATE_SCHEDD_DELAY		5

#define HASH_TABLE_SIZE			500

extern char *myUserName;

struct JobType
{
	char *Name;
	void(*InitFunc)();
	void(*ReconfigFunc)();
	const char *AdMatchConst;
	bool(*AdMustExpandFunc)(const ClassAd*);
	BaseJob *(*CreateFunc)(ClassAd*);
};

List<JobType> jobTypes;

// Stole these out of the schedd code
int procIDHash( const PROC_ID &procID, int numBuckets )
{
	//dprintf(D_ALWAYS,"procIDHash: cluster=%d proc=%d numBuck=%d\n",procID.cluster,procID.proc,numBuckets);
	return ( (procID.cluster+(procID.proc*19)) % numBuckets );
}

bool operator==( const PROC_ID a, const PROC_ID b)
{
	return a.cluster == b.cluster && a.proc == b.proc;
}

template class HashTable<PROC_ID, BaseJob *>;
template class HashBucket<PROC_ID, BaseJob *>;
template class List<BaseJob>;
template class Item<BaseJob>;
template class List<JobType>;
template class Item<JobType>;

HashTable <PROC_ID, BaseJob *> pendingScheddUpdates( HASH_TABLE_SIZE,
													 procIDHash );
bool addJobsSignaled = false;
bool removeJobsSignaled = false;
int contactScheddTid = TIMER_UNSET;
int contactScheddDelay;
time_t lastContactSchedd = 0;

char *ScheddAddr = NULL;
char *ScheddJobConstraint = NULL;
char *GridmanagerScratchDir = NULL;

HashTable <PROC_ID, BaseJob *> JobsByProcID( HASH_TABLE_SIZE,
											 procIDHash );

static void EmailTerminateEvent(ClassAd * jobAd, bool exit_status_valid);

bool firstScheddContact = true;

char *Owner = NULL;

void RequestContactSchedd();
int doContactSchedd();

// handlers
int ADD_JOBS_signalHandler( int );
int REMOVE_JOBS_signalHandler( int );


bool JobMatchesConstraint( const ClassAd *jobad, const char *constraint )
{
	ExprTree *tree;
	EvalResult *val;

	val = new EvalResult;

	Parse( constraint, tree );
	if ( tree == NULL ) {
		dprintf( D_FULLDEBUG,
				 "Parse() returned a NULL tree on constraint '%s'\n",
				 constraint );
		return false;
	}
	tree->EvalTree(jobad, val);           // evaluate the constraint.
	if(!val || val->type != LX_INTEGER) {
		delete tree;
		delete val;
		dprintf( D_FULLDEBUG, "Constraint '%s' evaluated to wrong type\n",
				 constraint );
		return false;
	} else {
        if( !val->i ) {
			delete tree;
			delete val;
			return false; 
		}
	}

	delete tree;
	delete val;
	return true;
}

// Job objects should call this function when they have changes that need
// to be propagated to the schedd.
// return value of true means requested update has been committed to schedd.
// return value of false means requested update has been queued, but has not
//   been committed to the schedd yet
bool
requestScheddUpdate( BaseJob *job )
{
	BaseJob *hashed_job;

	// Check if there's anything that actually requires contacting the
	// schedd. If not, just return true (i.e. update is complete)
	job->ad->ResetExpr();
	if ( job->deleteFromGridmanager == false &&
		 job->deleteFromSchedd == false &&
		 job->ad->NextDirtyExpr() == NULL ) {
		return true;
	}

	// Check if the job is already in the hash table
	if ( pendingScheddUpdates.lookup( job->procID, hashed_job ) != 0 ) {

		pendingScheddUpdates.insert( job->procID, job );
		RequestContactSchedd();
	}

	return false;
}

void
RequestContactSchedd()
{
	if ( contactScheddTid == TIMER_UNSET ) {
		time_t now = time(NULL);
		time_t delay = 0;
		if ( lastContactSchedd + contactScheddDelay > now ) {
			delay = (lastContactSchedd + contactScheddDelay) - now;
		}
		contactScheddTid = daemonCore->Register_Timer( delay,
												(TimerHandler)&doContactSchedd,
												"doContactSchedd", NULL );
	}
}

void
Init()
{
	pid_t schedd_pid;

	// schedd address may be overridden by a commandline option
	// only set it if it hasn't been set already
	if ( ScheddAddr == NULL ) {
		schedd_pid = daemonCore->getppid();
		ScheddAddr = daemonCore->InfoCommandSinfulString( schedd_pid );
		if ( ScheddAddr == NULL ) {
			EXCEPT( "Failed to determine schedd's address" );
		} else {
			ScheddAddr = strdup( ScheddAddr );
		}
	}

	// read config file
	// initialize variables

	Owner = my_username();
	if ( Owner == NULL ) {
		EXCEPT( "Can't determine username" );
	}

	if ( GridmanagerScratchDir == NULL ) {
		EXCEPT( "Schedd didn't specify scratch dir with -S" );
	}

	if ( InitializeProxyManager( GridmanagerScratchDir ) == false ) {
		EXCEPT( "Failed to initialize Proxymanager" );
	}

	JobType *new_type;

#if defined(ORACLE_UNIVERSE)
	new_type = new JobType;
	new_type->Name = strdup( "Oracle" );
	new_type->InitFunc = OracleJobInit;
	new_type->ReconfigFunc = OracleJobReconfig;
	new_type->AdMatchConst = OracleJobAdConst;
	new_type->AdMustExpandFunc = OracleJobAdMustExpand;
	new_type->CreateFunc = OracleJobCreate;
	jobTypes.Append( new_type );
#endif

#if defined(NORDUGRID_UNIVERSE)
	new_type = new JobType;
	new_type->Name = strdup( "Nordugrid" );
	new_type->InitFunc = NordugridJobInit;
	new_type->ReconfigFunc = NordugridJobReconfig;
	new_type->AdMatchConst = NordugridJobAdConst;
	new_type->AdMustExpandFunc = NordugridJobAdMustExpand;
	new_type->CreateFunc = NordugridJobCreate;
	jobTypes.Append( new_type );
#endif

	new_type = new JobType;
	new_type->Name = strdup( "GT3" );
	new_type->InitFunc = GT3JobInit;
	new_type->ReconfigFunc = GT3JobReconfig;
	new_type->AdMatchConst = GT3JobAdConst;
	new_type->AdMustExpandFunc = GT3JobAdMustExpand;
	new_type->CreateFunc = GT3JobCreate;
	jobTypes.Append( new_type );

	new_type = new JobType;
	new_type->Name = strdup( "Globus" );
	new_type->InitFunc = GlobusJobInit;
	new_type->ReconfigFunc = GlobusJobReconfig;
	new_type->AdMatchConst = GlobusJobAdConst;
	new_type->AdMustExpandFunc = GlobusJobAdMustExpand;
	new_type->CreateFunc = GlobusJobCreate;
	jobTypes.Append( new_type );

	jobTypes.Rewind();
	while ( jobTypes.Next( new_type ) ) {
		new_type->InitFunc();
	}

}

void
Register()
{
	daemonCore->Register_Signal( GRIDMAN_ADD_JOBS, "AddJobs",
								 (SignalHandler)&ADD_JOBS_signalHandler,
								 "ADD_JOBS_signalHandler", NULL, WRITE );

	daemonCore->Register_Signal( GRIDMAN_REMOVE_JOBS, "RemoveJobs",
								 (SignalHandler)&REMOVE_JOBS_signalHandler,
								 "REMOVE_JOBS_signalHandler", NULL, WRITE );

	Reconfig();
}

void
Reconfig()
{
	// This method is called both at startup [from method Init()], and
	// when we are asked to reconfig.

	contactScheddDelay = param_integer("GRIDMANAGER_CONTACT_SCHEDD_DELAY", 5);

	GahpReconfig();

	JobType *job_type;
	jobTypes.Rewind();
	while ( jobTypes.Next( job_type ) ) {
		job_type->ReconfigFunc();
	}

	// Tell all the job objects to deal with their new config values
	BaseJob *next_job;

	JobsByProcID.startIterations();

	while ( JobsByProcID.iterate( next_job ) != 0 ) {
		next_job->Reconfig();
	}
}

int
ADD_JOBS_signalHandler( int signal )
{
	dprintf(D_FULLDEBUG,"Received ADD_JOBS signal\n");

	if ( !addJobsSignaled ) {
		RequestContactSchedd();
		addJobsSignaled = true;
	}

	return TRUE;
}

int
REMOVE_JOBS_signalHandler( int signal )
{
	dprintf(D_FULLDEBUG,"Received REMOVE_JOBS signal\n");

	if ( !removeJobsSignaled ) {
		RequestContactSchedd();
		removeJobsSignaled = true;
	}

	return TRUE;
}

int
doContactSchedd()
{
	int rc;
	Qmgr_connection *schedd;
	BaseJob *curr_job;
	ClassAd *next_ad;
	char expr_buf[12000];
	bool schedd_updates_complete = false;
	bool schedd_deletes_complete = false;
	bool add_remove_jobs_complete = false;
	bool commit_transaction = true;
//<<<<<<< gridmanager.C
	bool fake_job_in_queue = false;
	int failure_line_num = 0;
//=======
	List<BaseJob> successful_deletes;
//>>>>>>> 1.9.4.33.6.23.2.29

	dprintf(D_FULLDEBUG,"in doContactSchedd()\n");

	contactScheddTid = TIMER_UNSET;

//<<<<<<< gridmanager.C
	// Write user log events before connection to the schedd
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		if ( curr_action->actions & UA_LOG_SUBMIT_EVENT &&
			 !curr_job->submitLogged ) {
			WriteGlobusSubmitEventToUserLog( curr_job->ad );
			curr_job->submitLogged = true;
		}
		if ( curr_action->actions & UA_LOG_EXECUTE_EVENT &&
			 !curr_job->executeLogged ) {
			WriteExecuteEventToUserLog( curr_job->ad );
			curr_job->executeLogged = true;
		}
		if ( curr_action->actions & UA_LOG_SUBMIT_FAILED_EVENT &&
			 !curr_job->submitFailedLogged ) {
			WriteGlobusSubmitFailedEventToUserLog( curr_job->ad,
												   curr_job->submitFailureCode );
			curr_job->submitFailedLogged = true;
		}
		if ( curr_action->actions & UA_LOG_TERMINATE_EVENT &&
			 !curr_job->terminateLogged ) {
			EmailTerminateEvent(curr_job->ad,
				curr_job->IsExitStatusValid());
			WriteTerminateEventToUserLog( curr_job );
			curr_job->terminateLogged = true;
		}
		if ( curr_action->actions & UA_LOG_ABORT_EVENT &&
			 !curr_job->abortLogged ) {
			WriteAbortEventToUserLog( curr_job->ad );
			curr_job->abortLogged = true;
		}
		if ( curr_action->actions & UA_LOG_EVICT_EVENT &&
			 !curr_job->evictLogged ) {
			WriteEvictEventToUserLog( curr_job->ad );
			curr_job->evictLogged = true;
		}
		if ( curr_action->actions & UA_HOLD_JOB &&
			 !curr_job->holdLogged ) {
			WriteHoldEventToUserLog( curr_job->ad );
			curr_job->holdLogged = true;
		}

	}

//=======
//>>>>>>> 1.9.4.33.6.23.2.29
	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		// Should we be retrying infinitely?
		lastContactSchedd = time(NULL);
		RequestContactSchedd();
		return TRUE;
	}

//<<<<<<< gridmanager.C
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		// Check the job status on the schedd to see if the job's been
		// held or removed. We don't want to blindly update the status.
		int job_status_schedd;
		rc = GetAttributeInt( curr_job->procID.cluster,
							  curr_job->procID.proc,
							  ATTR_JOB_STATUS, &job_status_schedd );
		if ( rc < 0 ) {
			if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			} else {
					// The job is not in the schedd's job queue. This
					// probably means that the user did a condor_rm -f,
					// so report a job status of REMOVED and pretend that
					// all updates for the job succeed. Otherwise, we'll
					// never make forward progress on the job.
				job_status_schedd = REMOVED;
				fake_job_in_queue = true;
			}
		}

		// If the job is marked as REMOVED or HELD on the schedd, don't
		// change it. Instead, modify our state to match it.
		// exception: if job is marked as REMOVED, allow us to place it on hold.
		if ( (job_status_schedd == REMOVED && (!(curr_action->actions & UA_HOLD_JOB)))
			 || job_status_schedd == HELD ) {
			curr_job->UpdateCondorState( job_status_schedd );
			curr_job->ad->SetDirtyFlag( ATTR_JOB_STATUS, false );
			curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON, false );
			curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON_CODE, false );
			curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON_SUBCODE, false );
		} else if ( curr_action->actions & UA_HOLD_JOB ) {
			char *reason = NULL;
			rc = GetAttributeStringNew( curr_job->procID.cluster,
										curr_job->procID.proc,
										ATTR_RELEASE_REASON, &reason );
			if ( rc >= 0 ) {
				curr_job->UpdateJobAdString( ATTR_LAST_RELEASE_REASON,
											 reason );
			} else if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			if ( reason ) {
				free( reason );
			}
			curr_job->UpdateJobAd( ATTR_RELEASE_REASON, "UNDEFINED" );
			curr_job->UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS,
									  (int)time(0) );
				
				// if the job was in REMOVED state, make certain we return
				// to the removed state when it is released.
			if ( job_status_schedd == REMOVED ) {
				curr_job->UpdateJobAdInt(ATTR_JOB_STATUS_ON_RELEASE,
					job_status_schedd);
			}

			int sys_holds = 0;
			rc=GetAttributeInt(curr_job->procID.cluster, 
							   curr_job->procID.proc, ATTR_NUM_SYSTEM_HOLDS,
							   &sys_holds);
			if ( rc < 0 && fake_job_in_queue == false ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			sys_holds++;
			curr_job->UpdateJobAdInt( ATTR_NUM_SYSTEM_HOLDS, sys_holds );
		} else {	// !UA_HOLD_JOB
			// If we have a
			// job marked as HELD, it's because of an earlier hold
			// (either by us or the user). In this case, we don't want
			// to undo a subsequent unhold done on the schedd. Instead,
			// we keep our HELD state, kill the job, forget about it,
			// then relearn about it later (this makes it easier to
			// ensure that we pick up changed job attributes).
			if ( curr_job->condorState == HELD ) {
				curr_job->ad->SetDirtyFlag( ATTR_JOB_STATUS, false );
				curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON, false );
				curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON_CODE, false );
				curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON_SUBCODE, false );
			} else {
					// Finally, if we are just changing from one unintersting state
					// to another, update the ATTR_ENTERED_CURRENT_STATUS time.
				if ( curr_job->condorState != job_status_schedd ) {
					curr_job->UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS,
											  (int)time(0) );
				}
			}
		}

		// Adjust run time for condor_q
		int shadowBirthdate = 0;
		curr_job->ad->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadowBirthdate );
		if ( curr_job->condorState == RUNNING &&
			 shadowBirthdate == 0 ) {

			// The job has started a new interval of running
			int current_time = (int)time(NULL);
			// ATTR_SHADOW_BIRTHDATE on the schedd will be updated below
			curr_job->UpdateJobAdInt( ATTR_SHADOW_BIRTHDATE, current_time );

		} else if ( curr_job->condorState != RUNNING &&
					shadowBirthdate != 0 ) {

			// The job has stopped an interval of running, add the current
			// interval to the accumulated total run time
			float accum_time = 0;
			rc = GetAttributeFloat(curr_job->procID.cluster,
								   curr_job->procID.proc,
								   ATTR_JOB_REMOTE_WALL_CLOCK,&accum_time);
			if ( fake_job_in_queue == true ) {
				accum_time = 0.0;
			} else if ( rc < 0 ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			accum_time += (float)( time(NULL) - shadowBirthdate );
			curr_job->UpdateJobAdFloat( ATTR_JOB_REMOTE_WALL_CLOCK,
										accum_time );
			curr_job->UpdateJobAd( ATTR_JOB_WALL_CLOCK_CKPT, "UNDEFINED" );
			// ATTR_SHADOW_BIRTHDATE on the schedd will be updated below
			curr_job->UpdateJobAdInt( ATTR_SHADOW_BIRTHDATE, 0 );

		}

		if ( curr_action->actions & UA_FORGET_JOB ) {
			curr_job->UpdateJobAdBool( ATTR_JOB_MANAGED, 0 );
		}

		dprintf( D_FULLDEBUG, "Updating classad values for %d.%d:\n",
				 curr_job->procID.cluster, curr_job->procID.proc );
		char attr_name[1024];
		char attr_value[1024];
		ExprTree *expr;
		curr_job->ad->ResetExpr();
		while ( (expr = curr_job->ad->NextDirtyExpr()) != NULL ) {
			attr_name[0] = '\0';
			attr_value[0] = '\0';
			expr->LArg()->PrintToStr(attr_name);
			expr->RArg()->PrintToStr(attr_value);

			dprintf( D_FULLDEBUG, "   %s = %s\n", attr_name, attr_value );
			rc = SetAttribute( curr_job->procID.cluster,
							   curr_job->procID.proc,
							   attr_name,
							   attr_value);
			if ( rc < 0 && fake_job_in_queue == false ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
		}

	}

	if ( CloseConnection() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	schedd_updates_complete = true;

	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		if ( curr_action->actions & UA_DELETE_FROM_SCHEDD ) {
			dprintf( D_FULLDEBUG, "Deleting job %d.%d from schedd\n",
					 curr_action->job->procID.cluster,
					 curr_action->job->procID.proc);
			BeginTransaction();
			if ( errno == ETIMEDOUT ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			rc = DestroyProc(curr_action->job->procID.cluster,
							 curr_action->job->procID.proc);
			if ( rc < 0 && fake_job_in_queue == false ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			if ( CloseConnection() < 0 ) {
				failure_line_num = __LINE__;
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			curr_action->deleted = true;
		}

	}

	schedd_deletes_complete = true;

//	if ( BeginTransaction() < 0 ) {
	errno = 0;
	BeginTransaction();
	if ( errno == ETIMEDOUT ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

//=======
//>>>>>>> 1.9.4.33.6.23.2.29

	// AddJobs
	/////////////////////////////////////////////////////
	if ( addJobsSignaled || firstScheddContact ) {
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for new jobs\n" );

		// Make sure we grab all Globus Universe jobs (except held ones
		// that we previously indicated we were done with)
		// when we first start up in case we're recovering from a
		// shutdown/meltdown.
		// Otherwise, grab all jobs that are unheld and aren't marked as
		// currently being managed and aren't marked as not matched.
		// If JobManaged is undefined, equate it with false.
		// If Matched is undefined, equate it with true.
		// NOTE: Schedds from Condor 6.6 and earlier don't include
		//   "(Universe==9)" in the constraint they give to the gridmanager,
		//   so this gridmanager will pull down non-globus-universe ads,
		//   although it won't use them. This is inefficient but not
		//   incorrect behavior.
		if ( firstScheddContact ) {
//<<<<<<< gridmanager.C
			sprintf( expr_buf, 
				"(%s) && %s == %d && (%s =!= FALSE || %s =?= TRUE) && ((%s == %d || %s == %d || (%s == %d && %s == \"%s\")) && %s =!= TRUE) == FALSE",
					 ScheddJobConstraint, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS, 
					 ATTR_JOB_MATCHED, ATTR_JOB_MANAGED, ATTR_JOB_STATUS, HELD,
					 ATTR_JOB_STATUS, COMPLETED, ATTR_JOB_STATUS, REMOVED, ATTR_GLOBUS_CONTACT_STRING, NULL_JOB_CONTACT, ATTR_JOB_MANAGED );
//=======
			// Grab all jobs for us to manage. This expression is a
			// derivative of the expression below for new jobs. We add
			// "|| Managed =?= TRUE" to also get jobs our previous
			// incarnation was in the middle of managing when it died
			// (if it died unexpectedly). With the new term, the
			// "&& Managed =!= TRUE" from the new jobs expression becomes
			// superfluous (by boolean logic), so we drop it.
			sprintf( expr_buf,
					 "(%s) && ((%s =!= FALSE && %s != %d) || %s =?= TRUE)",
					 ScheddJobConstraint, ATTR_JOB_MATCHED,
					 ATTR_JOB_STATUS, HELD, ATTR_JOB_MANAGED );
//>>>>>>> 1.9.4.33.6.23.2.29
		} else {
//<<<<<<< gridmanager.C
			sprintf( expr_buf, 
				"(%s) && %s == %d && %s =!= FALSE && %s != %d && %s != %d && (%s != %d || %s != \"%s\") && %s =!= TRUE",
					 ScheddJobConstraint, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS,
					 ATTR_JOB_MATCHED, ATTR_JOB_STATUS, HELD, 
					 ATTR_JOB_STATUS, COMPLETED, ATTR_JOB_STATUS, REMOVED, ATTR_GLOBUS_CONTACT_STRING, NULL_JOB_CONTACT, ATTR_JOB_MANAGED );
//=======
			// Grab new jobs for us to manage
			sprintf( expr_buf,
					 "(%s) && %s =!= FALSE && %s =!= TRUE && %s != %d",
					 ScheddJobConstraint, ATTR_JOB_MATCHED, ATTR_JOB_MANAGED,
					 ATTR_JOB_STATUS, HELD );
//>>>>>>> 1.9.4.33.6.23.2.29
		}
		dprintf( D_FULLDEBUG,"Using constraint %s\n",expr_buf);
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			BaseJob *old_job;
			int job_is_managed = 0;		// default to false if not in ClassAd
			int job_is_matched = 1;		// default to true if not in ClassAd

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
			next_ad->LookupBool(ATTR_JOB_MANAGED,job_is_managed);
			next_ad->LookupBool(ATTR_JOB_MATCHED,job_is_matched);

			if ( JobsByProcID.lookup( procID, old_job ) != 0 ) {

				int rc;
				JobType *job_type = NULL;
				BaseJob *new_job = NULL;

				// job had better be either managed or matched! (or both)
				ASSERT( job_is_managed || job_is_matched );

				// Search our job types for one that'll handle this job
				jobTypes.Rewind();
				while ( jobTypes.Next( job_type ) ) {
dprintf(D_FULLDEBUG,"***Trying job type %s\n",job_type->Name);
					if ( JobMatchesConstraint( next_ad, job_type->AdMatchConst ) ) {

						// Found one!
						dprintf( D_FULLDEBUG, "Using job type %s for job %d.%d\n",
								 job_type->Name, procID.cluster, procID.proc );
						break;
					}
				}

//<<<<<<< gridmanager.C
				if (must_expand) {
					// Get the expanded ClassAd from the schedd, which
					// has the globus resource filled in with info from
					// the matched ad.
					delete next_ad;
					next_ad = NULL;
					next_ad = GetJobAd(procID.cluster,procID.proc);
					if ( next_ad == NULL && errno == ETIMEDOUT ) {
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
//=======
				if ( job_type != NULL ) {
					if ( job_type->AdMustExpandFunc( next_ad ) ) {
						// Get the expanded ClassAd from the schedd, which
						// has the globus resource filled in with info from
						// the matched ad.
						delete next_ad;
						next_ad = NULL;
						next_ad = GetJobAd(procID.cluster,procID.proc);
						if ( next_ad == NULL && errno == ETIMEDOUT ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
							commit_transaction = false;
							goto contact_schedd_disconnect;
						}
						ASSERT(next_ad);
//>>>>>>> 1.9.4.33.6.23.2.29
					}
					new_job = job_type->CreateFunc( next_ad );
				} else {
					dprintf( D_ALWAYS, "No handlers for job %d.%d",
							 procID.cluster, procID.proc );
					new_job = new BaseJob( next_ad );
				}

				ASSERT(new_job);
				new_job->SetEvaluateState();
				dprintf(D_ALWAYS,"Found job %d.%d --- inserting\n",new_job->procID.cluster,new_job->procID.proc);
				JobsByProcID.insert( new_job->procID, new_job );
				num_ads++;

				if ( !job_is_managed ) {
					rc = SetAttribute( new_job->procID.cluster,
									   new_job->procID.proc,
									   ATTR_JOB_MANAGED,
									   "TRUE" );
					if ( rc < 0 ) {
//<<<<<<< gridmanager.C
						delete next_ad;
						failure_line_num = __LINE__;
//=======
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
//>>>>>>> 1.9.4.33.6.23.2.29
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
//<<<<<<< gridmanager.C
					if ( next_ad->LookupString(ATTR_RELEASE_REASON, &reason)
						     != 0 ) {
						rc = SetAttributeString( procID.cluster, procID.proc,
												 ATTR_LAST_RELEASE_REASON,
												 reason );
						free( reason );
						if ( rc < 0 ) {
							delete next_ad;
							failure_line_num = __LINE__;
							commit_transaction = false;
							goto contact_schedd_disconnect;
						}
					}
					rc = SetAttribute( procID.cluster, procID.proc,
									   ATTR_RELEASE_REASON, "UNDEFINED" );
					if ( rc < 0 ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					rc = SetAttributeString( procID.cluster,
											 procID.proc,
											 ATTR_HOLD_REASON,
											 hold_reason );
					if ( rc < 0 ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
 						goto contact_schedd_disconnect;
					}
					sprintf( buffer, "%s = \"%s\"", ATTR_HOLD_REASON,
							 hold_reason );
					next_ad->InsertOrUpdate( buffer );
					WriteHoldEventToUserLog( next_ad );

					delete next_ad;

				} else {

					const char *canonical_name = GlobusResource::CanonicalName( resource_name );
					ASSERT(canonical_name);
					rc = ResourcesByName.lookup( HashKey( canonical_name ),
												  resource );

					if ( rc != 0 ) {
						resource = new GlobusResource( canonical_name );
						ASSERT(resource);
						ResourcesByName.insert( HashKey( canonical_name ),
												 resource );
					} else {
						ASSERT(resource);
					}

					GlobusJob *new_job = new GlobusJob( next_ad, resource );
					ASSERT(new_job);
					new_job->SetEvaluateState();
					dprintf(D_ALWAYS,"Found job %d.%d --- inserting\n",new_job->procID.cluster,new_job->procID.proc);
					JobsByProcID.insert( new_job->procID, new_job );
					num_ads++;

					if ( !job_is_managed ) {
						// Set Managed to true in the local ad and leave it
						// dirty so that if our update here to the schedd is
						// aborted, the change will make it the first time
						// the job tries to update anything on the schedd.
						new_job->UpdateJobAdBool( ATTR_JOB_MANAGED, 1 );
						rc = SetAttribute( new_job->procID.cluster,
										   new_job->procID.proc,
										   ATTR_JOB_MANAGED,
										   "TRUE" );
						if ( rc < 0 ) {
							failure_line_num = __LINE__;
							commit_transaction = false;
							goto contact_schedd_disconnect;
						}
					}

				}

//=======
				}

//>>>>>>> 1.9.4.33.6.23.2.29
			} else {

				// We already know about this job, skip
				// But also set Managed=true on the schedd so that it won't
				// keep signalling us about it
				delete next_ad;
				rc = SetAttribute( procID.cluster, procID.proc,
								   ATTR_JOB_MANAGED, "TRUE" );
				if ( rc < 0 ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
					commit_transaction = false;
					goto contact_schedd_disconnect;
				}

			}

			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}	// end of while next_ad
		if ( errno == ETIMEDOUT ) {
			failure_line_num = __LINE__;
			commit_transaction = false;
			goto contact_schedd_disconnect;
		}

		dprintf(D_FULLDEBUG,"Fetched %d new job ads from schedd\n",num_ads);
	}	// end of handling add jobs


	// RemoveJobs
	/////////////////////////////////////////////////////
	if ( removeJobsSignaled ) {
		int num_ads = 0;

		dprintf( D_FULLDEBUG, "querying for removed/held jobs\n" );

		// Grab jobs marked as REMOVED or marked as HELD that we haven't
		// previously indicated that we're done with (by setting JobManaged
		// to FALSE. If JobManaged is undefined, equate it with false.
		sprintf( expr_buf, "(%s) && (%s == %d || (%s == %d && %s =?= TRUE))",
				 ScheddJobConstraint, ATTR_JOB_STATUS, REMOVED,
				 ATTR_JOB_STATUS, HELD, ATTR_JOB_MANAGED );

		dprintf( D_FULLDEBUG,"Using constraint %s\n",expr_buf);
		next_ad = GetNextJobByConstraint( expr_buf, 1 );
		while ( next_ad != NULL ) {
			PROC_ID procID;
			BaseJob *next_job;
			int curr_status;

			next_ad->LookupInteger( ATTR_CLUSTER_ID, procID.cluster );
			next_ad->LookupInteger( ATTR_PROC_ID, procID.proc );
			next_ad->LookupInteger( ATTR_JOB_STATUS, curr_status );

			if ( JobsByProcID.lookup( procID, next_job ) == 0 ) {
				// Should probably skip jobs we already have marked as
				// held or removed

//<<<<<<< gridmanager.C
				// Save the remove reason in our local copy of the job ad
				// so that we can write it in the abort log event.
				if ( curr_status == REMOVED ) {
					int rc;
					char *remove_reason = NULL;
					rc = GetAttributeStringNew( procID.cluster,
												procID.proc,
												ATTR_REMOVE_REASON,
												&remove_reason );
					if ( rc == 0 ) {
						next_job->UpdateJobAdString( ATTR_REMOVE_REASON,
													 remove_reason );
					} else if ( rc < 0 && errno == ETIMEDOUT ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					if ( remove_reason ) {
						free( remove_reason );
					}
				}

				// Save the hold reason in our local copy of the job ad
				// so that we can write it in the hold log event.
				if ( curr_status == HELD ) {
					int rc;
					char *hold_reason = NULL;
					int hcode = 0;
					int hsubcode = 0;
					rc = GetAttributeStringNew( procID.cluster,
												procID.proc,
												ATTR_HOLD_REASON,
												&hold_reason );
					rc += GetAttributeInt( procID.cluster,
												procID.proc,
												ATTR_HOLD_REASON_CODE,
												&hcode );
					rc += GetAttributeInt( procID.cluster,
												procID.proc,
												ATTR_HOLD_REASON_SUBCODE,
												&hsubcode );
					if ( rc == 0 ) {
						next_job->UpdateJobAdString( ATTR_HOLD_REASON,
													 hold_reason );
						next_job->UpdateJobAdInt( ATTR_HOLD_REASON_CODE,
													 hcode );
						next_job->UpdateJobAdInt( ATTR_HOLD_REASON_SUBCODE,
													 hsubcode );
					} else if ( rc < 0 && errno == ETIMEDOUT ) {
						delete next_ad;
						failure_line_num = __LINE__;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					if ( hold_reason ) {
						free( hold_reason );
					}
				}

				// Don't update the condor state if a communications error
				// kept us from getting a remove reason to go with it.
				next_job->UpdateCondorState( curr_status );
//=======
				next_job->JobAdUpdateFromSchedd( next_ad );
//>>>>>>> 1.9.4.33.6.23.2.29
				num_ads++;

			} else if ( curr_status == REMOVED ) {

				// If we don't know about the job, remove it immediately
				// I don't think this can happen in the normal case,
				// but I'm not sure.
				dprintf( D_ALWAYS, 
						 "Don't know about removed job %d.%d. "
						 "Deleting it immediately\n", procID.cluster,
						 procID.proc );
				// Log the removal of the job from the queue
				WriteAbortEventToUserLog( next_ad );
				rc = DestroyProc( procID.cluster, procID.proc );
				if ( rc < 0 ) {
					delete next_ad;
					failure_line_num = __LINE__;
					commit_transaction = false;
					goto contact_schedd_disconnect;
				}

			} else {

				dprintf( D_ALWAYS, "Don't know about held job %d.%d. "
						 "Ignoring it\n",
						 procID.cluster, procID.proc );

			}

			delete next_ad;
			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}
		if ( errno == ETIMEDOUT ) {
			commit_transaction = false;
			failure_line_num = __LINE__;
			goto contact_schedd_disconnect;
		}

		dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);
	}

	if ( CloseConnection() < 0 ) {
		failure_line_num = __LINE__;
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	add_remove_jobs_complete = true;

//	if ( BeginTransaction() < 0 ) {
	errno = 0;
	BeginTransaction();
	if ( errno == ETIMEDOUT ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

//<<<<<<< gridmanager.C
	if ( schedd_updates_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error during updates at line %d! Will retry\n", failure_line_num );
		lastContactSchedd = time(NULL);
		RequestContactSchedd();
		return TRUE;
	}
//=======
//>>>>>>> 1.9.4.33.6.23.2.29

	// Update existing jobs
	/////////////////////////////////////////////////////
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_job ) != 0 ) {

dprintf(D_FULLDEBUG,"Updating classad values for %d.%d:\n",curr_job->procID.cluster, curr_job->procID.proc);
		char attr_name[1024];
		char attr_value[1024];
		ExprTree *expr;
		curr_job->ad->ResetExpr();
		while ( (expr = curr_job->ad->NextDirtyExpr()) != NULL ) {
			attr_name[0] = '\0';
			attr_value[0] = '\0';
			expr->LArg()->PrintToStr(attr_name);
			expr->RArg()->PrintToStr(attr_value);

//<<<<<<< gridmanager.C
		if ( schedd_updates_complete ) {
			curr_job->ad->ClearAllDirtyFlags();
		}

		if ( curr_action->actions & UA_FORGET_JOB ) {
//=======
dprintf(D_FULLDEBUG,"   %s = %s\n",attr_name,attr_value);
			rc = SetAttribute( curr_job->procID.cluster,
							   curr_job->procID.proc,
							   attr_name,
							   attr_value);
			if ( rc < 0 ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
		}
//>>>>>>> 1.9.4.33.6.23.2.29

	}

	if ( CloseConnection() < 0 ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
		commit_transaction = false;
		goto contact_schedd_disconnect;
	}

	schedd_updates_complete = true;


	// Delete existing jobs
	/////////////////////////////////////////////////////
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_job ) != 0 ) {

		if ( curr_job->deleteFromSchedd ) {
dprintf(D_FULLDEBUG,"Deleting job %d.%d from schedd\n",curr_job->procID.cluster, curr_job->procID.proc);
			BeginTransaction();
			if ( errno == ETIMEDOUT ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			rc = DestroyProc(curr_job->procID.cluster,
							 curr_job->procID.proc);
			if ( rc < 0 ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			if ( CloseConnection() < 0 ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			successful_deletes.Append( curr_job );
		}

	}

	schedd_deletes_complete = true;


 contact_schedd_disconnect:
	DisconnectQ( schedd, commit_transaction );

	if ( add_remove_jobs_complete == true ) {
		firstScheddContact = false;
		addJobsSignaled = false;
		removeJobsSignaled = false;
//<<<<<<< gridmanager.C
	}

	// Check if we have any jobs left to manage. If not, exit.
	if ( JobsByProcID.getNumElements() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

	lastContactSchedd = time(NULL);

	if ( add_remove_jobs_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error at line %d! Will retry\n", failure_line_num );
//=======
	} else {
		dprintf( D_ALWAYS, "Schedd connection error during Add/RemoveJobs! Will retry\n" );
//>>>>>>> 1.9.4.33.6.23.2.29
		RequestContactSchedd();
		return TRUE;
	}

	if ( schedd_updates_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error during updates! Will retry\n" );
		lastContactSchedd = time(NULL);
		RequestContactSchedd();
		return TRUE;
	}

	// Wake up jobs that had schedd updates pending and delete job
	// objects that wanted to be deleted
	pendingScheddUpdates.startIterations();

	while ( pendingScheddUpdates.iterate( curr_job ) != 0 ) {

		curr_job->ad->ClearAllDirtyFlags();

		if ( curr_job->deleteFromGridmanager ) {

				// If the Job object wants to delete the job from the
				// schedd but we failed to do so, don't delete the job
				// object yet; wait until we successfully delete the job
				// from the schedd.
			if ( curr_job->deleteFromSchedd == true &&
				 successful_deletes.Delete( curr_job ) == false ) {
				continue;
			}

			JobsByProcID.remove( curr_job->procID );
				// If wantRematch is set, send a reschedule now
			if ( curr_job->wantRematch ) {
				static DCSchedd* schedd_obj = NULL;
				if ( !schedd_obj ) {
					schedd_obj = new DCSchedd(NULL,NULL);
					ASSERT(schedd_obj);
				}
				schedd_obj->reschedule();
			}
			pendingScheddUpdates.remove( curr_job->procID );
			delete curr_job;

		} else {
			pendingScheddUpdates.remove( curr_job->procID );

//<<<<<<< gridmanager.C
// TODO: This appears three times in the Condor source.  Unify?
//   (It only is made visible in condor_shadow.jim's prototypes.h.)
static char *
d_format_time( double dsecs )
{
	int days, hours, minutes, secs;
	static char answer[25];

	const int SECONDS = 1;
	const int MINUTES = (60 * SECONDS);
	const int HOURS   = (60 * MINUTES);
	const int DAYS    = (24 * HOURS);

	secs = (int)dsecs;

	days = secs / DAYS;
	secs %= DAYS;

	hours = secs / HOURS;
	secs %= HOURS;

	minutes = secs / MINUTES;
	secs %= MINUTES;

	(void)sprintf(answer, "%3d %02d:%02d:%02d", days, hours, minutes, secs);

	return( answer );
}

static
void
EmailTerminateEvent(ClassAd * jobAd, bool exit_status_valid)
{
	if ( !jobAd ) {
		dprintf(D_ALWAYS, 
			"email_terminate_event called with invalid ClassAd\n");
		return;
	}

	int cluster, proc;
	jobAd->LookupInteger( ATTR_CLUSTER_ID, cluster );
	jobAd->LookupInteger( ATTR_PROC_ID, proc );

	int notification = NOTIFY_COMPLETE; // default
	jobAd->LookupInteger(ATTR_JOB_NOTIFICATION,notification);

	switch( notification ) {
		case NOTIFY_NEVER:    return;
		case NOTIFY_ALWAYS:   break;
		case NOTIFY_COMPLETE: break;
		case NOTIFY_ERROR:    return;
		default:
			dprintf(D_ALWAYS, 
				"Condor Job %d.%d has unrecognized notification of %d\n",
				cluster, proc, notification );
				// When in doubt, better send it anyway...
			break;
	}

	char subjectline[50];
	sprintf( subjectline, "Condor Job %d.%d", cluster, proc );
	FILE * mailer =  email_user_open( jobAd, subjectline );

	if( ! mailer ) {
		// Is message redundant?  Check email_user_open and euo's children.
		dprintf(D_ALWAYS, 
			"email_terminate_event failed to open a pipe to a mail program.\n");
		return;
	}

		// gather all the info out of the job ad which we want to 
		// put into the email message.
	char JobName[_POSIX_PATH_MAX];
	JobName[0] = '\0';
	jobAd->LookupString( ATTR_JOB_CMD, JobName );

	char Args[_POSIX_ARG_MAX];
	Args[0] = '\0';
	jobAd->LookupString(ATTR_JOB_ARGUMENTS, Args);
	
	/*
	// Not present.  Probably doesn't make sense for Globus
	int had_core = FALSE;
	jobAd->LookupBool( ATTR_JOB_CORE_DUMPED, had_core );
	*/

	int q_date = 0;
	jobAd->LookupInteger(ATTR_Q_DATE,q_date);
	
	float remote_sys_cpu = 0.0;
	jobAd->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, remote_sys_cpu);
	
	float remote_user_cpu = 0.0;
	jobAd->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, remote_user_cpu);
	
	int image_size = 0;
	jobAd->LookupInteger(ATTR_IMAGE_SIZE, image_size);
	
	/*
	int shadow_bday = 0;
	jobAd->LookupInteger( ATTR_SHADOW_BIRTHDATE, shadow_bday );
	*/
	
	float previous_runs = 0;
	jobAd->LookupFloat( ATTR_JOB_REMOTE_WALL_CLOCK, previous_runs );
	
	time_t arch_time=0;	/* time_t is 8 bytes some archs and 4 bytes on other
						   archs, and this means that doing a (time_t*)
						   cast on & of a 4 byte int makes my life hell.
						   So we fix it by assigning the time we want to
						   a real time_t variable, then using ctime()
						   to convert it to a string */
	
	time_t now = time(NULL);

	fprintf( mailer, "Your Condor job %d.%d \n", cluster, proc);
	if ( JobName[0] ) {
		fprintf(mailer,"\t%s %s\n",JobName,Args);
	}
	if(exit_status_valid) {
		fprintf(mailer, "has ");

		int int_val;
		if( jobAd->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_val) ) {
			if( int_val ) {
				if( jobAd->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_val) ) {
					fprintf(mailer, "exited with the signal %d.\n", int_val);
				} else {
					fprintf(mailer, "exited with an unknown signal.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
				}
			} else {
				if( jobAd->LookupInteger(ATTR_ON_EXIT_CODE, int_val) ) {
					fprintf(mailer, "exited normally with status %d.\n",
						int_val);
				} else {
					fprintf(mailer, "exited normally with unknown status.\n");
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Return code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_CODE);
				}
			}
		} else {
			fprintf(mailer,"has exited.\n");
			dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  ",
				 cluster, proc, ATTR_ON_EXIT_BY_SIGNAL);
		}
	} else {
		fprintf(mailer,"has exited.\n");
	}

	/*
	if( had_core ) {
		fprintf( mailer, "Core file is: %s\n", getCoreName() );
	}
	*/

	arch_time = q_date;
	fprintf(mailer, "\n\nSubmitted at:        %s", ctime(&arch_time));
	
	double real_time = now - q_date;
	arch_time = now;
	fprintf(mailer, "Completed at:        %s", ctime(&arch_time));
	
	fprintf(mailer, "Real Time:           %s\n", 
			d_format_time(real_time));


	fprintf( mailer, "\n" );
	
	if( exit_status_valid ) {
		fprintf(mailer, "Virtual Image Size:  %d Kilobytes\n\n", image_size);
	}
	
	double rutime = remote_user_cpu;
	double rstime = remote_sys_cpu;
	double trtime = rutime + rstime;
	/*
	double wall_time = now - shadow_bday;
	fprintf(mailer, "Statistics from last run:\n");
	fprintf(mailer, "Allocation/Run time:     %s\n",d_format_time(wall_time) );
	*/
	if( exit_status_valid ) {
		fprintf(mailer, "Remote User CPU Time:    %s\n", d_format_time(rutime) );
		fprintf(mailer, "Remote System CPU Time:  %s\n", d_format_time(rstime) );
		fprintf(mailer, "Total Remote CPU Time:   %s\n\n", d_format_time(trtime));
	}
	
	/*
	double total_wall_time = previous_runs + wall_time;
	fprintf(mailer, "Statistics totaled from all runs:\n");
	fprintf(mailer, "Allocation/Run time:     %s\n",
			d_format_time(total_wall_time) );

	// TODO: Can we/should we get this for Globus jobs.
		// TODO: deal w/ total bytes <- obsolete? in original code)
	float network_bytes;
	network_bytes = bytesSent();
	fprintf(mailer, "\nNetwork:\n" );
	fprintf(mailer, "%10s Run Bytes Received By Job\n", 
			metric_units(network_bytes) );
	network_bytes = bytesReceived();
	fprintf(mailer, "%10s Run Bytes Sent By Job\n",
			metric_units(network_bytes) );
	*/

	email_close(mailer);
}

// Initialize a UserLog object for a given job and return a pointer to
// the UserLog object created.  This object can then be used to write
// events and must be deleted when you're done.  This returns NULL if
// the user didn't want a UserLog, so you must check for NULL before
// using the pointer you get back.
UserLog*
InitializeUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char userLogFile[_POSIX_PATH_MAX];
	char domain[_POSIX_PATH_MAX];
//=======
			curr_job->SetEvaluateState();
		}
//>>>>>>> 1.9.4.33.6.23.2.29

	}

	// Check if we have any jobs left to manage. If not, exit.
	if ( JobsByProcID.getNumElements() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

	lastContactSchedd = time(NULL);

	if ( schedd_deletes_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error! Will retry\n" );
		RequestContactSchedd();
	}

dprintf(D_FULLDEBUG,"leaving doContactSchedd()\n");
	return TRUE;
}

//<<<<<<< gridmanager.C
bool
WriteAbortEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	char removeReason[256];
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing abort record to user logfile\n",
			 cluster, proc );

	JobAbortedEvent event;

	removeReason[0] = '\0';
	job_ad->LookupString( ATTR_REMOVE_REASON, removeReason,
						   sizeof(removeReason) - 1 );

	event.setReason( removeReason );

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_ABORT event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

// TODO: We could do this with the jobad (as it was called before)
// and just probe for the ATTR_USE_GRID_SHELL...?
bool
WriteTerminateEventToUserLog( GlobusJob *curr_job ) 
{
	if( ! curr_job) {
		dprintf( D_ALWAYS, 
			"Internal Error: WriteTerminateEventToUserLog passed invalid "
			"GlobusJob (null curr_job).\n");
		return false;
	}
	ClassAd *job_ad = curr_job->ad;
	if( ! job_ad) {
		dprintf( D_ALWAYS, 
			"Internal Error: WriteTerminateEventToUserLog passed invalid "
			"GlobusJob (null ad).\n");
		return false;
	}

	int cluster, proc;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing terminate record to user logfile\n",
			 cluster, proc );

	JobTerminatedEvent event;
	struct rusage r;
	memset( &r, 0, sizeof( struct rusage ) );

#if !defined(WIN32)
	event.run_local_rusage = r;
	event.run_remote_rusage = r;
	event.total_local_rusage = r;
	event.total_remote_rusage = r;
#endif /* WIN32 */
	event.sent_bytes = 0;
	event.recvd_bytes = 0;
	event.total_sent_bytes = 0;
	event.total_recvd_bytes = 0;
	event.normal = true;

	// Globus doesn't tell us how the job exited, so we'll just assume it
	// exited normally.
	event.returnValue = 0;
	event.normal = true;

	if( curr_job->IsExitStatusValid() ) {
		int int_val;
		if( job_ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_val) ) {
			if( int_val ) {
				event.normal = false;
				if( job_ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_val) ) {
					event.signalNumber = int_val;
					event.normal = false;
				} else {
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Signal code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_SIGNAL);
					event.normal = false;
				}
			} else {
				if( job_ad->LookupInteger(ATTR_ON_EXIT_CODE, int_val) ) {
					event.normal = true;
					event.returnValue = int_val;
				} else {
					event.normal = false;
					dprintf( D_ALWAYS, "(%d.%d) Job ad lacks %s.  "
						 "Return code unknown.\n", cluster, proc, 
						 ATTR_ON_EXIT_CODE);
					event.normal = false;
				}
			}
		} else {
			event.normal = false;
			dprintf( D_ALWAYS,
				 "(%d.%d) Job ad lacks %s.  Final state unknown.\n",
				 cluster, proc, ATTR_ON_EXIT_BY_SIGNAL);
			event.normal = false;
		}
	}

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_TERMINATED event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteEvictEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing evict record to user logfile\n",
			 cluster, proc );

	JobEvictedEvent event;
	struct rusage r;
	memset( &r, 0, sizeof( struct rusage ) );

#if !defined(WIN32)
	event.run_local_rusage = r;
	event.run_remote_rusage = r;
#endif /* WIN32 */
	event.sent_bytes = 0;
	event.recvd_bytes = 0;

	event.checkpointed = false;

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_EVICTED event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteHoldEventToUserLog( ClassAd *job_ad )
{
	int cluster, proc;
	
	UserLog *ulog = InitializeUserLog( job_ad );
	if ( ulog == NULL ) {
		// User doesn't want a log
		return true;
	}

	job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	job_ad->LookupInteger( ATTR_PROC_ID, proc );

	dprintf( D_FULLDEBUG, 
			 "(%d.%d) Writing hold record to user logfile\n",
			 cluster, proc );

	JobHeldEvent event;


	event.initFromClassAd(job_ad);

	int rc = ulog->writeEvent(&event);
	delete ulog;

	if (!rc) {
		dprintf( D_ALWAYS,
				 "(%d.%d) Unable to log ULOG_JOB_HELD event\n",
				 cluster, proc );
		return false;
	}

	return true;
}

bool
WriteGlobusSubmitEventToUserLog( ClassAd *job_ad )
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

bool
WriteGlobusSubmitFailedEventToUserLog( ClassAd *job_ad, int failure_code )
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
			GahpMain.globus_gram_client_error_string(failure_code) );
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

bool
WriteGlobusResourceUpEventToUserLog( ClassAd *job_ad )
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

bool
WriteGlobusResourceDownEventToUserLog( ClassAd *job_ad )
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
//=======
//>>>>>>> 1.9.4.33.6.23.2.29
