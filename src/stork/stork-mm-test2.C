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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "dc_collector.h"
#include "get_daemon_name.h"
#include "condor_config.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"
#include "classad_oldnew.h"
#include "conversion.h"
using namespace std;

#include "match_lite.h"
#include "match_lite_resources.h"
#include "newclassad_stream.h"

#include "OrderedSet.h"
#include "stork-mm.h"
#include <list>

typedef pair<time_t, const char *> Dest;

class StorkMatchTest: public Service
{
  public:
	// ctor/dtor
	StorkMatchTest( void );
	~StorkMatchTest( void );
	int init( void );
	int config( bool );

  private:

	// Timer handlers
	int timerHandler( void );

	StorkMatchMaker			*mm;
	multimap<time_t, const char *, less<time_t> >dests;
	unsigned	target;
	//list<Dest *>			dests;
	char					*my_name;

	int TimerId;
	int Interval;
};

StorkMatchTest::StorkMatchTest( void )
{
}

StorkMatchTest::~StorkMatchTest( void )
{
}

int
StorkMatchTest::init( void )
{
	my_name = NULL;
	target = 200;

	// Read our Configuration parameters
	config( true );

	// Create teh match maker object
	mm = new StorkMatchMaker();
	dprintf( D_FULLDEBUG, "mm size %d @ %p\n", sizeof(*mm), mm );

	TimerId = daemonCore->Register_Timer(
		10, 10, (TimerHandlercpp)&StorkMatchTest::timerHandler,
		"StorkMatchTest::timerHandler()", this );
	if ( TimerId <= 0 ) {
		dprintf( D_FULLDEBUG, "Register_Timer() failed\n" );
		return -1;
	}

	return 0;
}

int
StorkMatchTest::config( bool init )
{
	return 0;
}

int
StorkMatchTest::timerHandler ( void )
{
	dprintf( D_FULLDEBUG, "Timer handler starting\n" );

	// Release some destinations
	time_t	now = time( NULL );
	multimap<time_t, const char *>::iterator iter, tmp;
	int		offset = 0;
	for( iter = dests.begin(); iter != dests.end(); iter++ ) {
		if ( iter->first > now ) {
			break;
		}
		tmp = iter;
		iter++;
		if ( random() % 20 ) {
			dprintf( D_FULLDEBUG, "Returning xfer %s\n", tmp->second );
			mm->returnTransferDestination( tmp->second );
		} else {
			dprintf( D_FULLDEBUG, "Failing xfer %s\n", tmp->second );
			mm->failTransferDestination( tmp->second );
		}
		free( (void*) tmp->second );
		dests.erase( tmp );
	}

	// Grab some destinations
	while ( dests.size() < target ) {
		if ( 0 == (random()%15) ) {
			break;
		}
		const char	*d = mm->getTransferDestination( "gsiftp" );
		if ( !d ) {
			break;
		}
		int		duration = 1 + ( random() % 300 );
		time_t	end = now + duration;
		dests.insert( make_pair( end, d ) );
		dprintf( D_FULLDEBUG, "Added %s duration %d end %ld\n",
				 d, duration, end );
	}

	dprintf( D_FULLDEBUG, "Timer handler done\n" );
	return 0;
}


//-------------------------------------------------------------

// about self
char* mySubSystem = "StorkMatchTest";		// used by Daemon Core

StorkMatchTest	smt;

//-------------------------------------------------------------

int main_init(int argc, char *argv[])
{
	DebugFlags = D_FULLDEBUG | D_ALWAYS;
	dprintf(D_ALWAYS, "main_init() called\n");

	smt.init( );


	return TRUE;
}

//-------------------------------------------------------------

int 
main_config( bool is_full )
{
	dprintf(D_ALWAYS, "main_config() called\n");
	smt.config( true );
	return TRUE;
}

//-------------------------------------------------------------

int main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int argc, char* argv[] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}
