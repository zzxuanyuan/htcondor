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
#include "condor_api.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "daemon.h"
#include "daemon_types.h"
#include "dc_collector.h"
#include "dc_master.h"
#include "dc_startd.h"
#include "admin_event.h"
#include "classad_hashtable.h"
#include "directory.h"

int numStartdStats = 0;
int ClaimRunTimeSort(ClassAd *job1, ClassAd *job2, void *data);

AdminEvent::AdminEvent( void ) : 
	m_JobNodes_su(1024,hashFunction), 
	m_CkptTest_su(256,hashFunction)
{
	m_mystate = EVENT_INIT;
	m_timeridDoShutdown = -1;
	m_intervalDoShutdown = 60;

	m_timerid_DoShutdown_States = 0;
	m_intervalCheck_DoShutdown_States = 5;
	m_intervalPeriod_DoShutdown_States = 0;

 	m_timerid_PollingVacates = -1;
    m_intervalCheck_PollingVacates = 5;
    m_intervalPeriod_PollingVacates = 10;

	m_benchmark_size = 200;
	m_benchmark_lastsize = 0;
	m_benchmark_increment = 200;
	m_benchmark_iteration = 0;

	m_haveShutdown = false;
	m_haveFullStats = false;
	m_haveBenchStats = false;
	m_stillPollingVacates = false;

	m_shutdownTime = 0;
	m_newshutdownTime = 0;
	m_shutdownDelta = 0;

	m_shutdownTarget = "";
	m_newshutdownTarget = "";
	m_shutdownConstraint = "";

	m_shutdownSize = 0;
	m_newshutdownSize = 0;
	m_lastVacateTimes = 0;
	m_VacateTimes = 0;

	m_NrightNow_megspersec = 0;
}

AdminEvent::~AdminEvent( void )
{
}

int
AdminEvent::init( void )
{
	// Read our Configuration parameters
	config( true );
	return 0;
}

int
AdminEvent::shutdownFast( void )
{
	dprintf(D_ALWAYS,"shutdownFast\n");
	char *tmp;

	tmp = param("SPOOL");
	if(tmp) {
		if(spoolClassAd(m_lastShutdown,"out") == 1) {
			dprintf(D_ALWAYS,"Failed to get/create initial classad from spool\n");
		}
		free(tmp);
	}

	return 0;
}

int
AdminEvent::shutdownGraceful( void )
{
	ClassAd *ad;
	char *tmp;

	tmp = param("SPOOL");
	if(tmp) {
		if(spoolClassAd(m_lastShutdown,"out") == 1) {
			dprintf(D_ALWAYS,"Failed to get/create initial classad from spool\n");
		}
		free(tmp);
	}

	dprintf(D_ALWAYS,"shutdownGraceful .. cleaning classad lists\n");

	m_PollingStartdAds.Rewind();
	while( (ad = m_PollingStartdAds.Next()) ){
		m_PollingStartdAds.Delete(ad);
	}

	m_CkptBatches.Rewind();
	while( (ad = m_CkptBatches.Next()) ){
		m_CkptBatches.Delete(ad);
	}

	m_CkptBenchMarks.Rewind();
	while( (ad = m_CkptBenchMarks.Next()) ){
		m_CkptBenchMarks.Delete(ad);
	}

	while( (ad = m_claimed_standard.Next()) ){
		m_claimed_standard.Delete(ad);
	}

	dprintf(D_ALWAYS,"shutdownGraceful .. cleaning hashes \n");

	empty_Hashes();
	fclose(m_spoolStorage);

	return 0;
}

int
AdminEvent::config( bool init )
{
	// Initial values
	if ( init ) {
		dprintf(D_FULLDEBUG, "AE::config(true) \n");
		m_timeridDoShutdown = -1;
		m_intervalDoShutdown = 60;

		m_benchmark_lastsize = 0;
		m_haveShutdown = false;
		m_shutdownTime = 0;
		m_shutdownTarget = "";
		m_mystate = EVENT_SAMPLING;
	} else {
		dprintf(D_FULLDEBUG, "AE::config(false) \n");
	}

	empty_Hashes();

	/* Lets check events which might be set */
	check_Shutdown(init);
	return 0;
}

int
AdminEvent::check_Shutdown( bool init )
{
	char *timeforit;
	char *tmp;

	dprintf(D_FULLDEBUG, "Checking For Shutdown\n");

	// Get EVENTD_SHUTDOWN_TIME parameter
	timeforit = param( "EVENTD_SHUTDOWN_TIME" );
	if( timeforit ) {
		 dprintf(D_FULLDEBUG, "EVENTD_SHUTDOWN_TIME is %s\n",
		 		timeforit);
		 process_ShutdownTime( timeforit );
		 free( timeforit );
	}

	/* Are we currently set with a shutdown? Is this a reconfig to same value? */
	if(m_timeridDoShutdown >= 0)
	{
		/* 
		  we have one set and if it is not the same time, calculate when
		  and reset the timer.
		*/
		if( m_shutdownTime != m_newshutdownTime){
			dprintf(D_ALWAYS, 
					"AE: We have a repeat shutdown event. ignoring\n");
		} else {
			if((m_newshutdownTime != 0) && (m_newshutdownTarget.Length() != 0)){
				time_t timeNow = time(NULL);
				if(m_newshutdownTime > timeNow) {
					m_intervalDoShutdown = 
							(unsigned)(m_newshutdownTime - timeNow);
					m_shutdownTime = m_newshutdownTime;
					if (( m_timeridDoShutdown < 0 ) 
							&& (m_intervalDoShutdown > 60)) 
					{
						dprintf(D_ALWAYS, 
							"AE: shutdown event. Set timer <<%d>> from now\n",
							m_intervalDoShutdown);
						m_timeridDoShutdown = daemonCore->Reset_Timer(
							m_timeridDoShutdown, m_intervalDoShutdown, 0);
					} else {
						dprintf(D_ALWAYS, 
							"AE::chk_Shtdwn: Shutdown denied (why)\n");
					}
				} else {
					dprintf(D_ALWAYS, 
						"AE::chk_Shtdwn: Shutdown denied (past or too close\n");
				}
			}
		}
	} else {
		/* this is a new timer request */
		if((m_newshutdownTime != 0) && (m_newshutdownTarget.Length() != 0)){
			time_t timeNow = time(NULL);
			if(m_newshutdownTime > timeNow) {
				m_intervalDoShutdown = (unsigned)(m_newshutdownTime - timeNow);
				m_shutdownTime = m_newshutdownTime;
				if (( m_timeridDoShutdown < 0 ) && (m_intervalDoShutdown > 60)) {
					dprintf(D_ALWAYS, 
						"AE: shutdown event. Set timer <<%d>> from now\n",
						m_intervalDoShutdown);
					m_timeridDoShutdown = daemonCore->Register_Timer(
						m_intervalDoShutdown,
						(TimerHandlercpp)&AdminEvent::th_DoShutdown,
						"AdminEvent::DoShutdown()", this );
				} else {
						dprintf(D_ALWAYS, 
							"AE::chk_Shtdwn: Shutdown denied (why)\n");
				}
			} else {
				dprintf(D_ALWAYS, 
					"AE::chk_Shtdwn: denied either past or too close!\n");
			}
		}
	}

	// Get EVENTD_SHUTDOWN_MACHINES parameter
	tmp = param( "EVENTD_SHUTDOWN_MACHINES" );
	if(tmp) {
		m_newshutdownTarget = tmp;
		dprintf(D_ALWAYS, 
				"EVENTD_SHUTDOWN_MACHINES is %s\n",tmp);
		free( tmp );
	}

	tmp = param("SPOOL");
	if(tmp) {
		if(spoolClassAd(m_lastShutdown,"in") == 1) {
			dprintf(D_ALWAYS,"Failed to get/create initial classad from spool\n");
		}
		free(tmp);
	} else {
		EXCEPT( "SPOOL not defined!" );
	}

	// Get EVENTD_ADMIN_MEGABITS_SEC parameter
	tmp = param( "EVENTD_ADMIN_MEGABITS_SEC" );
	if(tmp) {
		/* adjust to megabytes */
		m_newshutdownAdminRate = (atoi(tmp)/8);
		dprintf(D_ALWAYS, 
				"EVENTD_ADMIN_MEGABITS_SEC is %d\n",
				m_newshutdownAdminRate);
		free( tmp );
	} else {
		EXCEPT( "EVENTD_ADMIN_MEGABITS_SEC not defined!" );
	}

	// Get EVENTD_SHUTDOWN_SIZE parameter
	tmp = param( "EVENTD_SHUTDOWN_SIZE" );
	if( tmp ) {
		m_newshutdownSize = atoi(tmp);
		dprintf(D_ALWAYS, 
				"EVENTD_SHUTDOWN_SIZE is %s\n",tmp);
		free( tmp );
	}

	// Get EVENTD_VACATE_POLLING_START_SIZE parameter
	tmp = param( "EVENTD_VACATE_POLLING_START_SIZE" );
	if( tmp ) {
		m_benchmark_size = atoi(tmp);
		dprintf(D_ALWAYS, 
				"EVENTD_VACATE_POLLING_START_SIZE is %s\n",tmp);
		free( tmp );
	}

	// Get EVENTD_VACATE_POLLING_START_SIZE_INCREMENT parameter
	tmp = param( "EVENTD_VACATE_POLLING_START_SIZE_INCREMENT" );
	if( tmp ) {
		m_benchmark_increment = atoi(tmp);
		dprintf(D_ALWAYS, 
				"EVENTD_VACATE_POLLING_START_SIZE_INCREMENT is %s\n",
				tmp);
		free( tmp );
	}

	// Get EVENTD_VACATE_POLLING parameter
	tmp = param( "EVENTD_VACATE_POLLING" );
	if( tmp ) {
		m_intervalPeriod_PollingVacates = atoi(tmp);
		dprintf(D_ALWAYS, 
				"EVENTD_VACATE_POLLING is %s\n",tmp);
		free( tmp );
	}

	// Get EVENTD_SHUTDOWN_CONSTRAINT parameter
	tmp = param( "EVENTD_SHUTDOWN_CONSTRAINT" );
	if( tmp ) {
		m_shutdownConstraint = tmp;
		dprintf(D_ALWAYS, 
				"EVENTD_SHUTDOWN_CONSTRAINT is %s\n",tmp);
		free( tmp );
	}

	if(m_timerid_DoShutdown_States > 0) {
		//daemonCore->Reset_Timer(m_timerid_DoShutdown_States, 
			//m_intervalCheck_DoShutdown_States);
		m_timerid_DoShutdown_States = daemonCore->Register_Timer(
			m_intervalCheck_DoShutdown_States,
			(TimerHandlercpp)&AdminEvent::th_DoShutdown_States,
			"AdminEvent::DoShutdown_States()", this );
			dprintf(D_ALWAYS,
				"th_Check_PollingVacates <<< REGISTER TIMER ((+%d)) ID=%d >>>\n"
				,m_intervalCheck_DoShutdown_States, m_timerid_DoShutdown_States);
	} else {
		m_timerid_DoShutdown_States = daemonCore->Register_Timer(
			m_intervalCheck_DoShutdown_States,
			(TimerHandlercpp)&AdminEvent::th_DoShutdown_States,
			"AdminEvent::DoShutdown_States()", this );
			dprintf(D_ALWAYS,
				"th_Check_PollingVacates <<< REGISTER TIMER ((+%d)) ID=%d >>>\n"
				,m_intervalCheck_DoShutdown_States, m_timerid_DoShutdown_States);
	}
	return(0);
}

int
AdminEvent::spoolClassAd( ClassAd * ca_shutdownRate, char *direction )
{
	char *tmp;

	tmp = param("SPOOL");
	if(strcmp(direction,"in") == 0) {
		m_spoolHistory = dircat(tmp,"EventdShutdownRate.log");
		m_spoolStorage = fopen((const char *)m_spoolHistory.Value(),"r");
		if(m_spoolStorage != NULL) {
			int isEOF=0, error=0, empty=0;
			m_lastShutdown = new ClassAd(m_spoolStorage,"//*",isEOF,error,empty);
			if(m_lastShutdown != NULL) {
				dprintf(D_ALWAYS,"Got initial classad from spool(%s)\n",
					m_spoolHistory.Value());
				fclose(m_spoolStorage);
				m_lastShutdown->dPrint(D_ALWAYS);
			} else {
				dprintf(D_ALWAYS,"Failed to get initial classad from spool(%s)\n",
					m_spoolHistory.Value());
				return(1);
			}
		} else {
			char 	line[100];
			int 	lastrate = 0;

			dprintf(D_ALWAYS,"Failed to open from spool(%s/%d)\n",
				m_spoolHistory.Value(),errno);
			m_lastShutdown = new ClassAd();
			sprintf(line, "%s = %d", "LastShutdownRate", lastrate);
			m_lastShutdown->Insert(line);
			m_lastShutdown->dPrint(D_ALWAYS);
			m_spoolStorage = fopen((const char *)m_spoolHistory.Value(),"w+");
			if(m_spoolStorage != NULL) {
				m_lastShutdown->fPrint(m_spoolStorage);
				fclose(m_spoolStorage);
			} else {
				dprintf(D_ALWAYS,"Failed to open from spool for writing (%s/%d)\n",
					m_spoolHistory.Value(),errno);
				return(1);
			}
		}
		return(0);
	} else if(strcmp(direction,"out") == 0) {
		m_spoolStorage = fopen((const char *)m_spoolHistory.Value(),"w");
		if(m_spoolStorage != NULL) {
			m_lastShutdown->fPrint(m_spoolStorage);
			fclose(m_spoolStorage);
		} else {
			dprintf(D_ALWAYS,"Failed to open from spool for writing (%s/%d)\n",
				m_spoolHistory.Value(),errno);
			return(1);
		}
		return(0);
	} else {
		return(1);
	}
}

/*

		States

*/

int 
AdminEvent::th_DoShutdown_States( void )
{
	/**

	Handle Shutdown States.....

	Until happy call
		do_checkpoint_samples
	Then set up a callback allowing time to collect current ads
		set up the number of runs through to make
		start doing batches of vacates
	So, how long has it been since we benchmarked the current
		network for vacate optimal throughput????

	Consider recounting how much data is in images of CURRENT
		standard universe jobs.... Set times and alarms based
		on this new polling... It could have been a week ago
		that they set this up even though our currently understood
		time in this code is today......

	**/

	dprintf(D_FULLDEBUG, "AE::th_DoShutdown_States(STATE = %d) \n",m_mystate);

	switch(m_mystate)
	{
		case EVENT_INIT:
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States( no work here - EVENT_INIT: )\n");
			break;
		case EVENT_HUERISTIC:
			dprintf(D_ALWAYS, "AE::th_DoShutdown_States(FetchAds) \n");
			empty_Hashes();
			FetchAds_ByConstraint((char *)m_shutdownConstraint.Value());
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States(do_checkpoint_samples) \n");
			do_checkpoint_samples(true);
			break;

		case EVENT_EVAL_HUERISTIC:
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States(benchmark_analysis) \n");
			benchmark_analysis();
			break;

		case EVENT_SAMPLING:
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States( work here - EVENT_SAMPLING: )\n");
			// how much is running? estimate when to wake up by total image size
			// and the admin checkpointing capacity
			check_Shutdown(true);
			FetchAds_ByConstraint((char *)m_shutdownConstraint.Value());
			totalRunningJobs();
			break;

		case EVENT_EVAL_SAMPLING:
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States( no work - EVENT_EVAL_SAMPLING: )\n");
			break;

		case EVENT_MAIN_WAIT:
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States( no work - EVENT_MAIN_WAIT: )\n");
			break;

		case EVENT_RESAMPLE:
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States( no work - EVENT_RESAMPLE: )\n");
			break;

		case EVENT_GO:
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States( no work - EVENT_GO: )\n");
			break;

		//case EVENT_INIT:
			//break;

		default:
			dprintf(D_ALWAYS, 
				"AE::th_DoShutdown_States( Why Default Case )\n");
			break;
	}
	return(0);
}

/*

		Timers

*/

int 
AdminEvent::th_DoShutdown( void )
{
	DCMaster *		d;
	//bool wantTcp = 	true;

	dprintf(D_ALWAYS,"th_DoShutdown\n");

	/* mark this timer as NOT active */
	m_timeridDoShutdown = -1;

	/* when we wake up we do the shutdown */
	dprintf(D_ALWAYS, 
		"<<<Time For Shutdown!!!!--%s--!!!>>>\n",m_shutdownTarget.Value());
	d = new DCMaster(m_shutdownTarget.Value());
	dprintf(D_ALWAYS,"daemon name is %s\n",d->name());
	dprintf(D_ALWAYS,"call Shutdown now.....\n");
	//d->sendMasterOff(wantTcp);
	delete d;
	return(0);
}

int 
AdminEvent::th_Check_PollingVacates( void )
{
	ClassAd *ad;
	int enteredcurrentstate;
	MyString state;
	MyString name;
	MyString jobid;
	bool fFoundAd = false;
	bool fAllClear = true;

	/*
	   We want to get the ads for each machine in the pilot size
	   group and see if it has changed from the busy state and completed 
	   its checkpoint. If so we record the time and mark it done.
	   When they are all done, we set m_stillPoolingVacates to false
	   and we then make decisions.....
	*/
	int timespent = 0;
	int gotads = 0;
	StartdStats *ss;
	m_CkptTest_su.startIterations();
	while(m_CkptTest_su.iterate(ss) == 1) {
		// check out this job first
		if(ss->ckptdone == 0) { 
			/* Still need to do this one.... */
			gotads = pollStartdAds( m_PollingStartdAds, ss->myaddress, ss->name );
			if( gotads == 1 ){
				dprintf(D_ALWAYS,"failed to get ads for %s\n",ss->name);
				continue;
			}
			//dprintf(D_ALWAYS,"pollAds:BM ----<<%s-%s-%s>>----\n",
				//ss->myaddress, ss->name, ss->jobid);
			m_PollingStartdAds.Rewind();
			fFoundAd = false;
			while( (ad = m_PollingStartdAds.Next()) ){
				ad->LookupString( ATTR_NAME, name ); 
				ad->LookupString( ATTR_STATE, state ); 

				if((strcmp(state.Value(),"Unclaimed") == 0) 
						||(strcmp(state.Value(),"Owner") == 0) 
						|| (strcmp(state.Value(),"Claimed") == 0)) 
				{
					break; /* The job is gone on its own */
				}

				ad->LookupString( ATTR_JOB_ID, jobid ); 

				//dprintf(D_ALWAYS,"pollAds:AD ----<<%s-%s-%s>>----\n",
					//name.Value(), state.Value(), jobid.Value());

				if( (strcmp(name.Value(), ss->name) == 0) 
						&& (strcmp(jobid.Value(), ss->jobid) == 0) 
						&& (ss->ckptdone == 0) ) 
				{
					int now = (int) time(NULL);
					timespent = now - ss->ckpttime;
					fFoundAd = true;
					ss->state[0] = '\0';
					strcpy(ss->state,state.Value());
					ad->LookupInteger( ATTR_ENTERED_CURRENT_STATE, 
							enteredcurrentstate ); 
					if (ss->ckpttime > enteredcurrentstate ) {
						/* 
							still waiting on this node to checkpoint 
							and change state 
						*/
						dprintf(D_FULLDEBUG,
							"pollAds: Waiting on %s for %s in state %s\n",
							name.Value(),ss->jobid,ss->state);
						fAllClear = false;
					} else if( strcmp(ss->state,"Claimed") == 0) { 
						/* skip checks for this job for future benchmarks */
						dprintf(D_FULLDEBUG,
							"pollAds: CLAIMED??? %s for %s in state <%s>\n",
							name.Value(),ss->jobid,ss->state);
						fAllClear = false;
					} else if(strcmp(ss->state,"Preempting") != 0) {
							SS_store(ss,timespent);
							dprintf(D_FULLDEBUG,
								"pollAds: ckptpt took <<%d>> for <<%s>>!<<STATE=%s>>\n",
								ss->ckptlength, ss->name,ss->state);
					} else {
						dprintf(D_FULLDEBUG,
							"pollAds: Still Waiting on %s for %s in state <<%s>>\n",
							name.Value(),ss->jobid,ss->state);
						SS_test(ss,timespent);
						fAllClear = false;
					}
				}
				m_PollingStartdAds.Delete(ad); /* no reason to keep it */
			}
			if(!fFoundAd) {
				/* The job is done and gone.... quit waiting.....*/
				time_t timeNow = time(NULL);
				timespent = timeNow - ss->ckpttime;
				SS_store(ss, timespent);
			}
		} else {
			char *donestr;
			if(fAllClear) {
				donestr = "allclear";
			} else {
				donestr = "NOT-allclear";
			}
			//dprintf(D_ALWAYS,"th_Check_PollingVacates(((DONE))): <<Name %s Size %d>>\n",
				//ss->name,ss->ckptmegs);
			//dprintf(D_ALWAYS,"th_Check_PollingVacates <<< %s >>>\n",donestr);
		}
	}

	if(fAllClear){
		daemonCore->Cancel_Timer(m_timerid_PollingVacates);

		dprintf(D_ALWAYS, 
			"th_Check_PollingVacates: m_timerid_DoShutdown_States is %d\n"
			,m_timerid_DoShutdown_States);

		m_mystate = EVENT_EVAL_SAMPLING;
		if(m_timerid_DoShutdown_States > 0) {
			//daemonCore->Reset_Timer(m_timerid_DoShutdown_States, 
				//m_intervalCheck_DoShutdown_States);
			m_timerid_DoShutdown_States = daemonCore->Register_Timer(
				m_intervalCheck_DoShutdown_States,
				(TimerHandlercpp)&AdminEvent::th_DoShutdown_States,
				"AdminEvent::DoShutdown_States()", this );
			dprintf(D_ALWAYS,"th_Check_PollingVacates <<< RESET TIMER(Need to fix this!) ((+%d)) >>>\n"
					,m_intervalCheck_DoShutdown_States);
		} else {
			m_timerid_DoShutdown_States = daemonCore->Register_Timer(
				m_intervalCheck_DoShutdown_States,
				(TimerHandlercpp)&AdminEvent::th_DoShutdown_States,
				"AdminEvent::DoShutdown_States()", this );
			dprintf(D_ALWAYS,
				"th_Check_PollingVacates <<< REGISTER TIMER ((+%d)) ID=%d >>>\n"
				,m_intervalCheck_DoShutdown_States, m_timerid_DoShutdown_States);
		}
	}
	return(0);
}

/*

		EVENT HANDLING METHODS

*/

int
AdminEvent::do_checkpoint_samples( bool init )
{
	ClassAd *ad;

	/* 
		if we processed a good combo of a time and target(s) set a timer
		to do the dirty deed. At the momment we just care about how
		many Standard Universe Jobs we have. We benchmark both now and
		in enough time to handle what we just found and what we find then.
		Thus we can set a roll back timer schedule to send out batches
		of checkpoint requests. 
	*/
	
	dprintf(D_ALWAYS,"do_checkpoint_samples\n");

	m_claimed_standard.Rewind();
	if((ad = m_claimed_standard.Next()) ){
		 dprintf(D_ALWAYS, 
		 	"AE::do_checkpoint_samples() starting checkpointing list and benchmark\n");
		/* 
			process the current standard universe jobs into a hash
			and then prepare a benchmark test. 
		*/

		/* Add all the Standard Universe Ads to our hash */
		//dprintf(D_ALWAYS,
			//"About to call standardUProcess:< %d elements and %d table size >\n",
			//m_JobNodes_su.getNumElements(), m_JobNodes_su.getTableSize());
		standardUProcess();

		/* show the current list of Ads */
		//standardUDisplay_StartdStats();

		run_ckpt_benchmark();

		//dprintf(D_ALWAYS, "setting check polling vacate times for +%d\n",
			//m_intervalCheck_PollingVacates);
		m_timerid_PollingVacates = daemonCore->Register_Timer(
			m_intervalCheck_PollingVacates,
			m_intervalPeriod_PollingVacates,
			(TimerHandlercpp)&AdminEvent::th_Check_PollingVacates,
			"AdminEvent::PollingVacates()", this );
		//dprintf(D_ALWAYS, "timer ID is %d\n",
			//m_timerid_PollingVacates);
		/**/

	}
	return(0);
}

/*

		MISC COMMANDS

*/

/*
	The following is a basic sort routine to sort the current
	standard universe jobs into longest running on their current claims 
	first. These are always our highest priority as they have the
	most to loose if shutdown without a checkpoint.

 	usage: classadlist.Sort( (SortFunctionType)ClaimRunTimeSort );

*/

int 
ClaimRunTimeSort(ClassAd *job1, ClassAd *job2, void *data)
{
    int claimruntime1=0, claimruntime2=0;

    job1->LookupInteger(ATTR_TOTAL_CLAIM_RUN_TIME, claimruntime1);
    job2->LookupInteger(ATTR_TOTAL_CLAIM_RUN_TIME, claimruntime2);
    if (claimruntime2 < claimruntime1) return 1;
    if (claimruntime2 > claimruntime1) return 0;
	return 0;
}

int
AdminEvent::empty_Hashes()
{
	StartdStats *ss;

	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		delete ss;
	}

	m_CkptTest_su.startIterations();
	while(m_CkptTest_su.iterate(ss) == 1) {
		delete ss;
	}

	m_JobNodes_su.clear(); /* clear our our sampling hash */
	m_CkptTest_su.clear(); /* clear out last benchmarking hash */
	return 0;
}

int
AdminEvent::pollStartdAds( ClassAdList &adsList, char *sinful, char *name )
{
	DCStartd *		d;
	bool fRes = true;

	//d = new DCStartd(name, NULL, sinful, NULL);
	d = new DCStartd(name, NULL);
	dprintf(D_FULLDEBUG,"daemon name is %s\n",d->name());
	dprintf(D_FULLDEBUG,"call getAds now.....\n");
	dprintf(D_FULLDEBUG, 
		"Want to get ads from  here ((%s)) and this vm ((%s))\n",sinful,name);
	fRes = d->getAds(adsList, name);
	if(fRes) {
		delete d;
		return(0);
	} else {
		delete d;
		return(1);
	}
}

int
AdminEvent::sendCheckpoint( char *sinful, char *name )
{
	DCStartd *		d;

	/* when we wake up we do the shutdown */
	d = new DCStartd(name, NULL, sinful, NULL);
	dprintf(D_FULLDEBUG,"daemon name is %s\n",d->name());
	dprintf(D_FULLDEBUG,"call checkpointJob now.....\n");
	dprintf(D_FULLDEBUG, 
		"Want to checkpoint to here ((%s)) and this vm ((%s))\n",sinful,name);
	d->checkpointJob(name);
	delete d;
	return(0);
}

int
AdminEvent::sendVacateClaim( char *sinful, char *name )
{
	DCStartd *		d;
	ClassAd reply;
	//int timeout = -1;
	//bool fRes = true;

	/* when we wake up we do the shutdown */
	d = new DCStartd(name, NULL, sinful, NULL);
	d->vacateClaim(name);
	//fRes = d->releaseClaim(VACATE_GRACEFUL,&reply,timeout);
	//if(!fRes) {
		//dprintf(D_ALWAYS,
			//"d->releaseClaim(VACATE_GRACEFUL,&reply,timeout) FAILED\n");
	//}
	delete d;
	return(0);
}

/*
	As we get a feel for the checkpointing capacity we want to
	do batches of particular sizes to average impact of that size
	a batch. It may be that the pool is quiet now with few jobs
	and will be busy later. Allow testing for enough jobs for
	the given iteration. Might also be that we are testing on a smaller
	subset of jobs set my some constraint.
*/

bool 
AdminEvent::have_requested_batch( int batchsz )
{
	int accumulated = 0;
	StartdStats *ss;

	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		accumulated += (ss->imagesize/1000);
		//dprintf(D_ALWAYS,"have_requested_batch<partial> wanted %d had %d\n",
			//batchsz, accumulated);
		if(accumulated >= batchsz) {
			dprintf(D_ALWAYS,"have_requested_batch<TRUE> wanted %d had %d\n",
				batchsz, accumulated);
			return(true);
		}
	}
	dprintf(D_ALWAYS,"have_requested_batch<FALSE> wanted %d had %d\n",
		batchsz, accumulated);
	return(false);
}

/*
	Iterate through standard jobs until we hit megs limit. Fill out limited 
	stat records in m_CkptTest_su. Set a timer to wait for Ckpts and then see
	how long they took. Consider lower or higher quantity to find "best" 
	quantity to do at once to see when network saturation occurs..... ?????
*/

int
AdminEvent::run_ckpt_benchmark()
{
	int megs = 0;
	int current_size = 0;
	StartdStats *ss, *tt;
	//dprintf(D_ALWAYS,"*************<<<run_ckpt_benchmark>>>*********\n");

	if(m_benchmark_lastsize == 0) {
		/* first check */
		megs = m_benchmark_lastsize = m_benchmark_size;
		m_benchmark_iteration = 0;
	} else if(m_benchmark_iteration == BATCH_SIZE) {
		compute_ckpt_batches();
		m_benchmark_iteration = 0;
		megs = m_benchmark_lastsize = 
			(m_benchmark_lastsize + m_benchmark_increment);
	} else {
		megs = m_benchmark_lastsize;
	}

	if( ! have_requested_batch(megs) ) {
		dprintf(D_ALWAYS,
			"In run_ckpt_benchmark SU jobs<< waiting for batch size >>\n");
	 	return(0);
	} else {
		/* looks like we'll be fine for another iteration */
		//dprintf(D_ALWAYS,
			//"In run_ckpt_benchmark SU jobs<< batch size met >>\n");
		m_benchmark_iteration++;
	}

	m_JobNodes_su.startIterations();
	//dprintf(D_ALWAYS,
		//"In run_ckpt_benchmark SU jobs<< %d elements and %d table size >>\n",
		//m_JobNodes_su.getNumElements(), m_JobNodes_su.getTableSize());
	while(m_JobNodes_su.iterate(ss) == 1) {
		HashKey namekey(ss->name);	// where do we belong?
		//dprintf(D_ALWAYS,"hashing %s imagesize %d<<%d>> \n",
			//ss->name,ss->imagesize,(ss->imagesize/1000));
		if(m_CkptTest_su.lookup(namekey,tt) < 0) {
			if((ss->imagesize/1000) > 40) {
				// Good, lets add it in and accumulate the size in our total
				tt = new StartdStats(ss->name, ss->universe, ss->imagesize, ss->lastcheckpoint);
				tt->jobstart = ss->jobstart;
				tt->virtualmachineid = ss->virtualmachineid;

				tt->ckptmegs = (ss->imagesize/1000); // how big was it at checkpoint size
				tt->ckpttime = (int) time(NULL); 
				tt->ckptgroup = 0; 
				tt->ckptdone = 0; 
				tt->ckptlength = 0; 

				strcpy(tt->clientmachine, ss->clientmachine);
				strcpy(tt->state, ss->state);
				strcpy(tt->activity, ss->activity);
				strcpy(tt->myaddress, ss->myaddress);
				strcpy(tt->jobid, ss->jobid);
				strcpy(tt->ckptsrvr, ss->ckptsrvr);

				m_CkptTest_su.insert(namekey,tt);

				//sendCheckpoint(ss->myaddress,ss->name);
				sendVacateClaim(ss->myaddress,ss->name);
	
				current_size += (tt->ckptmegs);
				dprintf(D_ALWAYS,"Test Checkpoint Benchmark now at <%d> Megs\n",
					current_size);
			}
		} else {
			dprintf(D_ALWAYS,"Why is %s already in hash table??\n",ss->name);
		}

		if( current_size > megs ) {
			/* done with this size test */
			break;
		}

		//delete ss;
	}
	m_haveBenchStats = true;
	return(0);
}

int 
AdminEvent::benchmark_insert( float megspersec, int megs, int time )
{
	ClassAd *ad = new ClassAd();
	int mylength = 0;

	char line[100];
	sprintf(line, "%s = %f", "MegsPerSec", megspersec);
	ad->Insert(line);

	sprintf(line, "%s = %d", "Megs", megs);
	ad->Insert(line);

	sprintf(line, "%s = %d", "Length", time);
	ad->Insert(line);

	m_CkptBenchMarks.Insert(ad);
	mylength = m_CkptBenchMarks.MyLength();
	//dprintf(D_ALWAYS,"*****<<<<<Benchmark ads at %d>>>>>*****\n",mylength);
	return(0);
}

int
AdminEvent::compute_ckpt_batches()
{
	ClassAd *ad;
	ClassAd *new_ad = new ClassAd();
	char line[100];

	int howmany = 0;
	float 	megspersec = 0;
	float 	megspersec_tot = 0;
	int 	megs = 0;
	int 	longesttime = 0;

	m_CkptBenchMarks.Rewind();
	while( (ad = m_CkptBenchMarks.Next()) ){
		ad->LookupFloat( "MegsPerSec", megspersec );
		ad->LookupInteger( "Megs", megs );
		ad->LookupInteger( "Length", longesttime );
		megspersec_tot += megspersec;
		howmany++;
		//delete ad;
	}
	megspersec_tot = megspersec_tot/howmany;
	dprintf(D_ALWAYS,"Average checkpoints at %f for %d samples\n",
		megspersec_tot, howmany);

	sprintf(line, "%s = %f", "MegsPerSec", megspersec_tot);
	new_ad->Insert(line);

	sprintf(line, "%s = %d", "BatchSize", m_benchmark_lastsize);
	new_ad->Insert(line);

	m_CkptBatches.Insert(new_ad);

	/* remove the batch from rolling sampling */
	m_CkptBenchMarks.Rewind();
	while( (ad = m_CkptBenchMarks.Next()) ){
		m_CkptBenchMarks.Delete(ad);
	}
	return(0);
}

int
AdminEvent::benchmark_show_results()
{
	ClassAd *ad;
	float 	megspersec = 0;
	int 	megs = 0;
	int 	longesttime = 0;
	int 	batchsize = 0;

	
	dprintf(D_ALWAYS,"***** Individual Ratings *****\n");
	m_CkptBenchMarks.Rewind();
	while( (ad = m_CkptBenchMarks.Next()) ){
		ad->LookupFloat( "MegsPerSec", megspersec );
		ad->LookupInteger( "Megs", megs );
		ad->LookupInteger( "Length", longesttime );
		dprintf(D_ALWAYS,"MegsPerSec = %f Megs = %d Longest = %d\n",
			megspersec, megs, longesttime);
	}

	dprintf(D_ALWAYS,"***** Batch Ratings *****\n");
	m_CkptBatches.Rewind();
	while( (ad = m_CkptBatches.Next()) ){
		ad->LookupFloat( "MegsPerSec", megspersec );
		ad->LookupInteger( "BatchSize", batchsize );
		dprintf(D_ALWAYS,"MegsPerSec = %f BatchSize = %d\n",
			megspersec, batchsize);
	}
	return(0);
}

int
AdminEvent::benchmark_analysis()
{
	StartdStats *ss;
	float megspersec_accumulator = 0;
	float megspersec_result = 0;
	float megspersec_votes = 0;

	int megs = 0;
	int shortest = 0;
	int longest = 0;

	float tossed_megspersec_accumulator = 0;
	float tossed_megspersec_result = 0;
	float tossed_megspersec_votes = 0;

	int tossed_megs = 0;
	int tossed_shortest = 0;
	int tossed_longest = 0;

	bool f_continue = true;
	bool f_anyjobs = false;

	m_CkptTest_su.startIterations();
	//dprintf(D_ALWAYS,"benchmark_analysis <<< %d elements and %d table size >>\n",
		//m_CkptTest_su.getNumElements(), m_CkptTest_su.getTableSize());
	while(m_CkptTest_su.iterate(ss) == 1) {
		if(ss->ckptdone == 1) {
			/* one's thrown out are set to -1 */
			f_anyjobs = true;

			dprintf(D_ALWAYS,
				"+++++++++++++++++<<benchmark_analysis>>+++++++++++++++++\n");
			dprintf(D_ALWAYS,
				"Good JobId %s Name %s Image Sz %d\n",
				ss->jobid,ss->name,ss->imagesize);
			dprintf(D_FULLDEBUG,"State %s\n",ss->state);
			dprintf(D_FULLDEBUG,"Activity %s\n",ss->activity);
			dprintf(D_FULLDEBUG,"ClientMachine %s\n",ss->clientmachine);
			dprintf(D_FULLDEBUG,"MyAddress %s\n",ss->myaddress);
			dprintf(D_FULLDEBUG,"Universe %d\n",ss->universe);
			dprintf(D_FULLDEBUG,"Jobstart %d\n",ss->jobstart);
			dprintf(D_FULLDEBUG,"LastCheckPoint %d\n",ss->lastcheckpoint);
			dprintf(D_FULLDEBUG,"VirtualMachineId %d\n",ss->virtualmachineid);
			//dprintf(D_ALWAYS,"======================================\n");
			dprintf(D_FULLDEBUG,"CkptTime %d\n",ss->ckpttime);
			dprintf(D_ALWAYS,"CkptLength %d CkptMegs %d CkptMegspersec %f\n",
				ss->ckptlength,ss->ckptmegs,ss->ckptmegspersec);
			dprintf(D_FULLDEBUG,"CkptGroup %d\n",ss->ckptgroup);
			dprintf(D_FULLDEBUG,"CkptDone %d\n",ss->ckptdone);
			/* maintain shortest */
			if( shortest == 0 ) {
				shortest = ss->ckptlength;
			} else if( ss->ckptlength < shortest) {
				shortest = ss->ckptlength;
			}
			/* maintain longest */
			if( ss->ckptlength > longest) {
				longest = ss->ckptlength;
			}
			megspersec_accumulator += ss->ckptmegspersec;
			megspersec_votes += 1;
			megs += ss->ckptmegs;
		} else if(ss->ckptdone == -1) {
			/* one's thrown out are set to -1 */
			f_anyjobs = true;

			dprintf(D_ALWAYS,
				"+++++++++++++++++<<benchmark_analysis>>+++++++++++++++++\n");
			dprintf(D_ALWAYS,
				"Tossed JobId %s Name %s Image Sz %d\n",
				ss->jobid,ss->name,ss->imagesize);
			dprintf(D_FULLDEBUG,"State %s\n",ss->state);
			dprintf(D_FULLDEBUG,"Activity %s\n",ss->activity);
			dprintf(D_FULLDEBUG,"ClientMachine %s\n",ss->clientmachine);
			dprintf(D_FULLDEBUG,"MyAddress %s\n",ss->myaddress);
			dprintf(D_FULLDEBUG,"Universe %d\n",ss->universe);
			dprintf(D_FULLDEBUG,"Jobstart %d\n",ss->jobstart);
			dprintf(D_FULLDEBUG,"LastCheckPoint %d\n",ss->lastcheckpoint);
			dprintf(D_FULLDEBUG,"VirtualMachineId %d\n",ss->virtualmachineid);
			//dprintf(D_ALWAYS,"======================================\n");
			dprintf(D_FULLDEBUG,"CkptTime %d\n",ss->ckpttime);
			dprintf(D_ALWAYS,"CkptLength %d CkptMegs %d CkptMegspersec %f\n",
				ss->ckptlength,ss->ckptmegs,ss->ckptmegspersec);
			dprintf(D_FULLDEBUG,"CkptGroup %d\n",ss->ckptgroup);
			dprintf(D_FULLDEBUG,"CkptDone %d\n",ss->ckptdone);
			/* maintain tossed_shortest */
			if( tossed_shortest == 0 ) {
				tossed_shortest = ss->ckptlength;
			} else if( ss->ckptlength < tossed_shortest) {
				tossed_shortest = ss->ckptlength;
			}
			/* maintain tossed_longest */
			if( ss->ckptlength > tossed_longest) {
				tossed_longest = ss->ckptlength;
			}
			tossed_megspersec_accumulator += ss->ckptmegspersec;
			tossed_megspersec_votes += 1;
			tossed_megs += ss->ckptmegs;
		}
	}
	if(f_anyjobs) {
		megspersec_result = (megspersec_accumulator/megspersec_votes);
		tossed_megspersec_result = (tossed_megspersec_accumulator/tossed_megspersec_votes);
		dprintf(D_ALWAYS,"Store %f megs per sec %d size\n",megspersec_result,megs);
		dprintf(D_ALWAYS,"TOSSED %f megs per sec %d size\n",tossed_megspersec_result,tossed_megs);
		f_continue = benchmark_store_results(megspersec_result, megs, longest);
		benchmark_show_results();
	}
	/*
		if this is true, we have not seen a substantial impact to increasing
		size of total vacated jobs...
	*/

	if(!f_continue) {
		/* Set wake up time for resampling */
		m_mystate = EVENT_MAIN_WAIT;
	} else {
		/* back to sampling state */
		m_mystate = EVENT_SAMPLING;
	}

	if(m_timerid_DoShutdown_States > 0) {
		daemonCore->Reset_Timer(m_timerid_DoShutdown_States, 
			m_intervalCheck_DoShutdown_States);
	} else {
		m_timerid_DoShutdown_States = daemonCore->Register_Timer(
			m_intervalCheck_DoShutdown_States,
			(TimerHandlercpp)&AdminEvent::th_DoShutdown_States,
			"AdminEvent::DoShutdown_States()", this );
	}

	return(0);
}

int AdminEvent::SS_store(StartdStats *ss, int duration)
{
	/* record this one */
	ss->ckptdone = 1;
	ss->ckptlength = duration;
	ss->ckptmegspersec = (float)((float)ss->ckptmegs/(float)ss->ckptlength);

	/* look for ones too far off by updating the latest to complete */
	m_NrightNow_megspersec = ss->ckptmegspersec;
	m_NrightNow_size = ss->ckptmegs;
	m_NrightNow_time = duration;
	dprintf(D_ALWAYS,"Store: MPS %f MEGS %d TIME %d\n",m_NrightNow_megspersec,
		m_NrightNow_size, m_NrightNow_time);

	return(0);
}

int AdminEvent::SS_test(StartdStats *ss, int duration)
{
	/* compare to last good checkpoint */
	float megspersec = (float)((float)ss->ckptmegs/(float)duration);
	if(m_NrightNow_megspersec == 0) {
		if(megspersec < .3) {
			ss->ckptdone = -1; /* mark not usable */
			dprintf(D_ALWAYS,"tossing %s using chkptsrvr %s size %d(Last %f This %f)\n",
				ss->name,ss->ckptsrvr,ss->ckptmegs,m_NrightNow_megspersec,megspersec);
		} else {
			dprintf(D_ALWAYS,"Test: MPS %f MEGS %d TIME %d\n",megspersec,
				ss->ckptmegs, duration);
		}
	//} else if( m_NrightNow_megspersec/megspersec > 8.0 ) {
		//ss->ckptdone = -1; /* mark not usable */
		//dprintf(D_ALWAYS,"tossing %s using chkptsrvr %s Size %d(Last %f This %f)\n",
			//ss->name,ss->ckptsrvr,ss->ckptmegs,m_NrightNow_megspersec,megspersec);
		/* this one is twice as slow, toss it */
	} else {
		dprintf(D_FULLDEBUG,"Test: MPS %f MEGS %d TIME %d\n",megspersec,
			ss->ckptmegs, duration);
	}

	return(0);
}

bool
AdminEvent::benchmark_store_results(float  megspersec, int totmegs, int tottime)
{
	bool fContinue = true;

	benchmark_insert( megspersec, totmegs, tottime);

	return(fContinue);	/* no appreciable length increase - 
						go bigger on vacate size */
}

/*

		PROCESSING ROUTINES

*/

int
AdminEvent::process_ShutdownTime( char *req_time )
{
	// Crunch time into time_t format via str to tm
	struct tm tm;
	struct tm *tmnow;
	char *res;
	time_t timeNow = time(NULL);

dprintf(D_ALWAYS, "timeNow is %ld\n",timeNow);

	// make a tm with info for now and reset
	// secs, day and hour from the next scan of the req_time`
	tmnow = localtime(&timeNow);

	// find secs, hour and minutes
	res = strptime(req_time,"%H:%M:%S",&tm);
	if(res != NULL) {
		dprintf(D_ALWAYS, "Processing Shutdown Time String<<LEFTOVERS--%s-->>\n",res);
		//return(-1);
	}

	// Get today into request structure
	tm.tm_mday = tmnow->tm_mday;
	tm.tm_mon = tmnow->tm_mon;
	tm.tm_year = tmnow->tm_year;
	tm.tm_wday = tmnow->tm_wday;
	tm.tm_yday = tmnow->tm_yday;
	tm.tm_isdst = tmnow->tm_isdst;

	dprintf(D_ALWAYS, "(tmnow after change):Processing Shutdown Time seconds <%d> minutes <%d> hours <%d>\n",
		tm.tm_sec,tm.tm_min,tm.tm_hour);
	// Get our time_t value
	m_newshutdownTime = mktime(&tm);
	
	return(0);
}

int
AdminEvent::FetchAds_ByConstraint( char *constraint )
{
	CondorError errstack;
	CondorQuery *query;
    QueryResult q;
	ClassAd *ad;
	DCCollector* pool = NULL;
	AdTypes     type    = (AdTypes) -1;

	MyString machine;
	MyString state;
	MyString sinful;
	MyString name;
	MyString remoteuser;

	int jobuniverse = -1;
	int totalclaimruntime = -1;

	MyString machine2;
	MyString state2;
	MyString sinful2;
	MyString name2;
	MyString remoteuser2;

	pool = new DCCollector( "" );

	if( !pool->addr() ) {
		dprintf (D_ALWAYS, 
			"Getting Collector Object Error:  %s\n",pool->error());
		return(1);
	}

	// fetch the query

	// we are looking for starter ads
	type = STARTD_AD;

	// instantiate query object
	if( !(query = new CondorQuery (type))) {
		dprintf (D_ALWAYS, 
			"Getting Collector Query Object Error:  Out of memory\n");
		return(1);
	}

	dprintf(D_FULLDEBUG, "Processing Shutdown constraint String<<%s>>\n",
		constraint);

	query->addORConstraint( constraint );

	q = query->fetchAds( m_collector_query_ads, pool->addr(), &errstack);

	if( q != Q_OK ){
		dprintf(D_ALWAYS, "Trouble fetching Ads with<<%s>><<%d>>\n",
			constraint,q);
		delete query;
		delete pool;
		return(1);
	}

	if( m_collector_query_ads.Length() <= 0 ){
		dprintf(D_ALWAYS, "Found no ClassAds matching <<%s>> <<%d results>>\n",
			constraint,m_collector_query_ads.Length());
	} else {
		dprintf(D_FULLDEBUG, "Found <<%d>> ClassAds matching <<%s>>\n",
			m_collector_query_ads.Length(),constraint);
	}

	// output result
	// we always fill the sorted class ad lists with the result of the query
	m_claimed_standard.Rewind();
	m_collector_query_ads.Rewind();
	while( (ad = m_collector_query_ads.Next()) ){
		ad->LookupString( ATTR_MACHINE, machine );
		ad->LookupString( ATTR_STATE, state );
		ad->LookupString( ATTR_MY_ADDRESS, sinful );
		ad->LookupString( ATTR_REMOTE_USER, remoteuser );
		ad->LookupInteger( ATTR_JOB_UNIVERSE, jobuniverse );
		ad->LookupInteger( ATTR_TOTAL_CLAIM_RUN_TIME, totalclaimruntime );

		if( ! machine.Value() ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		}

		if(jobuniverse == CONDOR_UNIVERSE_STANDARD) {
			m_claimed_standard.Insert(ad);
		}

		m_collector_query_ads.Delete(ad);
	}

	// sort list with oldest standard universe jobs first
	m_claimed_standard.Sort( (SortFunctionType)ClaimRunTimeSort );

	delete pool;
	delete query;
	return(0);
}

int
AdminEvent::totalRunningJobs()
{
	ClassAd *ad;

	MyString sinful;
	MyString clientmachine;
	MyString jobid;	
	MyString ckptsrvr;

	int		imagesize;	
	int 	totalimagesize = 0;
	int     testtime = 0;

	time_t timeNow = time(NULL);

	dprintf(D_ALWAYS,"totalRunningJobs\n");


	dprintf(D_ALWAYS,"The following were assigned claimed standard U\n");
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		imagesize = 0;
		ad->LookupString( ATTR_MY_ADDRESS, sinful ); 
		ad->LookupString( ATTR_CLIENT_MACHINE, clientmachine ); 
		ad->LookupString( ATTR_JOB_ID, jobid ); 
		ad->LookupString( ATTR_CKPT_SERVER, ckptsrvr ); 
		ad->LookupInteger( ATTR_IMAGE_SIZE, imagesize ); 
		//m_claimed_standard.Delete(ad);
		if(imagesize > 0) {
			totalimagesize += (imagesize/1000);
		}
	}

	dprintf(D_ALWAYS,
		"Size of jobs from active constraint <<%d megs>>\n",totalimagesize);
	return(0);
}

int
AdminEvent::standardUProcess( )
{
	ClassAd *ad;

	MyString machine;
	MyString sinful;
	MyString name;
	MyString state;
	MyString activity;
	MyString clientmachine;
	MyString jobid;	
	MyString ckptsrvr;

	int		virtualmachineid;	
	int		jobuniverse;	
	int		jobstart;	
	int		imagesize;	
	int		lastperiodiccheckpoint;	
	int 	remotetimeNow;

	time_t timeNow = time(NULL);
	int     testtime = 0;

	dprintf(D_ALWAYS,"standardUProcess\n");

	StartdStats *ss;

	dprintf(D_ALWAYS,"The following were assigned claimed standard U\n");
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		ad->LookupString( ATTR_MACHINE, machine );
		ad->LookupString( ATTR_MY_ADDRESS, sinful ); 
		ad->LookupString( ATTR_NAME, name ); 
		HashKey		namekey(name.Value());
		ad->LookupString( ATTR_STATE, state ); 
		ad->LookupString( ATTR_ACTIVITY, activity ); 
		ad->LookupString( ATTR_CLIENT_MACHINE, clientmachine ); 
		ad->LookupString( ATTR_JOB_ID, jobid ); 
		ad->LookupString( ATTR_CKPT_SERVER, ckptsrvr ); 
		ad->LookupInteger( ATTR_JOB_START, jobstart ); 
		ad->LookupInteger( ATTR_JOB_UNIVERSE, jobuniverse ); 
		ad->LookupInteger( ATTR_IMAGE_SIZE, imagesize ); 
		ad->LookupInteger( ATTR_VIRTUAL_MACHINE_ID, virtualmachineid ); 
		ad->LookupInteger( "MonitorSelfTime", remotetimeNow ); 
		ad->LookupInteger( ATTR_LAST_PERIODIC_CHECKPOINT, lastperiodiccheckpoint ); 
		if(m_JobNodes_su.lookup(namekey,ss) < 0) {
			//dprintf(D_ALWAYS,"Must hash name %s ckptsrvr %s\n",name,ckptsrvr);
			ss = new StartdStats(name.Value(), jobuniverse, imagesize, lastperiodiccheckpoint);
			ss->virtualmachineid = virtualmachineid;
			ss->jobstart = jobstart;
			ss->remotetime = timeNow - remotetimeNow;
			testtime = timeNow + ss->remotetime; /* adjust by remote differential */
			ss->ckpttime = testtime;
			ss->ckptlength = 0;
			strcpy(ss->clientmachine, clientmachine.Value());
			strcpy(ss->state, state.Value());
			strcpy(ss->activity, activity.Value());
			strcpy(ss->myaddress, sinful.Value());
			strcpy(ss->jobid, jobid.Value());
			strcpy(ss->ckptsrvr, ckptsrvr.Value());

			m_JobNodes_su.insert(namekey,ss);
		} else {
			dprintf(D_ALWAYS,"Why is %s already in hash table??\n",name.Value());
		}

		m_claimed_standard.Delete(ad);
	}
	m_haveFullStats = true;

	return(0);
}

/*

		DISPLAY ROUTINES

*/

int
AdminEvent::standardU_benchmark_Display( )
{
	dprintf(D_ALWAYS,"standardU_benchmark_Display\n");

	StartdStats *ss;
	m_CkptTest_su.startIterations();
	//dprintf(D_ALWAYS,"<<< %d elements and %d table size >>\n",
		//m_CkptTest_su.getNumElements(), m_CkptTest_su.getTableSize());
	while(m_CkptTest_su.iterate(ss) == 1) {
		dprintf(D_ALWAYS,"+++++++++++++++++++<<standardU_benchmark_Display>>+++++++++++++++++++\n");
		dprintf(D_ALWAYS,"JobId %s Name %s SZ %d\n",ss->jobid,ss->name,ss->imagesize);
		dprintf(D_FULLDEBUG,"State %s\n",ss->state);
		dprintf(D_FULLDEBUG,"Activity %s\n",ss->activity);
		dprintf(D_FULLDEBUG,"ClientMachine %s\n",ss->clientmachine);
		dprintf(D_FULLDEBUG,"MyAddress %s\n",ss->myaddress);
		dprintf(D_FULLDEBUG,"Universe %d\n",ss->universe);
		dprintf(D_FULLDEBUG,"Jobstart %d\n",ss->jobstart);
		dprintf(D_ALWAYS,"LastCheckPoint %d\n",ss->lastcheckpoint);
		dprintf(D_FULLDEBUG,"VirtualMachineId %d\n",ss->virtualmachineid);
		dprintf(D_FULLDEBUG,"======================================\n");
		dprintf(D_FULLDEBUG,"CkptTime %d\n",ss->ckpttime);
		dprintf(D_ALWAYS,"CkptLength %d\n",ss->ckptlength);
		dprintf(D_FULLDEBUG,"CkptGroup %d\n",ss->ckptgroup);
		dprintf(D_FULLDEBUG,"CkptDone %d\n",ss->ckptdone);
		dprintf(D_ALWAYS,"CkptMegs %d CKPTSRVR %s\n",ss->ckptmegs,ss->ckptsrvr);
	}
	dprintf(D_ALWAYS,"++++++++++++++++++++++++++++++++++++++\n");
	return(0);
}

int
AdminEvent::standardUDisplay_StartdStats( )
{
	dprintf(D_FULLDEBUG,"standardUDisplay_StartdStats\n");

	StartdStats *ss;
	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		dprintf(D_FULLDEBUG,"******************<<standardUDisplay_StartdStats>>********************\n");
		dprintf(D_FULLDEBUG,"JobId %s Name %s SZ %d\n",ss->jobid,ss->name,ss->imagesize);
		dprintf(D_FULLDEBUG,"State %s\n",ss->state);
		dprintf(D_FULLDEBUG,"Activity %s\n",ss->activity);
		dprintf(D_FULLDEBUG,"ClientMachine %s\n",ss->clientmachine);
		dprintf(D_FULLDEBUG,"MyAddress %s\n",ss->myaddress);
		dprintf(D_FULLDEBUG,"Universe %d\n",ss->universe);
		dprintf(D_FULLDEBUG,"Jobstart %d\n",ss->jobstart);
		dprintf(D_FULLDEBUG,"LastCheckPoint %d\n",ss->lastcheckpoint);
		dprintf(D_FULLDEBUG,"VirtualMachineId %d\n",ss->virtualmachineid);
		dprintf(D_FULLDEBUG,"--------------------------------------\n");
		dprintf(D_FULLDEBUG,"CkptTime %d\n",ss->ckpttime);
		dprintf(D_FULLDEBUG,"CkptLength %d\n",ss->ckptlength);
		dprintf(D_FULLDEBUG,"CkptGroup %d\n",ss->ckptgroup);
		dprintf(D_FULLDEBUG,"CkptDone %d\n",ss->ckptdone);
		dprintf(D_FULLDEBUG,"CkptMegs %d CKPTSRVR %s\n",ss->ckptmegs,ss->ckptsrvr);
	}
	dprintf(D_FULLDEBUG,"**************************************\n");
	return(0);
}

int
AdminEvent::standardUDisplay()
{
	ClassAd *ad;
	MyString machine;
	MyString sinful;
	MyString name;

	dprintf(D_ALWAYS,"The following were assigned claimed standard U\n");
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		ad->LookupString( ATTR_MACHINE, machine );
		ad->LookupString( ATTR_MY_ADDRESS, sinful );
		ad->LookupString( ATTR_NAME, name );
		if( ! machine.Value() ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_FULLDEBUG, "Found <<%s>> machine matching <<%s>> Standard SORTED!!!!\n",machine.Value(),m_shutdownConstraint.Value());
			//ad->dPrint( D_ALWAYS );
			//sendCheckpoint(sinful.Value(),name.Value());
		}
	}
	return(0);
}
