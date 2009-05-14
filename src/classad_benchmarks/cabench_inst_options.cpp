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
#include "simple_arg.h"

#include "cabench_inst_options.h"

CaBenchInstOptions::CaBenchInstOptions( const char *v,
										const char *name ) 
		: CaBenchOptions( v, name ),
		  m_num_attrs( 0 )
{
	/* Do nothing */
}

bool
CaBenchInstOptions::Verify( void ) const
{
	if ( m_num_attrs == 0 ) {
		fprintf( stderr, "No # attributes specified\n" );
		return false;
	}
	return true;
}

const char *
CaBenchInstOptions::getUsage( void ) const
{
	static const char *	usage = "<num-attrs>";
	return usage;
}

const char *
CaBenchInstOptions::getOpts( void ) const
{
	static const char *	opts =
		"";
	return opts;
}

const char *
CaBenchInstOptions::getFixed( void ) const
{
	static const char *	fixed =
		"  <num-attrs>: # of attributes in each ad\n"
		"";
	return fixed;
}

CaBenchInstOptions::OptStatus
CaBenchInstOptions::ProcessArgLocal( SimpleArg &arg,
									 int &fixed,
									 int & /*index*/ )
{
	if ( ! arg.ArgIsOpt() ) {
		if ( 3 == fixed ) {
			if ( !arg.getOpt( m_num_attrs, true ) ) {
				fprintf(stderr, "Invalid # attrs %s\n", arg.Arg() );
				return CaBenchOptions::OPT_ERROR;
			}
			fixed++;
			return CaBenchOptions::OPT_HANDLED;
		}
	}
	return CaBenchOptions::OPT_ERROR;
}
