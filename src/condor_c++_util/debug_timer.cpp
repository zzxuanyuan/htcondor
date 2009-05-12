/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"

#include "debug_timer.h"

// DebugTimer simple base class methods
DebugTimerSimple::DebugTimerSimple( bool sample )
		: m_time(0),
		  m_ref(NULL)
{
	if ( sample ) {
		Sample( true );
	}
}

DebugTimerSimple::DebugTimerSimple( const DebugTimerSimple &ref, bool sample )
		: m_time(0),
		  m_ref(&ref)
{
	if ( sample ) {
		Sample( true );
	}
}

DebugTimerSimple::~DebugTimerSimple( void )
{
}

double
DebugTimerSimple::dtime( void ) const
{
	struct timeval	tv;
	gettimeofday( &tv, NULL );
	return ( tv.tv_sec + ( tv.tv_usec / 1000000.0 ) );
}

double
DebugTimerSimple::Sample( bool store )
{
	double now = dtime();
	if ( store ) {
		m_time = now;
	}
	return now;
}

const DebugTimerSimple &
DebugTimerSimple::GetRef( void ) const
{
	assert( m_ref != NULL );
	return *m_ref;
}

// DebugTimer Class with logging methods
DebugTimerOut::DebugTimerOut( const char *label, bool sample )
		: DebugTimerSimple( sample ),
		  m_label( label )
{
	return;
}

DebugTimerOut::DebugTimerOut( const DebugTimerSimple &ref,
							  const char *label, bool sample )
		: DebugTimerSimple( ref, sample ),
		  m_label( label )
{
	return;
}

DebugTimerOut::~DebugTimerOut( void )
{
}

void
DebugTimerOut::Log( void ) const
{
	Log( GetRef() );
}

void
DebugTimerOut::Log( int count ) const
{
	Log( GetRef(), count );
}

void
DebugTimerOut::Log( int count, const char *label ) const
{
	Log( GetRef(), count, label );
}

void
DebugTimerOut::Log( const DebugTimerSimple &ref ) const
{
	Log( Diff(ref) );
}

void
DebugTimerOut::Log( const DebugTimerSimple &ref, int num ) const
{
	Log( Diff(ref), num );
}

void
DebugTimerOut::Log( const DebugTimerSimple &ref, int num, const char *l ) const
{
	Log( Diff(ref), num, l );
}

void
DebugTimerOut::Log( double diff, int num ) const
{
	Log( diff, num, m_label );
}

void
DebugTimerOut::Log( double diff, int num, const char *label ) const
{
	char	buf[256];

	double	per = 0.0, per_sec = 0.0;
	if ( num > 0 ) {
		per = diff / num;
		per_sec = 1.0 / per;
	}
	snprintf( buf, sizeof( buf ),
			  "  %-15s %4d in %8.5fs => %9.7fsp %10.2f/s\n",
			  label, num, diff, per, per_sec );
	Output( buf );
}

void
DebugTimerOut::Log( double diff ) const
{	
	char	buf[256];

	snprintf( buf, sizeof( buf ),
			  "  %-15s %8.5fs\n", m_label, diff );
	Output( buf );
}
