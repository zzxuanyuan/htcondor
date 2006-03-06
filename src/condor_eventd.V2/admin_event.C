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

int numStartdStats = 0;
int ClaimRunTimeSort(ClassAd *job1, ClassAd *job2, void *data);

AdminEvent::AdminEvent( void ) : m_JobNodes_su(1024,hashFunction), m_CkptTest_su(256,hashFunction)
{
	m_mystate = EVENT_INIT;
	dprintf(D_ALWAYS, "AdminEvent::constructor(STATE now %d) \n",m_mystate);
	m_timeridDoShutdown = -1;
	m_intervalDoShutdown = 60;

	m_timerid_DoShutdown_States = 0;
	m_intervalCheck_DoShutdown_States = 5;
	m_intervalPeriod_DoShutdown_States = 0;

 	m_timerid_PollingVacates = -1;
    m_intervalCheck_PollingVacates = 30;
    m_intervalPeriod_PollingVacates = 30;

	m_benchmark_size = 500;
	m_benchmark_lastsize = 0;
	m_benchmark_increment = 500;

    m_NminusOne_megspersec = 0;
    m_NminusOne_size = 0;
    m_NminusOne_time = 0;

    m_N_megspersec = 0;
    m_N_size = 0;
    m_N_time = 0;

    m_NplusOne_megspersec = 0;
    m_NplusOne_size = 0;
    m_NplusOne_time = 0;

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
}

AdminEvent::~AdminEvent( void )
{
}

int
AdminEvent::init( void )
{
	// Read our Configuration parameters
	dprintf(D_FULLDEBUG, "AdminEvent::init() \n");

	config( true );

	return 0;
}

int
AdminEvent::shutdownFast( void )
{
	dprintf(D_ALWAYS,"shutdownFast\n");

	return 0;
}

int
AdminEvent::shutdownGraceful( void )
{
	dprintf(D_ALWAYS,"shutdownGraceful\n");

	return 0;
}

int
AdminEvent::config( bool init )
{
	// Initial values
	if ( init ) {
		dprintf(D_FULLDEBUG, "AdminEvent::config(true) \n");
		m_timeridDoShutdown = -1;
		m_intervalDoShutdown = 60;

		m_haveShutdown = false;
		m_shutdownTime = 0;
		m_shutdownTarget = "";
		m_mystate = EVENT_SAMPLING;
	} else {
		dprintf(D_FULLDEBUG, "AdminEvent::config(false) \n");
	}

	m_JobNodes_su.clear();

	/* Lets check events which might be set */
	check_Shutdown(init);

	return 0;
}

/*

		Timers

*/

int 
AdminEvent::timerHandler_DoShutdown_States( void )
{
	dprintf(D_ALWAYS,"timerHandler_DoShutdown_States\n");

	/**

	Handle Shutdown States.....

	Until happy call
		check_Shutdown_batch_sizes
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

	dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States(STATE = %d) \n",m_mystate);

	switch(m_mystate)
	{
		case EVENT_INIT:
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States( no work here - EVENT_INIT: )\n");
			break;
		case EVENT_SAMPLING:
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States(FetchAds) \n");
			m_JobNodes_su.clear(); /* clear our our samplin hash */
			FetchAds_ByConstraint((char *)m_shutdownConstraint.Value());
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States(check_Shutdown_batch_sizes) \n");
			check_Shutdown_batch_sizes(true);
			break;

		case EVENT_EVAL_SAMPLING:
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States(standardU_benchmark_Display) \n");
			standardU_benchmark_Display();
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States(benchmark_analysis) \n");
			benchmark_analysis();
			break;
		case EVENT_MAIN_WAIT:
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States( no work here - EVENT_MAIN_WAIT: )\n");
			break;
		case EVENT_RESAMPLE:
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States( no work here - EVENT_RESAMPLE: )\n");
			break;
		case EVENT_GO:
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States( no work here - EVENT_GO: )\n");
			break;
		//case EVENT_INIT:
			//break;
		default:
			dprintf(D_ALWAYS, "AdminEvent::timerHandler_DoShutdown_States( Why Default Case )\n");
			break;
	}
	return(0);
}

int 
AdminEvent::timerHandler_DoShutdown( void )
{
	DCMaster *		d;
	bool wantTcp = 	true;

	dprintf(D_ALWAYS,"timerHandler_DoShutdown\n");

	/* mark this timer as NOT active */
	m_timeridDoShutdown = -1;

	/* when we wake up we do the shutdown */
	dprintf(D_ALWAYS, "<<<Time For Shutdown!!!!--%s--!!!>>>\n",m_shutdownTarget.Value());
	d = new DCMaster(m_shutdownTarget.Value());
	dprintf(D_ALWAYS,"daemon name is %s\n",d->name());
	dprintf(D_ALWAYS,"call Shutdown now.....\n");
	//d->sendMasterOff(wantTcp);
	return(0);
}

int 
AdminEvent::timerHandler_Check_PollingVacates( void )
{
	ClassAdList startdAds;
	ClassAd *ad;
	int enteredcurrentstate;
	char *name;
	char *jobid;
	bool fFoundAd = false;
	bool fAllClear = true;

	//dprintf(D_ALWAYS, "timerHandler_Check_PollingVacates: start...\n");
	/*
	   We want to get the ads for each machine in the pilot size
	   group and see if it has changed from the busy state and completed 
	   its checkpoint. If so we record the time and mark it done.
	   When they are all done, we set m_stillPoolingVacates to false
	   and we then make decisions.....
	*/
	StartdStats *ss;
	m_CkptTest_su.startIterations();
	dprintf(D_ALWAYS,"<<< %d elements and %d table size >>\n",
		m_CkptTest_su.getNumElements(), m_CkptTest_su.getTableSize());
	while(m_CkptTest_su.iterate(ss) == 1) {
		// check out this job first
		//dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates: <<Name %s Size %d>>\n",
			//ss->name,ss->ckptmegs);
		//dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates: MyAddress %s\n",ss->myaddress);
		//dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates: Getting Ads to see status of vacate\n");
		if(ss->ckptdone == 0) { /* Still need to do this one.... */
			pollStartdAds( startdAds, ss->myaddress, ss->name );
			startdAds.Rewind();
			fFoundAd = false;
			while( (ad = startdAds.Next()) ){
				ad->LookupString( ATTR_NAME, &name ); 
				ad->LookupString( ATTR_JOB_ID, &jobid ); 
				//dprintf(D_ALWAYS,"pollStartdAds: got Startd ads for JOB %s on %s\n",jobid,name); 
				if( (strcmp(name, ss->name) == 0) && (strcmp(jobid, ss->jobid) == 0) && 
					(ss->ckptdone == 0) ) {
					fFoundAd = true;
					//dprintf(D_ALWAYS,"pollStartdAds: got match for JOB %s on %s\n",jobid,name);
					ad->LookupInteger( ATTR_ENTERED_CURRENT_STATE, enteredcurrentstate ); 
					//dprintf(D_ALWAYS,"pollStartdAds: requested vacate at %d and state change is at %d\n",
						//ss->ckpttime,enteredcurrentstate);
					if (ss->ckpttime > enteredcurrentstate ) {
						/* still waiting on this node to checkpoint and change state */
						dprintf(D_ALWAYS,"pollStartdAds: Still Waiting on %s for %s\n",name,ss->jobid);
						fAllClear = false;
					} else {
						/* skip checks for this job for future benchmarks */
						ss->ckptdone = 1;
						ss->ckptlength = enteredcurrentstate - ss->ckpttime;
						ss->ckptmegspersec = (float)((float)ss->ckptmegs/(float)ss->ckptlength);
						dprintf(D_ALWAYS,"pollStartdAds: checkpoint took <<%d>> for <<%s>> is done!!!\n",
							ss->ckptlength, ss->name);
					}
				}
			}
			if(!fFoundAd) {
				/* The job is done and gone.... quit waiting.....*/
				//dprintf(D_ALWAYS,"Job gone, quit waiting.....<<job %s>>!!!!!!!!!!!!\n",ss->jobid);
				time_t timeNow = time(NULL);
				ss->ckptdone = 1;
				ss->ckptlength = timeNow - ss->ckpttime;
				ss->ckptmegspersec = (float)((float)ss->ckptmegs/(float)ss->ckptlength);
				dprintf(D_ALWAYS,"pollStartdAds: job gone <<%s>: checkpoint took <<%d>> for <<%s>> is done!!!\n",
					ss->jobid,ss->ckptlength, ss->name);
			}
		} else {
			char *donestr;
			if(fAllClear) {
				donestr = "allclear";
			} else {
				donestr = "NOT-allclear";
			}
			//dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates(((DONE))): <<Name %s Size %d>>\n",
				//ss->name,ss->ckptmegs);
			//dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates <<< %s >>>\n",donestr);
		}
	}
	//dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates is done!!\n");

	if(fAllClear){
		dprintf(D_ALWAYS, "timerHandler_Check_PollingVacates: done checking ads...\n");
		daemonCore->Cancel_Timer(m_timerid_PollingVacates);
		dprintf(D_ALWAYS, "timerHandler_Check_PollingVacates: m_timerid_DoShutdown_States is %d\n"
			,m_timerid_DoShutdown_States);

		m_mystate = EVENT_EVAL_SAMPLING;
		if(m_timerid_DoShutdown_States > 0) {
			//daemonCore->Reset_Timer(m_timerid_DoShutdown_States, 
				//m_intervalCheck_DoShutdown_States);
			m_timerid_DoShutdown_States = daemonCore->Register_Timer(
				m_intervalCheck_DoShutdown_States,
				(TimerHandlercpp)&AdminEvent::timerHandler_DoShutdown_States,
				"AdminEvent::DoShutdown_States()", this );
			dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates <<< RESET TIMER ((+%d)) >>>\n"
					,m_intervalCheck_DoShutdown_States);
		} else {
			m_timerid_DoShutdown_States = daemonCore->Register_Timer(
				m_intervalCheck_DoShutdown_States,
				(TimerHandlercpp)&AdminEvent::timerHandler_DoShutdown_States,
				"AdminEvent::DoShutdown_States()", this );
			dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates <<< REGISTER TIMER ((+%d)) ID=%d >>>\n"
				,m_intervalCheck_DoShutdown_States, m_timerid_DoShutdown_States);
		}
	}
	return(0);
}

/*

		EVENT HANDLING METHODS

*/

int
AdminEvent::check_Shutdown( bool init )
{
	char *timeforit, *nameforit, *sizeforit, *constraintforit;
	dprintf(D_ALWAYS,"check_Shutdown\n");

	dprintf(D_FULLDEBUG, "Checking For Shutdown\n");
	ClassAd *ad;

	// Get EVENTD_SHUTDOWN_TIME parameter
	timeforit = param( "EVENTD_SHUTDOWN_TIME" );
	if( timeforit ) {
		 //dprintf(D_ALWAYS, "AdminEvent::init() EVENTD_SHUTDOWN_TIME is %s\n",timeforit);
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
			dprintf(D_ALWAYS, "AdminEvent: We have a repeat shutdown event. ignoring\n");
		} else {
			if((m_newshutdownTime != 0) && (m_newshutdownTarget.Length() != 0)){
				time_t timeNow = time(NULL);
				if(m_newshutdownTime > timeNow) {
					m_intervalDoShutdown = (unsigned)(m_newshutdownTime - timeNow);
					m_shutdownTime = m_newshutdownTime;
					if (( m_timeridDoShutdown < 0 ) && (m_intervalDoShutdown > 60)) {
						dprintf(D_ALWAYS, "AdminEvent: We have a shutdown event. Setting timer <<%d>> from now\n",
							m_intervalDoShutdown);
						m_timeridDoShutdown = daemonCore->Reset_Timer(
							m_timeridDoShutdown, m_intervalDoShutdown, 0);
					}
				} else {
					dprintf(D_ALWAYS, "AdminEvent::check_Shutdown: Shutdown denied as either a past time or too close to now!\n");
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
					dprintf(D_ALWAYS, "AdminEvent: We have a shutdown event. Setting timer <<%d>> from now\n",
						m_intervalDoShutdown);
					m_timeridDoShutdown = daemonCore->Register_Timer(
						m_intervalDoShutdown,
						(TimerHandlercpp)&AdminEvent::timerHandler_DoShutdown,
						"AdminEvent::DoShutdown()", this );
				}
			} else {
				dprintf(D_ALWAYS, "AdminEvent::check_Shutdown: Shutdown denied as either a past time or too close to now!\n");
			}
		}
	}

	// Get EVENTD_SHUTDOWN_MACHINES parameter
	nameforit = param( "EVENTD_SHUTDOWN_MACHINES" );
	if( nameforit ) {
		 //dprintf(D_ALWAYS, "AdminEvent::init() EVENTD_SHUTDOWN_MACHINES is %s\n",nameforit);
		 process_ShutdownTarget( nameforit );
		 free( nameforit );
	}

	// Get EVENTD_SHUTDOWN_SIZE parameter
	sizeforit = param( "EVENTD_SHUTDOWN_SIZE" );
	if( sizeforit ) {
		 dprintf(D_ALWAYS, "AdminEvent::init() EVENTD_SHUTDOWN_SIZE is %s\n",sizeforit);
		 process_ShutdownSize( sizeforit );
		 free( sizeforit );
	}

	// Get EVENTD_SHUTDOWN_CONSTRAINT parameter
	constraintforit = param( "EVENTD_SHUTDOWN_CONSTRAINT" );
	if( constraintforit ) {
		 dprintf(D_ALWAYS, "AdminEvent::init() EVENTD_SHUTDOWN_CONSTRAINT is %s\n",constraintforit);
		 process_ShutdownConstraint( constraintforit);
		 free( constraintforit );
	}

	if(m_timerid_DoShutdown_States > 0) {
		//daemonCore->Reset_Timer(m_timerid_DoShutdown_States, 
			//m_intervalCheck_DoShutdown_States);
		m_timerid_DoShutdown_States = daemonCore->Register_Timer(
			m_intervalCheck_DoShutdown_States,
			(TimerHandlercpp)&AdminEvent::timerHandler_DoShutdown_States,
			"AdminEvent::DoShutdown_States()", this );
			dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates <<< REGISTER TIMER ((+%d)) ID=%d >>>\n"
				,m_intervalCheck_DoShutdown_States, m_timerid_DoShutdown_States);
	} else {
		m_timerid_DoShutdown_States = daemonCore->Register_Timer(
			m_intervalCheck_DoShutdown_States,
			(TimerHandlercpp)&AdminEvent::timerHandler_DoShutdown_States,
			"AdminEvent::DoShutdown_States()", this );
			dprintf(D_ALWAYS,"timerHandler_Check_PollingVacates <<< REGISTER TIMER ((+%d)) ID=%d >>>\n"
				,m_intervalCheck_DoShutdown_States, m_timerid_DoShutdown_States);
	}
}

int
AdminEvent::check_Shutdown_batch_sizes( bool init )
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
	
	dprintf(D_ALWAYS,"check_Shutdown_batch_sizes\n");

	m_claimed_standard.Rewind();
	if((ad = m_claimed_standard.Next()) ){
		 dprintf(D_ALWAYS, "AdminEvent::check_Shutdown_batch_sizes() starting checkpointing list and benchmark\n");
		/* 
			process the current standard universe jobs into a hash
			and then prepare a benchmark test. 
		*/

		/* Add all the Standard Universe Ads to our hash */
		standardUProcess();

		/* show the current list of Ads */
		standardUDisplay_StartdStats();

		/* always clear out last checkpoint size sampling */
		m_CkptTest_su.clear();

		setup_run_ckpt_benchmark();
		m_intervalCheck_PollingVacates = 60;
		m_intervalPeriod_PollingVacates = 60;

		dprintf(D_ALWAYS, "setting check polling vacate times for +%d\n",
			m_intervalCheck_PollingVacates);
		m_timerid_PollingVacates = daemonCore->Register_Timer(
			m_intervalCheck_PollingVacates,
			m_intervalPeriod_PollingVacates,
			(TimerHandlercpp)&AdminEvent::timerHandler_Check_PollingVacates,
			"AdminEvent::PollingVacates()", this );
		dprintf(D_ALWAYS, "timer ID is %d\n",
			m_timerid_PollingVacates);
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
AdminEvent::pollStartdAds( ClassAdList &adsList, char *sinful, char *name )
{
	DCStartd *		d;

	//d = new DCStartd(name, NULL, sinful, NULL);
	d = new DCStartd(name, NULL);
	dprintf(D_FULLDEBUG,"daemon name is %s\n",d->name());
	dprintf(D_FULLDEBUG,"call getAds now.....\n");
	dprintf(D_FULLDEBUG, "Want to get ads from  here ((%s)) and this vm ((%s))\n",sinful,name);
	d->getAds(adsList, name);
	return(0);
}

int
AdminEvent::sendCheckpoint( char *sinful, char *name )
{
	DCStartd *		d;

	/* when we wake up we do the shutdown */
	d = new DCStartd(name, NULL, sinful, NULL);
	dprintf(D_FULLDEBUG,"daemon name is %s\n",d->name());
	dprintf(D_FULLDEBUG,"call checkpointJob now.....\n");
	dprintf(D_FULLDEBUG, "Want to checkpoint to here ((%s)) and this vm ((%s))\n",sinful,name);
	d->checkpointJob(name);
	return(0);
}

int
AdminEvent::sendVacateClaim( char *sinful, char *name )
{
	DCStartd *		d;
	ClassAd reply;
	int timeout = -1;
	bool fRes = true;

	/* when we wake up we do the shutdown */
	d = new DCStartd(name, NULL, sinful, NULL);
	dprintf(D_ALWAYS,"daemon name is %s\n",d->name());
	dprintf(D_ALWAYS,"call vacateClaim now.....\n");
	dprintf(D_ALWAYS, "Want to vacate claim  here ((%s)) and this vm ((%s))\n",sinful,name);
	//d->vacateClaim(name);
	fRes = d->releaseClaim(VACATE_GRACEFUL,&reply,timeout);
	if(!fRes) {
		dprintf(D_ALWAYS,"d->releaseClaim(VACATE_GRACEFUL,&reply,timeout) FAILED\n");
	}

	return(0);
}

/*
	Iterate through standard jobs until we hit megs limit. Fill out limited 
	stat records in m_CkptTest_su. Set a timer to wait for Ckpts and then see
	how long they took. Consider lower or higher quantity to find "best" quantity
	to do at once to see when network saturation occurs..... ?????
*/

int
AdminEvent::setup_run_ckpt_benchmark()
{
	int megs = 0;
	ClassAd *ad;
	int current_size = 0;
	char *name;
	StartdStats *ss, *tt;
	dprintf(D_ALWAYS,"setup_run_ckpt_benchmark\n");

	if(m_benchmark_lastsize == 0) {
		/* first check */
		megs = m_benchmark_lastsize = m_benchmark_size;
	} else {
		megs = m_benchmark_lastsize = (m_benchmark_lastsize + m_benchmark_increment);
	}

	m_JobNodes_su.startIterations();
	dprintf(D_ALWAYS,"<<< %d elements and %d table size >>\n",
		m_CkptTest_su.getNumElements(), m_CkptTest_su.getTableSize());
	while(m_JobNodes_su.iterate(ss) == 1) {
		HashKey namekey(ss->name);	// where do we belong?
		dprintf(D_ALWAYS,"hashing %s imagesize %d<<%d>> \n",ss->name,ss->imagesize,(ss->imagesize/1000));
		if(m_CkptTest_su.lookup(namekey,tt) < 0) {
			// Good, lets add it in and accumulate the size in our total
			dprintf(D_ALWAYS,"NEW and adding %s\n",ss->name);
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

			if(tt->ckptmegs > 0) {
				m_CkptTest_su.insert(namekey,tt);
				dprintf(D_FULLDEBUG,"Sending vacate command to  *****<%s:%s>*****\n",
					ss->myaddress, ss->name);
				//sendCheckpoint(ss->myaddress,ss->name);
				sendVacateClaim(ss->myaddress,ss->name);
	
				//pollStartdAds( m_fromStartd, tt->myaddress, tt->name );
				//m_fromStartd.Rewind();
				//while( (ad = m_fromStartd.Next()) ){
					//ad->LookupString( ATTR_NAME, &name ); //**
					//dprintf(D_ALWAYS,"pollStartdAds: got Startd ads for %s\n",name);
				//}
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
	standardU_benchmark_Display();
	return(0);
}

int
AdminEvent::benchmark_analysis()
{
	StartdStats *ss;
	float megspersec_accumulator = 0;
	float megspersec_result = 0;
	int	megspersec_votes = 0;
	int megs = 0;
	int shortest = 0;
	int longest = 0;
	bool f_continue = true;

	dprintf(D_ALWAYS,"benchmark_analysis\n");

	m_CkptTest_su.startIterations();
	dprintf(D_ALWAYS,"<<< %d elements and %d table size >>\n",
		m_CkptTest_su.getNumElements(), m_CkptTest_su.getTableSize());
	while(m_CkptTest_su.iterate(ss) == 1) {
		dprintf(D_ALWAYS,"++++++++++++++++++++++++++++++++++++++\n");
		dprintf(D_ALWAYS,"JobId %s\n",ss->jobid);
		dprintf(D_ALWAYS,"Name %s\n",ss->name);
		dprintf(D_FULLDEBUG,"State %s\n",ss->state);
		dprintf(D_FULLDEBUG,"Activity %s\n",ss->activity);
		dprintf(D_FULLDEBUG,"ClientMachine %s\n",ss->clientmachine);
		dprintf(D_ALWAYS,"MyAddress %s\n",ss->myaddress);
		dprintf(D_ALWAYS,"ImageSize %d\n",ss->imagesize);
		dprintf(D_FULLDEBUG,"Universe %d\n",ss->universe);
		dprintf(D_FULLDEBUG,"Jobstart %d\n",ss->jobstart);
		dprintf(D_FULLDEBUG,"LastCheckPoint %d\n",ss->lastcheckpoint);
		dprintf(D_FULLDEBUG,"VirtualMachineId %d\n",ss->virtualmachineid);
		dprintf(D_ALWAYS,"======================================\n");
		dprintf(D_FULLDEBUG,"CkptTime %d\n",ss->ckpttime);
		dprintf(D_ALWAYS,"CkptLength %d\n",ss->ckptlength);
		dprintf(D_FULLDEBUG,"CkptGroup %d\n",ss->ckptgroup);
		dprintf(D_FULLDEBUG,"CkptDone %d\n",ss->ckptdone);
		dprintf(D_ALWAYS,"CkptMegs %d\n",ss->ckptmegs);
		dprintf(D_ALWAYS,"CkptMegspersec %f\n",ss->ckptmegspersec);
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
		//delete ss;
	}
	megspersec_result = ((float)megspersec_accumulator/(float)megspersec_votes);
	f_continue = benchmark_store_results(megspersec_result, megs, longest);
	benchmark_show_results();
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
			(TimerHandlercpp)&AdminEvent::timerHandler_DoShutdown_States,
			"AdminEvent::DoShutdown_States()", this );
	}

	dprintf(D_ALWAYS," average save for %d size is %f\n",megs,megspersec_result);
	return(0);
}

bool
AdminEvent::benchmark_store_results(float  megspersec, int totmegs, int tottime)
{
	if(m_NminusOne_megspersec == 0) {
		m_NminusOne_megspersec = megspersec;
		m_NminusOne_size = totmegs;
		m_NminusOne_time = tottime;
	} else if(m_N_megspersec == 0) {
		m_N_megspersec = megspersec;
		m_N_size = totmegs;
		m_N_time = tottime;
	} else if(m_NplusOne_megspersec == 0) {
		m_NplusOne_megspersec = megspersec;
		m_NplusOne_size = totmegs;
		m_NplusOne_time = tottime;
	}
	else { /* shift times down */
		m_NminusOne_megspersec = m_N_megspersec;
		m_N_megspersec = m_NplusOne_megspersec;
		m_NplusOne_megspersec = megspersec;
		m_NminusOne_time = m_N_time;
		m_N_time = m_NplusOne_time;
		m_NplusOne_time = tottime;
		m_NminusOne_size = m_N_size;
		m_N_size = m_NplusOne_size;
		m_NplusOne_size = totmegs;
	}
	return(true);	/* no appreciable length increase - 
						go bigger on vacate size */
}

int
AdminEvent::benchmark_show_results()
{
	dprintf(D_ALWAYS,"T minus1: Rate %f Megs %d Time %d\n",
		m_NminusOne_megspersec, m_NminusOne_size, m_NminusOne_time);
	dprintf(D_ALWAYS,"T       : Rate %f Megs %d Time %d\n",
		m_N_megspersec, m_N_size, m_N_time);
	dprintf(D_ALWAYS,"T  plus1: Rate %f Megs %d Time %d\n",
		m_NplusOne_megspersec, m_NplusOne_size, m_NplusOne_time);
}

/*

		PROCESSING ROUTINES

*/

int
AdminEvent::process_ShutdownTime( char *req_time )
{
	dprintf(D_ALWAYS, "Processing Shutdown Time String<<%s>>\n",req_time);
	// Crunch time into time_t format via str to tm
	struct tm tm;
	struct tm *tmnow;
	char *res;
	time_t timeNow = time(NULL);

	// make a tm with info for now and reset
	// secs, day and hour from the next scan of the req_time`
	tmnow = localtime(&timeNow);

	// find secs, hour and minutes
	res = strptime(req_time,"%H:%M:%S",&tm);
	dprintf(D_ALWAYS, "Processing Shutdown Time seconds <%d> minutes <%d> hours <%d>\n",
		tm.tm_sec,tm.tm_min,tm.tm_hour);
	if(res != NULL) {
		dprintf(D_FULLDEBUG, "Processing Shutdown Time String<<LEFTOVERS--%s-->>\n",res);
		//return(-1);
	}

	// Check TM values

	// Get request into this days structure
	tmnow->tm_sec = tm.tm_sec;
	tmnow->tm_min = tm.tm_min;
	tmnow->tm_hour = tm.tm_hour;

	// Get our time_t value
	m_newshutdownTime = mktime(tmnow);
	dprintf(D_FULLDEBUG, "Time now is %ld and shutdown is %ld\n",
		timeNow,m_shutdownTime);
	
	return(0);
}

int
AdminEvent::process_ShutdownTarget( char *target )
{
	dprintf(D_ALWAYS, "Processing Shutdown target String<<%s>>\n",target);
	// remember the target
	m_newshutdownTarget = target;
	return(0);
}

int
AdminEvent::process_ShutdownSize( char *size )
{
	dprintf(D_ALWAYS, "Processing Shutdown Size String<<%s>>\n",size);
	// remember the size
	m_newshutdownSize = atoi(size);
	return(0);
}

int
AdminEvent::process_ShutdownConstraint( char *constraint )
{
	// remember the constraint
	dprintf(D_ALWAYS,"process_ShutdownConstraint\n");

	m_shutdownConstraint = constraint;
}

int
AdminEvent::FetchAds_ByConstraint( char *constraint )
{
	bool first = true;
	CondorError errstack;
	CondorQuery *query;
    QueryResult q;
	ClassAd *ad;
	DCCollector* pool = NULL;
	AdTypes     type    = (AdTypes) -1;
	char* tmp = NULL;
	const char* host = NULL;

	char *machine = NULL;
	char *state = NULL;
	char *sinful = NULL;
	char *name = NULL;
	char *remoteuser = NULL;
	int jobuniverse = -1;
	int imagesz = -1;
	int totalclaimruntime = -1;

	char *machine2 = NULL;
	char *state2 = NULL;
	char *sinful2 = NULL;
	char *name2 = NULL;
	char *remoteuser2 = NULL;
	int jobuniverse2 = -1;
	int imagesz2 = -1;
	int totalclaimruntime2 = -1;

	dprintf(D_ALWAYS,"FetchAds_ByConstraint\n");

	pool = new DCCollector( "" );

	if( !pool->addr() ) {
		dprintf (D_ALWAYS, "Getting Collector Object Error:  %s\n",pool->error());
		return(1);
	}

	// fetch the query

	// we are looking for starter ads
	type = STARTD_AD;
	//type = MASTER_AD;

	// instantiate query object
	if( !(query = new CondorQuery (type))) {
		dprintf (D_ALWAYS, "Getting Collector Query Object Error:  Out of memory\n");
		return(1);
	}

	dprintf(D_ALWAYS, "Processing Shutdown constraint String<<%s>>\n",constraint);

	query->addORConstraint( constraint );

	q = query->fetchAds( m_collector_query_ads, pool->addr(), &errstack);

	if( q != Q_OK ){
		dprintf(D_ALWAYS, "Trouble fetching Ads with<<%s>><<%d>>\n",constraint,q);
		return(1);
	}

	if( m_collector_query_ads.Length() <= 0 ){
		dprintf(D_ALWAYS, "Found no ClassAds matching <<%s>> <<%d results>>\n",constraint,m_collector_query_ads.Length());
	} else {
		dprintf(D_ALWAYS, "Found <<%d>> ClassAds matching <<%s>>\n",m_collector_query_ads.Length(),constraint);
	}

	// output result
	// we always fill the sorted class ad lists with the result of the query
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		m_claimed_standard.Delete(ad);
	}
	m_collector_query_ads.Rewind();
	while( (ad = m_collector_query_ads.Next()) ){
		ad->LookupString( ATTR_MACHINE, &machine );
		ad->LookupString( ATTR_STATE, &state );
		ad->LookupString( ATTR_MY_ADDRESS, &sinful );
		ad->LookupString( ATTR_REMOTE_USER, &remoteuser );
		ad->LookupInteger( ATTR_JOB_UNIVERSE, jobuniverse );
		ad->LookupInteger( ATTR_TOTAL_CLAIM_RUN_TIME, totalclaimruntime );

		if( ! machine ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, "Found <<%s>> machine <<%d>> universe <<%d>> imagesz matching <<%s>>\n",machine,jobuniverse,imagesz,constraint);
			dprintf(D_ALWAYS, "Found <<%s>> remoteuser <<%d>> totalclaimruntime \n",remoteuser,totalclaimruntime);
		}

		if(strcmp(state,"Unclaimed") == 0){
			dprintf(D_FULLDEBUG,"Unclaimed ignored %s \n",sinful);
			//m_unclaimed.Insert(ad);
		} else if(jobuniverse == CONDOR_UNIVERSE_STANDARD) {
			m_claimed_standard.Insert(ad);
			dprintf(D_ALWAYS,"Standard Universe Job at %s \n",sinful);
		} else {
			dprintf(D_FULLDEBUG,"Other Univereses ignored %s \n",sinful);
			//m_claimed_otherUs.Insert(ad);
		}

		m_collector_query_ads.Delete(ad);

	}

	// sort list with oldest standard universe jobs first
	m_claimed_standard.Sort( (SortFunctionType)ClaimRunTimeSort );


	/*
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		ad->LookupString( ATTR_MACHINE, &machine2 );
		ad->LookupString( ATTR_STATE, &state2 );
		ad->LookupString( ATTR_MY_ADDRESS, &sinful2 );
		ad->LookupString( ATTR_REMOTE_USER, &remoteuser2 );
		ad->LookupInteger( ATTR_JOB_UNIVERSE, jobuniverse2 );
		ad->LookupInteger( ATTR_TOTAL_CLAIM_RUN_TIME, totalclaimruntime2 );

		if( ! machine2 ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, "SORTED <<%s>> machine <<%d>> universe <<%d>> imagesz matching <<%s>>\n",machine2,jobuniverse2,imagesz2,constraint);
			dprintf(D_ALWAYS, "SORTED <<%s>> remoteuser <<%d>> totalclaimruntime \n",remoteuser2,totalclaimruntime2);
		}
	}

	*/

	// output result
	// standardUDisplay();

	delete pool;
	return(0);
}

int
AdminEvent::standardUProcess( )
{
	ClassAd *ad;

	char 	*machine;
	char 	*sinful;
	char 	*name;
	char 	*state;
	char 	*activity;
	char 	*clientmachine;
	char	*jobid;	

	int		virtualmachineid;	
	int		jobuniverse;	
	int		jobstart;	
	int		imagesize;	
	int		lastperiodiccheckpoint;	

	time_t timeNow = time(NULL);
	int     testtime = (int)timeNow;

	dprintf(D_ALWAYS,"standardUProcess\n");

	StartdStats *ss;

	dprintf(D_ALWAYS,"The following were assigned claimed standard U\n");
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		ad->LookupString( ATTR_MACHINE, &machine );
		ad->LookupString( ATTR_MY_ADDRESS, &sinful ); //**
		ad->LookupString( ATTR_NAME, &name ); //**
		HashKey		namekey(name);
		ad->LookupString( ATTR_STATE, &state ); //**
		ad->LookupString( ATTR_ACTIVITY, &activity ); //**
		ad->LookupString( ATTR_CLIENT_MACHINE, &clientmachine ); //**
		ad->LookupString( ATTR_JOB_ID, &jobid ); //**
		ad->LookupInteger( ATTR_JOB_START, jobstart ); //**
		ad->LookupInteger( ATTR_JOB_UNIVERSE, jobuniverse ); //**
		ad->LookupInteger( ATTR_IMAGE_SIZE, imagesize ); //**
		ad->LookupInteger( ATTR_VIRTUAL_MACHINE_ID, virtualmachineid ); //**
		ad->LookupInteger( ATTR_LAST_PERIODIC_CHECKPOINT, lastperiodiccheckpoint ); //**
		if(m_JobNodes_su.lookup(namekey,ss) < 0) {
			dprintf(D_ALWAYS,"Must hash name %s\n",name);
			ss = new StartdStats(name, jobuniverse, imagesize, lastperiodiccheckpoint);
			ss->virtualmachineid = virtualmachineid;
			ss->jobstart = jobstart;
			ss->ckpttime = testtime;
			ss->ckptlength = 0;
			strcpy(ss->clientmachine, clientmachine);
			strcpy(ss->state, state);
			strcpy(ss->activity, activity);
			strcpy(ss->myaddress, sinful);
			strcpy(ss->jobid, jobid);

			m_JobNodes_su.insert(namekey,ss);
		} else {
			dprintf(D_ALWAYS,"Why is %s already in hash table??\n",name);
		}

		/*
		if( ! machine ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, "Found <<%s>> machine matching <<%s>> Standard SORTED!!!!\n",machine,m_shutdownConstraint.Value());
			ad->dPrint( D_ALWAYS );
			//sendCheckpoint(sinful,name);
		}
		*/
	}
	m_haveFullStats = true;

	return(0);
}

int
AdminEvent::standardUProcess_ckpt_times( )
{
	ClassAd *ad;

	char 	*name;

	int		imagesize;	
	int		lastperiodiccheckpoint;	
	int		timetockpt;

	time_t timeNow = time(NULL);
	int     testtime = (int)timeNow;

	dprintf(D_ALWAYS,"standardUProcess_ckpt_times\n");

	StartdStats *ss;

	dprintf(D_ALWAYS,"The following were assigned claimed standard U\n");
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		/* lets find the name and see if we are benchmarking this job */
		ad->LookupString( ATTR_NAME, &name ); //**
		HashKey		namekey(name);
		if(m_CkptTest_su.lookup(namekey,ss) < 0) {
			dprintf(D_ALWAYS,"Not benchmarking checkpointing on this job%s\n",name);
		} else {
			dprintf(D_ALWAYS,"Found %s as Benchmark Job\n",name);
			ad->LookupInteger( ATTR_LAST_PERIODIC_CHECKPOINT, lastperiodiccheckpoint ); //**
			dprintf(D_ALWAYS,"Checkpoint Now *****<<%d>>*****\n",lastperiodiccheckpoint);
			timetockpt = lastperiodiccheckpoint - ss->ckpttime;
			ss->ckptlength = timetockpt;
		}

	}
	//m_haveFullStats = true;

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
	dprintf(D_ALWAYS,"<<< %d elements and %d table size >>\n",
		m_CkptTest_su.getNumElements(), m_CkptTest_su.getTableSize());
	while(m_CkptTest_su.iterate(ss) == 1) {
		dprintf(D_ALWAYS,"++++++++++++++++++++++++++++++++++++++\n");
		dprintf(D_ALWAYS,"JobId %s\n",ss->jobid);
		dprintf(D_ALWAYS,"Name %s\n",ss->name);
		dprintf(D_ALWAYS,"State %s\n",ss->state);
		dprintf(D_ALWAYS,"Activity %s\n",ss->activity);
		dprintf(D_ALWAYS,"ClientMachine %s\n",ss->clientmachine);
		dprintf(D_ALWAYS,"MyAddress %s\n",ss->myaddress);
		dprintf(D_ALWAYS,"ImageSize %d\n",ss->imagesize);
		dprintf(D_ALWAYS,"Universe %d\n",ss->universe);
		dprintf(D_ALWAYS,"Jobstart %d\n",ss->jobstart);
		dprintf(D_ALWAYS,"LastCheckPoint %d\n",ss->lastcheckpoint);
		dprintf(D_ALWAYS,"VirtualMachineId %d\n",ss->virtualmachineid);
		dprintf(D_ALWAYS,"======================================\n");
		dprintf(D_ALWAYS,"CkptTime %d\n",ss->ckpttime);
		dprintf(D_ALWAYS,"CkptLength %d\n",ss->ckptlength);
		dprintf(D_ALWAYS,"CkptGroup %d\n",ss->ckptgroup);
		dprintf(D_ALWAYS,"CkptDone %d\n",ss->ckptdone);
		dprintf(D_ALWAYS,"CkptMegs %d\n",ss->ckptmegs);
		//delete ss;
	}
	dprintf(D_ALWAYS,"++++++++++++++++++++++++++++++++++++++\n");
	//m_CkptTest_su.clear(); empties..... it it seems
	return(0);
}

int
AdminEvent::standardUDisplay_StartdStats( )
{
	dprintf(D_ALWAYS,"standardUDisplay_StartdStats\n");

	StartdStats *ss;
	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		dprintf(D_FULLDEBUG,"**************************************\n");
		dprintf(D_FULLDEBUG,"JobId %s\n",ss->jobid);
		dprintf(D_FULLDEBUG,"Name %s\n",ss->name);
		dprintf(D_FULLDEBUG,"State %s\n",ss->state);
		dprintf(D_FULLDEBUG,"Activity %s\n",ss->activity);
		dprintf(D_FULLDEBUG,"ClientMachine %s\n",ss->clientmachine);
		dprintf(D_FULLDEBUG,"MyAddress %s\n",ss->myaddress);
		dprintf(D_FULLDEBUG,"ImageSize %d\n",ss->imagesize);
		dprintf(D_FULLDEBUG,"Universe %d\n",ss->universe);
		dprintf(D_FULLDEBUG,"Jobstart %d\n",ss->jobstart);
		dprintf(D_FULLDEBUG,"LastCheckPoint %d\n",ss->lastcheckpoint);
		dprintf(D_FULLDEBUG,"VirtualMachineId %d\n",ss->virtualmachineid);
		dprintf(D_FULLDEBUG,"--------------------------------------\n");
		dprintf(D_FULLDEBUG,"CkptTime %d\n",ss->ckpttime);
		dprintf(D_FULLDEBUG,"CkptLength %d\n",ss->ckptlength);
		dprintf(D_FULLDEBUG,"CkptGroup %d\n",ss->ckptgroup);
		dprintf(D_FULLDEBUG,"CkptDone %d\n",ss->ckptdone);
		dprintf(D_FULLDEBUG,"CkptMegs %d\n",ss->ckptmegs);
	}
	dprintf(D_ALWAYS,"**************************************\n");
	//m_JobNodes_su.clear(); empties..... it it seems
	return(0);
}

int
AdminEvent::missedDisplay( )
{
	ClassAd *ad;
	char *machine;

	dprintf(D_ALWAYS,"The following were NOT assigned sublists\n");
	m_collector_query_ads.Rewind();
	while( (ad = m_collector_query_ads.Next()) ){
		ad->LookupString( ATTR_MACHINE, &machine );
		if( ! machine ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, "Found <<%s>> machine matching <<%s>> NOT SORTED!!!!\n",machine,m_shutdownConstraint.Value());
			ad->dPrint( D_ALWAYS );
		}
	}
	return(0);
}

int
AdminEvent::standardUDisplay()
{
	ClassAd *ad;
	char *machine;
	char *sinful;
	char *name;

	dprintf(D_ALWAYS,"The following were assigned claimed standard U\n");
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		ad->LookupString( ATTR_MACHINE, &machine );
		ad->LookupString( ATTR_MY_ADDRESS, &sinful );
		ad->LookupString( ATTR_NAME, &name );
		if( ! machine ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_FULLDEBUG, "Found <<%s>> machine matching <<%s>> Standard SORTED!!!!\n",machine,m_shutdownConstraint.Value());
			//ad->dPrint( D_ALWAYS );
			//sendCheckpoint(sinful,name);
		}
	}
	return(0);
}

int
AdminEvent::otherUsDisplay()
{
	ClassAd *ad;
	char *machine;

	dprintf(D_ALWAYS,"The following were assigned claimed vanilla U\n");
	m_claimed_otherUs.Rewind();
	while( (ad = m_claimed_otherUs.Next()) ){
		ad->LookupString( ATTR_MACHINE, &machine );
		if( ! machine ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, "Found <<%s>> machine matching <<%s>> Vanilla!!!!\n",machine,m_shutdownConstraint.Value());
		}
	}
	return(0);
}

int
AdminEvent::unclaimedDisplay()
{
	ClassAd *ad;
	char *machine;

	dprintf(D_ALWAYS,"The following were assigned Unclaimed U\n");
	m_unclaimed.Rewind();
	while( (ad = m_unclaimed.Next()) ){
		ad->LookupString( ATTR_MACHINE, &machine );
		if( ! machine ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, "Found <<%s>> machine matching <<%s>> Unclaimed!!!!\n",machine,m_shutdownConstraint.Value());
		}
	}
	return(0);
}

