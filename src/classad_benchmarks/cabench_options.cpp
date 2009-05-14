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
								const char *name )
		: m_version( v ),
		  m_name( name ),
		  m_verbosity( 0 ),
		  m_num_loops( -1 ),
		  m_data_file( NULL ),
		  m_num_ads( 0 ),
		  m_ad_mult( 0 ),
		  m_random( false )
{
	/* Do nothing */
}

void
CaBenchOptions::Usage( void ) const
{
	const char *	usage =
		"Usage: %s <loops> <data-file> <num-ads|*ad-mult> %s [options]\n"
		"  -d <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --debug <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number>: set verbosity level (default is 1)\n"
		"  --version: print the version number and compile date\n"
		"\n"
		"%s"
		"  --[en|dis]able-random: En/Disable randomization <disabled>\n"
		"\n"
		"  <loops>: number of loops to perform\n"
		"  <data-file>: Data file for benchmark\n"
		"  <num-ads>: Target # of ads to put in the collection/list\n"
		"  <*num-ads>: Ad count multiplier\n"
		"%s";
	printf( usage, m_name, getUsage(), getOpts(), getFixed() );
}

bool
CaBenchOptions::Verify( void ) const
{
	if ( m_num_loops < 0 ) {
		fprintf( stderr, "No # loops specified\n" );
		return false;
	}
	if ( m_data_file == NULL ) {
		fprintf( stderr, "No data file specified\n" );
		return false;
	}
	if ( ( m_num_ads == 0 ) && ( m_ad_mult == 0 ) ){
		fprintf( stderr, "No # ads / ad multiplier specified\n" );
		return false;
	}
	return true;
}

CaBenchOptions::OptStatus
CaBenchOptions::ProcessArgs(int argc, const char *argv[] )
{
	int	fixed = 0;
	for ( int index = 1; index < argc;  ) {
		SimpleArg	arg( argv, argc, index );

		if ( arg.Error() ) {
			Usage();
			return OPT_ERROR;
		}

		OptStatus	status;
		if ( arg.ArgIsOpt() ) {
			status = ProcessArg( arg, index );
		}
		else {
			status = ProcessArg( arg, index, fixed );
		}

		switch( status ) {
		case OPT_ERROR:
			Usage( );
			return status;

		case OPT_HANDLED:
			goto BOTTOM;

		case OPT_DONE:
		case OPT_HELP:
			return status;

		default:
			fprintf(stderr, "Unrecognized argument: <%s>\n", arg.Arg() );
			Usage();
			return OPT_ERROR;
		}

	  BOTTOM:
		index = arg.Index();
	}
	return OPT_DONE;
}

CaBenchOptions::OptStatus
CaBenchOptions::ProcessArg(SimpleArg &arg, int index )
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
		return OPT_HELP;

	} else if ( arg.Match('v') ) {
		m_verbosity++;

	} else if ( arg.Match("verbosity") ) {
		if ( ! arg.getOpt(m_verbosity) ) {
			fprintf(stderr, "Value needed for %s\n", arg.Arg() );
			return OPT_ERROR;
		}

	} else if ( arg.Match( 'V', "version" ) ) {
		printf("test_log_reader: %s, %s\n", m_version, __DATE__);
	}
	else if ( arg.Match( "enable-random" ) ) {
		m_random = true;
	}
	else if ( arg.Match( "disable-random" ) ) {
		m_random =  false;
	}
	else {
		return ProcessArgLocal( arg, index );
	}
	return OPT_HANDLED;
}

CaBenchOptions::OptStatus
CaBenchOptions::ProcessArg(SimpleArg &arg, int index, int &fixed )
{
	if ( 0 == fixed ) {
		if ( !arg.getOpt( m_num_loops ) ) {
			fprintf(stderr, "Invalid loop count %s\n", arg.Arg() );
			return OPT_ERROR;
		}
	}
	else if ( 1 == fixed ) {
		if ( !arg.getOpt( m_data_file, true ) ) {
			fprintf(stderr, "Invalid file name\n" );
			return CaBenchOptions::OPT_ERROR;
		}
	}
	else if ( 2 == fixed ) {
		const char *s = arg.Arg();
		if ( (s[0] == '*') && isdigit(s[1]) ) {
			m_ad_mult = atoi( s+1 );
			arg.ConsumeOpt( );
		}
		else if ( !arg.getOpt( m_num_ads ) ) {
			fprintf(stderr, "Invalid ad count %s\n", arg.Arg() );
			return CaBenchOptions::OPT_ERROR;
		}
	}
	else {
		OptStatus status = ProcessArgLocal( arg, index, fixed - 3 );
		if ( OPT_HANDLED != status ) {
			return status;
		}
	}

	fixed++;
	return OPT_HANDLED;
}
