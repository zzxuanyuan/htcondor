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
#ifndef __ADMINEVENT_H__
#define __ADMINEVENT_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "dc_master.h"
#include "MyString.h"


//#include "match_lite_resources.h"

class AdminEvent : public Service
{
  public:
	// ctor/dtor
	AdminEvent( void );
	~AdminEvent( void );
	int init( void );
	int config( bool init = false );
	int shutdownFast(void);
	int shutdownGraceful(void);

  private:
	// Command handlers
	// Timer handlers
	int timerHandler_DoShutdown( void );

	
	int 		m_timeridDoShutdown;
	unsigned 	m_intervalDoShutdown;

	// Event Handling Methods
	int check_Shutdown( bool init = false );
	int process_ShutdownTime( char *req_time );
	int process_ShutdownTarget( char *target );
	int process_ShutdownSize( char *size );
	int process_ShutdownConstraint( char *coonstraint );

	// Operation Markers
	bool 		m_haveShutdown;
	time_t 		m_shutdownTime;			/* established shutdown time */
	time_t 		m_newshutdownTime;		/* new shutdown time being considered */
	time_t 		m_shutdownDelta;		/* time till shutdown event occurs */
	MyString 	m_shutdownTarget;		/* what machine(s) */
	MyString 	m_newshutdownTarget;	/* what new machine(s) */
	MyString 	m_shutdownConstraint;	/* which machines? */
	MyString 	m_newshutdownConstraint;	/* which machines? */
	unsigned 	m_shutdownSize;			/* impact is minimized by batching requests */
	unsigned 	m_newshutdownSize;			/* impact is minimized by batching requests */

	// storage
	struct machine {
		MyString mach_target;
		struct machine * mach_nxt;
		};

};




#endif//__ADMINEVENT_H__
