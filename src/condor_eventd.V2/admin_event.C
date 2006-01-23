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

AdminEvent::AdminEvent( void ) : m_JobNodes_su(1024,hashFunction), m_CkptTest_su(256,hashFunction)
{
	dprintf(D_FULLDEBUG, "AdminEvent::constructor() \n");
	m_timeridDoShutdown = -1;
	m_intervalDoShutdown = 60;

	m_haveShutdown = false;
	m_haveFullStats = false;
	m_haveBenchStats = false;
	m_shutdownTime = 0;
	m_shutdownTarget = "";
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
	return 0;
}

int
AdminEvent::shutdownGraceful( void )
{
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
	} else {
		dprintf(D_FULLDEBUG, "AdminEvent::config(false) \n");
	}

	/* Lets check events which might be set */
	check_Shutdown(init);

	return 0;
}

/*

		Timers

*/

int 
AdminEvent::timerHandler_DoShutdown( void )
{
	DCMaster *		d;
	bool wantTcp = 	true;

	/* mark this timer as NOT active */
	m_timeridDoShutdown = -1;

	/* when we wake up we do the shutdown */
	dprintf(D_ALWAYS, "<<<Time For Shutdown!!!!--%s--!!!>>>\n",m_shutdownTarget.Value());
	d = new DCMaster(m_shutdownTarget.Value());
	dprintf(D_ALWAYS,"daemon name is %s\n",d->name());
	dprintf(D_FULLDEBUG,"call Shutdown now.....\n");
	//d->sendMasterOff(wantTcp);
	return(0);
}

int 
AdminEvent::timerHandler_Check_Ckpt_BenchM( void )
{
	// rerun last constraint
	// process ads for which ones are still standard Universe
	// look at the mark for when the last checkpoint completed
	// wait longer(reset timer) if some are not different from hash store
	// increase checkpoint size until slow down seen
	// determine number of batches based on current total size of all jobs
	return(0);
}

/*

		EVENT HANDLING METHODS

*/

int
AdminEvent::check_Shutdown( bool init )
{
	char *timeforit, *nameforit, *sizeforit, *constraintforit;
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
		 process_ShutdownConstraint( constraintforit, "new" );
		 free( constraintforit );
	}

	/* if we processed a good combo of a time and target(s) set a timer
		to do the dirty deed. At the momment we just care about how
		many Standard Universe Jobs we have. We benchmark both now and
		in enough time to handle what we just found and what we find then.
		Thus we can set a roll back timer schedule to send out batches
		of checkpoint requests. */
	
	m_claimed_standard.Rewind();
	if((ad = m_claimed_standard.Next()) ){
		 dprintf(D_ALWAYS, "AdminEvent::init() starting checkpointing list and benchmark\n");
		/* process the current standard universe jobs into a hash
			and then prepare a benchmark test. */
		/*
		if(m_haveFullStats == true) {
			m_haveFullStats = false;
			m_JobNodes_su.clear();
		}
		*/

		/* Add all the Standard Universe Ads to our hash */
		standardUProcess();

		/* show the current list of Ads */
		standardUDisplay_StartdStats();

		if(m_haveBenchStats == true) {
			m_haveBenchStats = false;
			m_CkptTest_su.clear();
		}
		setup_run_ckpt_benchmark(500);
		/**/

	}

	return(0);
}

/*

		MISC COMMANDS

*/

int
AdminEvent::sendCheckpoint( char *sinful, char *name )
{
	DCStartd *		d;

	/* when we wake up we do the shutdown */
	d = new DCStartd(name, NULL, sinful, NULL);
	dprintf(D_ALWAYS,"daemon name is %s\n",d->name());
	dprintf(D_FULLDEBUG,"call checkpointJob now.....\n");
	dprintf(D_ALWAYS, "Want to checkpoint to here ((%s)) and this vm ((%s))\n",sinful,name);
	d->checkpointJob(name);
	return(0);
}

/*
	Iterate through standard jobs until we hit megs limit. Fill out limited 
	stat records in m_CkptTest_su. Set a timer to wait for Ckpts and then see
	how long they took. Consider lower or higher quantity to find "best" quantity
	to do at once to see when network saturation occurs..... ?????
*/


int
AdminEvent::setup_run_ckpt_benchmark(int megs)
{
	int current_size = 0;
	StartdStats *ss, *tt;
	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		HashKey namekey(ss->name);	// where do we belong?
		dprintf(D_ALWAYS,"hashing %s\n",ss->name);
		if(m_CkptTest_su.lookup(namekey,tt) < 0) {
			// Good, lets add it in and accumulate the size in our total
			dprintf(D_ALWAYS,"NEW and adding %s\n",ss->name);
			tt = new StartdStats(ss->name, ss->universe, ss->imagesize, ss->lastcheckpoint);
			tt->jobstart = ss->jobstart;
			tt->virtualmachineid = ss->virtualmachineid;

			tt->ckptmegs = ss->imagesize; // how big was it at checkpoint size
			tt->ckpttime = (int) time(NULL); // 
			tt->ckptgroup = 0; // 

			strcpy(tt->clientmachine, ss->clientmachine);
			strcpy(tt->state, ss->state);
			strcpy(tt->activity, ss->activity);
			strcpy(tt->myaddress, ss->myaddress);
			strcpy(tt->jobid, ss->jobid);

			m_CkptTest_su.insert(namekey,tt);
			current_size += (tt->ckptmegs/1000);
			dprintf(D_ALWAYS,"Test Checkpoint Benchmark now at <%d> Megs\n",current_size);
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
AdminEvent::process_ShutdownConstraint( char *constraint, char *process_type )
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
	int jobuniverse = -1;
	int imagesz = -1;

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

	if( strcmp(process_type,"new") == 0) {
		// remember the constraint
		m_newshutdownConstraint = constraint;
	}

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
	m_collector_query_ads.Rewind();
	while( (ad = m_collector_query_ads.Next()) ){
		ad->LookupString( ATTR_MACHINE, &machine );
		ad->LookupString( ATTR_STATE, &state );
		ad->LookupString( ATTR_MY_ADDRESS, &sinful );
		ad->LookupInteger( ATTR_JOB_UNIVERSE, jobuniverse );
		ad->LookupInteger( ATTR_IMAGE_SIZE, imagesz );

		if( ! machine ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, "Found <<%s>> machine <<%d>> universe <<%d>> imagesz matching <<%s>>\n",machine,jobuniverse,imagesz,constraint);
		}

		if(strcmp(state,"Unclaimed") == 0){
			m_unclaimed.Insert(ad);
		} else if(jobuniverse == CONDOR_UNIVERSE_STANDARD) {
			m_claimed_standard.Insert(ad);
			dprintf(D_ALWAYS,"Standard Universe Job at %s \n",sinful);
		} else {
			m_claimed_otherUs.Insert(ad);
		}

		m_collector_query_ads.Delete(ad);

	}

	// output result
	//missedDisplay();
	//standardUDisplay();
	//otherUsDisplay();
	//unclaimedDisplay();

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
			sendCheckpoint(sinful,name);
		}
		*/
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
	StartdStats *ss;
	m_CkptTest_su.startIterations();
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
		dprintf(D_ALWAYS,"CkptGroup %d\n",ss->ckptgroup);
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
	StartdStats *ss;
	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		dprintf(D_ALWAYS,"**************************************\n");
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
		dprintf(D_ALWAYS,"--------------------------------------\n");
		dprintf(D_ALWAYS,"CkptTime %d\n",ss->ckpttime);
		dprintf(D_ALWAYS,"CkptGroup %d\n",ss->ckptgroup);
		dprintf(D_ALWAYS,"CkptMegs %d\n",ss->ckptmegs);
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
			dprintf(D_ALWAYS, "Found <<%s>> machine matching <<%s>> Standard SORTED!!!!\n",machine,m_shutdownConstraint.Value());
			ad->dPrint( D_ALWAYS );
			sendCheckpoint(sinful,name);
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

