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

#include "classad_benchmark_constraint_options.h"

ClassAdConstraintBenchmarkOptions::ClassAdConstraintBenchmarkOptions( void ) 
		: m_verbosity( 0 ),
		  m_num_ads( 0 ),
		  m_num_queries( 0 ),
		  m_query( NULL ),
		  m_view_expr( NULL ),
		  m_two_way( false ),
		  m_random( false )
{
	/* Do nothing */
}

bool
ClassAdConstraintBenchmarkOptions::Verify( void ) const
{
	if ( m_num_ads == 0 ) {
		fprintf( stderr, "No # ads specified\n" );
		return false;
	}
	if ( m_num_queries == 0 ) {
		fprintf( stderr, "No # queries specified\n" );
		return false;
	}
	if ( m_ad_file == NULL ) {
		fprintf( stderr, "No ad file specified\n" );
		return false;
	}
	if ( m_query == NULL ) {
		fprintf( stderr, "No query specified\n" );
		return false;
	}
	return true;
}
