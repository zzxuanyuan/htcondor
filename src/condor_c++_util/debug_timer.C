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

#include "debug_timer.h"

DebugTimer::DebugTimer( bool start )
{
	if ( start ) {
		Start( );
	}
}

DebugTimer::~DebugTimer( void )
{
}

double
DebugTimer::dtime( void )
{
	struct timeval	tv;
	gettimeofday( &tv, NULL );
	return ( tv.tv_sec + ( tv.tv_usec / 1000000.0 ) );
}

void
DebugTimer::Start( void )
{
	t1 = dtime( );
	on = true;
}

void
DebugTimer::Stop( void )
{
	if ( on ) {
		t2 = dtime( );
		on = false;
	}
}

void
DebugTimer::Log( const char *s, int num, bool stop )
{
	if ( stop ) {
		Stop( );
	}
	double	timediff = t2 - t1;
	if ( num >= 0 ) {
		double	per = 0.0, per_sec = 0.0;
		if ( num > 0 ) {
			per = timediff / num;
			per_sec = 1.0 / per;
		}
		dprintf( D_FULLDEBUG,
				 "DebugTimer: %-25s %4d in %8.5fs => %9.7fsp %10.2f/s\n",
				 s, num, timediff, per, per_sec );
	} else {
		dprintf( D_FULLDEBUG, "DebugTimer: %-25s %8.5fs\n", s, timediff );
	}
}
