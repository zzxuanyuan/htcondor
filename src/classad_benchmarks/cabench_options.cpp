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

#include "cabench_query_options.h"

CaBenchOptions::CaBenchOptions( const char *v,
								const char *name,
								const char *opts ) 
		: m_version( v ),
		  m_name( name ),
		  m_opts( opts ),
		  m_verbosity( 0 )
{
	/* Do nothing */
}

void
CaBenchOptions::Usage( void ) const
{
	const char *	usage =
		"Usage: %s [options] "
		"%s\n"
		"  -d <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --debug <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number>: set verbosity level (default is 1)\n"
		"  --version: print the version number and compile date\n";
	printf( usage, m_name, m_opts );
}

CaBenchOptions::OptStatus
CaBenchOptions::ProcessArg(SimpleArg &arg, int &index )
{
	if ( arg.Error() ) {
		return OPT_ERROR;
	}

	if ( arg.Match( 'd', "debug") ) {
		if ( arg.hasOpt() ) {
			set_debug_flags( arg.getOpt() );
			index = arg.ConsumeOpt( );
		} else {
			fprintf(stderr, "Value needed for %s\n", arg.Arg() );
			return OPT_ERROR;
		}

	} else if ( ( arg.Match("usage") )		||
				( arg.Match('h') )			||
				( arg.Match("help") )  )	{
		Usage();
		return OPT_DONE;

	} else if ( arg.Match('v') ) {
		m_verbosity++;
		return OPT_HANDLED;

	} else if ( arg.Match("verbosity") ) {
		if ( ! arg.getOpt(m_verbosity) ) {
			fprintf(stderr, "Value needed for %s\n", arg.Arg() );
			return OPT_ERROR;
		}
		return OPT_HANDLED;

	} else if ( arg.Match( 'V', "version" ) ) {
		printf("test_log_reader: %s, %s\n", m_version, __DATE__);
		return OPT_HANDLED;
	}

	return OPT_OTHER;
}
