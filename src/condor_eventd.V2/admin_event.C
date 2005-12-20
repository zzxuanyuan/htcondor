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
#include "admin_event.h"



AdminEvent::AdminEvent( void )
{
	dprintf(D_FULLDEBUG, "AdminEvent::constructor() \n");
	m_timeridDoShutdown = -1;
	m_intervalDoShutdown = 60;

	m_haveShutdown = false;
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


// Timers

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
	d->sendMasterOff(wantTcp);
	return(0);
}


// Event Handling Methods

int
AdminEvent::check_Shutdown( bool init )
{
	char *timeforit, *nameforit, *sizeforit, *constraintforit;
	dprintf(D_FULLDEBUG, "Checking For Shutdown\n");

	// Get EVENTD_SHUTDOWN_TIME parameter
	timeforit = param( "EVENTD_SHUTDOWN_TIME" );
	if( timeforit ) {
		 //dprintf(D_ALWAYS, "AdminEvent::init() EVENTD_SHUTDOWN_TIME is %s\n",timeforit);
		 process_ShutdownTime( timeforit );
		 free( timeforit );
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
		 process_ShutdownConstraint( constraintforit );
		 free( constraintforit );
	}

	/* if we processed a good combo of a time and target(s) set a timer
		to do the dirty deed. */

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

	return(0);
}

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
	CondorError errstack;
	CondorQuery *query;
    QueryResult q;
	ClassAdList result;
	ClassAd *ad;
	DCCollector* pool = NULL;
	AdTypes     type    = (AdTypes) -1;
	char* tmp = NULL;
	const char* host = NULL;

	pool = new DCCollector( "" );

	if( !pool->addr() ) {
		dprintf (D_ALWAYS, "Getting Collector Object Error:  %s\n",pool->error());
		return(1);
	}

	// fetch the query

	// we are looking for starter ads
	type = STARTD_AD;

	// instantiate query object
	if( !(query = new CondorQuery (type))) {
		dprintf (D_ALWAYS, "Getting Collector Query Object Error:  Out of memory\n");
		return(1);
	}

	dprintf(D_ALWAYS, "Processing Shutdown constraint String<<%s>>\n",constraint);
	// remember the constraint
	m_newshutdownConstraint = constraint;

	query->addORConstraint( m_newshutdownConstraint.Value());

	q = query->fetchAds( result, pool->addr(), &errstack);

	if( q != Q_OK ){
		dprintf(D_ALWAYS, "Trouble fetching Ads with<<%s>><<%d>>\n",constraint,q);
		return(1);
	}

	if( result.Length() <= 0 ){
		dprintf(D_ALWAYS, "Found no ClassAds matching <<%s>>\n",constraint);
	} else {
		dprintf(D_ALWAYS, "Found <<%d>> ClassAds matching <<%s>>\n",result.Length(),constraint);
	}

	// output result
	result.Rewind();
	while( ad = result.Next() ){
		ad->LookupString( ATTR_MACHINE, &tmp );
		if( ! tmp ) {
			// weird, malformed ad.
			// should we print a warning?
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, "Found <<%s>> machine matching <<%s>>\n",tmp,constraint);
			//host = get_host_part( tmp );
			//dprintf(D_ALWAYS, "Found <<%s>> machine matching <<%s>>\n",host,constraint);
		}
	}
	//prettyPrint (result, &totals);

	return(0);
}
