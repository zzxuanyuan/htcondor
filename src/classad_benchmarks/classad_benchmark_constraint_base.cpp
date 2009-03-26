/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "stat_wrapper.h"
#include "condor_random_num.h"
#include "debug_timer_printf.h"

#include "classad_benchmark_constraint_base.h"

#include <vector>

ClassAdConstraintBenchmarkBase::ClassAdConstraintBenchmarkBase( void )
{
	m_verbosity = 0;
	m_view = false;
}

ClassAdConstraintBenchmarkBase::~ClassAdConstraintBenchmarkBase( void )
{
}

bool
ClassAdConstraintBenchmarkBase::setUseView( bool use_view )
{
	m_view = use_view;
	return true;
}

bool
ClassAdConstraintBenchmarkBase::setVerbosity( int v )
{
	m_verbosity = v;
	return true;
}

bool
ClassAdConstraintBenchmarkBase::incVerbosity( void )
{
	m_verbosity++;
	return true;
}

bool
ClassAdConstraintBenchmarkBase::readAdFile( const char *fname )
{
	FILE	*fp = fopen( fname, "r" );
	if ( !fp ) {
		fprintf( stderr, "Error opening %s\n", fname );
		return false;
	}
	while( parseTemplateAd(fp) ) {
		// Do nothing
	}
	fclose( fp );
	printf( "Read %d template ads\n", numTemplates() );
	return ( numTemplates() != 0 );
}

bool
ClassAdConstraintBenchmarkBase::setup( int num_ads )
{
	// Template ad?
	int		num_templates = numTemplates();
	if ( !num_templates ) {
		return false;
	}

	// Setup the view
	if ( m_view && (!createView( "MyType", "Machine" ) )  ) {
		return false;
	}

	// Generate ads
	DebugTimerPrintf	timer;
	for( int i = 0;  i < num_ads;  i++ ) {
		int			 ad_num  = (get_random_int() % num_templates);
		if ( !generateAd( ad_num ) ) {
			fprintf( stderr, "Ad generation failed\n" );
			return false;
		}
	}	
	timer.Log( "setup ads", num_ads );

	if ( !collectionInfo( ) ) {
		return false;
	}

	return true;
}

bool
ClassAdConstraintBenchmarkBase::runQueries( 
	int num_queries, const char *query, bool two_way )
{

	DebugTimerPrintf	timer;
	for( int i = 0;  i < num_queries;  i++ ) {
		int					 matches = 0;
		DebugTimerPrintf	 qtimer( false );

		qtimer.Start();
		if ( !runQuery( query, two_way, matches ) ) {
			fprintf( stderr, "runQuery failed\n" );
			return false;
		}
		qtimer.Log( "Query matches", matches );
	}
	timer.Log( "searchs", num_queries );

	return true;
}
