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
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "self_draining_queue.h"


SelfDrainingQueue::SelfDrainingQueue( const char* queue_name, int frequency )
{
	if( queue_name ) {
		name = strdup( queue_name );
	} else {
		name = strdup( "[unnamed]" );
	}
	handler_fn = NULL;
	handlercpp_fn = NULL;
	service_ptr = NULL;

	this->frequency = frequency;
	tid = -1;
}


SelfDrainingQueue::~SelfDrainingQueue()
{
	if( name ) {
		free( name );
	}
	if( tid != -1 ) {
		daemonCore->Cancel_Timer( tid );
	}
}


bool
SelfDrainingQueue::registerHandler( ServiceDataHandler handler_fn )
{
	if( handlercpp_fn ) {
		handlercpp_fn = NULL;
	}
	if( service_ptr ) {
		service_ptr = NULL;
	}
	this->handler_fn = handler_fn;
	return true;
}


bool
SelfDrainingQueue::registerHandlercpp( ServiceDataHandlercpp 
									   handlercpp_fn, 
									   Service* service_ptr )
{
	if( handler_fn ) {
		handler_fn = NULL;
	}
	this->handlercpp_fn = handlercpp_fn;
	this->service_ptr = service_ptr;
	return true;
}


bool
SelfDrainingQueue::enqueue( ServiceData* data )
{
	queue.enqueue(data);
	dprintf( D_FULLDEBUG,
			 "Added data to SelfDrainingQueue %s, now has %d element(s)\n",
			 name, queue.Length() );
	registerTimer();
	return true;
}


int
SelfDrainingQueue::timerHandler( void )
{
	dprintf( D_FULLDEBUG,
			 "Inside SelfDrainingQueue::timerHandler() for %s\n", name );
	tid = -1;
	ServiceData* d;
	if( queue.IsEmpty() ) {
		dprintf( D_FULLDEBUG,
				 "SelfDrainingQueue %s is empty, timerHandler() returning\n",
				 name );
		return TRUE;
	}
	queue.dequeue(d);
	if( handler_fn ) {
		handler_fn( d );
	} else if( handlercpp_fn && service_ptr ) {
		(service_ptr->*handlercpp_fn)( d );
	}

	if( queue.IsEmpty() ) {
		dprintf( D_FULLDEBUG,
				 "SelfDrainingQueue %s is empty, not resetting timer\n",
				 name );
	} else {
			// if there's anything left in the queue, reset our timer
		dprintf( D_FULLDEBUG,
				 "SelfDrainingQueue %s still has %d element(s), "
				 "resetting timer\n", name, queue.Length() );
		registerTimer();
	}
	return TRUE;
}


bool
SelfDrainingQueue::registerTimer( void )
{
	if( !handler_fn && !(service_ptr && handlercpp_fn) ) {
		EXCEPT( "Programmer error: trying to register timer for "
				"SelfDrainingQueue %s without having a handler function", 
				name );
	}
	if( tid != -1 ) {
		return true;
	}
	tid = daemonCore->
		Register_Timer( frequency, 
						(TimerHandlercpp)&SelfDrainingQueue::timerHandler,
						"SelfDrainingQueue::timerHandler", this );
    if( tid == -1 ) {
            // Error registering timer!
        EXCEPT( "Can't register daemonCore timer for SelfDrainingQueue %s",
				name );
    }
	dprintf( D_FULLDEBUG,
			 "Registered timer from SelfDrainingQueue %s (id: %d)\n",
			 name, tid );

    return true;
}
