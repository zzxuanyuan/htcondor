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
#include "condor_classad.h"
#include "condor_qmgr.h"
#include "my_username.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "format_time.h"  // for format_time and friends
#include "daemon.h"
#include "dc_schedd.h"

#include "gridmanager.h"

#define QMGMT_TIMEOUT 15

#define UPDATE_SCHEDD_DELAY		5

#define HASH_TABLE_SIZE			500

// timer id values that indicates the timer is not registered
#define TIMER_UNSET		-1

extern char *myUserName;

struct ScheddUpdateAction {
	BaseJob *job;
	int actions;
	int request_id;
	bool deleted;
};

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
template class HashTable<HashKey, char *>;
template class HashBucket<HashKey, char *>;
template class HashTable<PROC_ID, ScheddUpdateAction *>;
template class HashBucket<PROC_ID, ScheddUpdateAction *>;

HashTable <PROC_ID, ScheddUpdateAction *> pendingScheddUpdates( HASH_TABLE_SIZE,
																procIDHash );
HashTable <PROC_ID, ScheddUpdateAction *> completedScheddUpdates( HASH_TABLE_SIZE,
																  procIDHash );
bool addJobsSignaled = false;
bool removeJobsSignaled = false;
int contactScheddTid = TIMER_UNSET;
int contactScheddDelay;
time_t lastContactSchedd = 0;

char *ScheddAddr = NULL;
char *X509Proxy = NULL;
bool useDefaultProxy = true;
char *ScheddJobConstraint = NULL;
char *GridmanagerScratchDir = NULL;

HashTable <PROC_ID, BaseJob *> JobsByProcID( HASH_TABLE_SIZE,
											 procIDHash );

bool firstScheddContact = true;

char *Owner = NULL;

void RequestContactSchedd();
int doContactSchedd();

// handlers
int ADD_JOBS_signalHandler( int );
int REMOVE_JOBS_signalHandler( int );


// return value of true means requested update has been committed to schedd.
// return value of false means requested update has been queued, but has not
//   been committed to the schedd yet
bool
addScheddUpdateAction( BaseJob *job, int actions, int request_id )
{
	ScheddUpdateAction *curr_action;

	if ( request_id != 0 &&
		 completedScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		ASSERT( curr_action->job == job );

		if ( request_id == curr_action->request_id && request_id != 0 ) {
			completedScheddUpdates.remove( job->procID );
			delete curr_action;
			return true;
		} else {
			completedScheddUpdates.remove( job->procID );
			delete curr_action;
		}
	}

	if ( pendingScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		ASSERT( curr_action->job == job );

		curr_action->actions |= actions;
		curr_action->request_id = request_id;
	} else if ( actions ) {
		curr_action = new ScheddUpdateAction;
		curr_action->job = job;
		curr_action->actions = actions;
		curr_action->request_id = request_id;
		curr_action->deleted = false;

		pendingScheddUpdates.insert( job->procID, curr_action );
		RequestContactSchedd();
	} else {
		// If a new request comes in with no actions and there are no
		// pending actions, just return true (since there's nothing to be
		// committed to the schedd)
		return true;
	}

	return false;

}

void
removeScheddUpdateAction( BaseJob *job ) {
	ScheddUpdateAction *curr_action;

	if ( completedScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		completedScheddUpdates.remove( job->procID );
		delete curr_action;
	}

	if ( pendingScheddUpdates.lookup( job->procID, curr_action ) == 0 ) {
		pendingScheddUpdates.remove( job->procID );
		delete curr_action;
	}
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

	if ( ScheddJobConstraint == NULL ) {
		// CRUFT: Backwards compatibility with pre-6.5.1 schedds that don't
		//   give us a constraint expression for querying our jobs. Build
		//   it ourselves like the old gridmanager did.
		MyString buf;
		MyString expr = "";

		if ( myUserName ) {
			if ( strchr(myUserName,'@') ) {
				// we were given a full name : owner@uid-domain
				buf.sprintf("%s == \"%s\"", ATTR_USER, myUserName);
			} else {
				// we were just give an owner name without a domain
				buf.sprintf("%s == \"%s\"", ATTR_OWNER, myUserName);
			}
		} else {
			buf.sprintf("%s == \"%s\"", ATTR_OWNER, Owner);
		}
		expr += buf;

		if(useDefaultProxy == false) {
			buf.sprintf(" && %s =?= \"%s\" ", ATTR_X509_USER_PROXY, X509Proxy);
		} else {
			buf.sprintf(" && %s =?= UNDEFINED ", ATTR_X509_USER_PROXY);
		}
		expr += buf;

		ScheddJobConstraint = strdup( expr.Value() );

		if ( X509Proxy == NULL ) {
			EXCEPT( "Old schedd didn't specify proxy with -x" );
		}

	} else {

		if ( GridmanagerScratchDir == NULL ) {
			EXCEPT( "Schedd didn't specify scratch dir with -S" );
		}
	}

	GlobusJobInit();
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

	GlobusJobReconfig();
	OracleJobReconfig();

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
	ScheddUpdateAction *curr_action;
	BaseJob *curr_job;
	ClassAd *next_ad;
	char expr_buf[12000];
	bool schedd_updates_complete = false;
	bool schedd_deletes_complete = false;
	bool add_remove_jobs_complete = false;
	bool commit_transaction = true;

	dprintf(D_FULLDEBUG,"in doContactSchedd()\n");

	contactScheddTid = TIMER_UNSET;

	schedd = ConnectQ( ScheddAddr, QMGMT_TIMEOUT, false );
	if ( !schedd ) {
		dprintf( D_ALWAYS, "Failed to connect to schedd!\n");
		// Should we be retrying infinitely?
		lastContactSchedd = time(NULL);
		RequestContactSchedd();
		return TRUE;
	}


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
		if ( firstScheddContact ) {
//			sprintf( expr_buf, "%s && %s == %d && !(%s == %d && %s =!= TRUE)",
			sprintf( expr_buf, 
				"(%s) && %s == %d && (%s =!= FALSE || %s =?= TRUE) && ((%s == %d || %s == %d || %s == %d) && %s =!= TRUE) == FALSE",
					 ScheddJobConstraint, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS, 
					 ATTR_JOB_MATCHED, ATTR_JOB_MANAGED, ATTR_JOB_STATUS, HELD,
					 ATTR_JOB_STATUS, COMPLETED, ATTR_JOB_STATUS, REMOVED, ATTR_JOB_MANAGED );
		} else {
			sprintf( expr_buf, 
				"%s && %s == %d && %s =!= FALSE && %s != %d && %s != %d && %s != %d && %s =!= TRUE",
					 ScheddJobConstraint, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS,
					 ATTR_JOB_MATCHED, ATTR_JOB_STATUS, HELD, 
					 ATTR_JOB_STATUS, COMPLETED, ATTR_JOB_STATUS, REMOVED, ATTR_JOB_MANAGED );
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
				BaseJob *new_job = NULL;

				// job had better be either managed or matched! (or both)
				ASSERT( job_is_managed || job_is_matched );

				if ( OracleJobAdMatch( next_ad ) ) {

					new_job = new OracleJob( next_ad );

				}else if ( GlobusJobAdMatch( next_ad ) ) {

					if ( GlobusJobAdMustExpand( next_ad ) ) {
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
					}

					new_job = new GlobusJob( next_ad );

				} else {
					// TODO: should put job on hold
					EXCEPT("No handlers for job %d.%d",procID.cluster,
						   procID.proc);
				}

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
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
				}

			} else {

				// We already know about this job, skip
				delete next_ad;

			}

			next_ad = GetNextJobByConstraint( expr_buf, 0 );
		}	// end of while next_ad
		if ( errno == ETIMEDOUT ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
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
		sprintf( expr_buf, "(%s) && %s == %d && (%s == %d || (%s == %d && %s =?= TRUE))",
				 ScheddJobConstraint, ATTR_JOB_UNIVERSE, CONDOR_UNIVERSE_GLOBUS,
				 ATTR_JOB_STATUS, REMOVED, ATTR_JOB_STATUS, HELD,
				 ATTR_JOB_MANAGED );

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
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
						delete next_ad;
						commit_transaction = false;
						goto contact_schedd_disconnect;
					}
					if ( remove_reason ) {
						free( remove_reason );
					}
				}

				// Don't update the condor state if a communications error
				// kept us from getting a remove reason to go with it.
				next_job->UpdateCondorState( curr_status );
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
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
					delete next_ad;
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
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
			commit_transaction = false;
			goto contact_schedd_disconnect;
		}

		dprintf(D_FULLDEBUG,"Fetched %d job ads from schedd\n",num_ads);
	}

	if ( CloseConnection() < 0 ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
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


	// Update existing jobs
	/////////////////////////////////////////////////////
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
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
			commit_transaction = false;
			goto contact_schedd_disconnect;
		}

		// If the job is marked as REMOVED or HELD on the schedd, don't
		// change it. Instead, modify our state to match it.
		if ( job_status_schedd == REMOVED || job_status_schedd == HELD ) {
			curr_job->UpdateCondorState( job_status_schedd );
			curr_job->ad->SetDirtyFlag( ATTR_JOB_STATUS, false );
			curr_job->ad->SetDirtyFlag( ATTR_HOLD_REASON, false );
		} else if ( curr_action->actions & UA_HOLD_JOB ) {
			char *reason = NULL;
			rc = GetAttributeStringNew( curr_job->procID.cluster,
										curr_job->procID.proc,
										ATTR_RELEASE_REASON, &reason );
			if ( rc >= 0 ) {
				curr_job->UpdateJobAdString( ATTR_LAST_RELEASE_REASON,
											 reason );
			} else if ( errno == ETIMEDOUT ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			if ( reason ) {
				free( reason );
			}
			curr_job->UpdateJobAd( ATTR_RELEASE_REASON, "UNDEFINED" );
			curr_job->UpdateJobAdInt( ATTR_ENTERED_CURRENT_STATUS,
									  (int)time(0) );
			int sys_holds = 0;
			rc=GetAttributeInt(curr_job->procID.cluster, 
							   curr_job->procID.proc, ATTR_NUM_SYSTEM_HOLDS,
							   &sys_holds);
			if ( rc < 0 ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
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
			if ( rc < 0 ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
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

		curr_job->ad->ClearAllDirtyFlags();

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

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		if ( curr_action->actions & UA_DELETE_FROM_SCHEDD ) {
dprintf(D_FULLDEBUG,"Deleting job %d.%d from schedd\n",curr_action->job->procID.cluster, curr_action->job->procID.proc);
			BeginTransaction();
			if ( errno == ETIMEDOUT ) {
dprintf(D_ALWAYS,"***schedd failure at %d!\n",__LINE__);
				commit_transaction = false;
				goto contact_schedd_disconnect;
			}
			rc = DestroyProc(curr_action->job->procID.cluster,
							 curr_action->job->procID.proc);
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
			curr_action->deleted = true;
		}

	}

	schedd_deletes_complete = true;


 contact_schedd_disconnect:
	DisconnectQ( schedd, commit_transaction );

	if ( add_remove_jobs_complete == true ) {
		firstScheddContact = false;
		addJobsSignaled = false;
		removeJobsSignaled = false;
	} else {
		dprintf( D_ALWAYS, "Schedd connection error during Add/RemoveJobs! Will retry\n" );
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

	while ( pendingScheddUpdates.iterate( curr_action ) != 0 ) {

		curr_job = curr_action->job;

		if ( curr_action->actions & UA_FORGET_JOB ) {

			if ( (curr_action->actions & UA_DELETE_FROM_SCHEDD) ?
				 curr_action->deleted == true :
				 true ) {

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
				delete curr_job;

				delete curr_action;
			}
		} else if ( curr_action->request_id != 0 ) {
			completedScheddUpdates.insert( curr_job->procID, curr_action );
			curr_job->SetEvaluateState();
		} else {
			delete curr_action;
		}

	}

	pendingScheddUpdates.clear();

	// Check if we have any jobs left to manage. If not, exit.
	if ( JobsByProcID.getNumElements() == 0 ) {
		dprintf( D_ALWAYS, "No jobs left, shutting down\n" );
		daemonCore->Send_Signal( daemonCore->getpid(), SIGTERM );
	}

	lastContactSchedd = time(NULL);

	if ( add_remove_jobs_complete == false ) {
		dprintf( D_ALWAYS, "Schedd connection error! Will retry\n" );
		RequestContactSchedd();
	}

dprintf(D_FULLDEBUG,"leaving doContactSchedd()\n");
	return TRUE;
}

