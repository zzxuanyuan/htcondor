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

CaBenchQueryOptions::CaBenchQueryOptions( const char *v,
										  bool support_views,
										  const char *name ) 
		: m_version( v ),
		  m_support_views( support_views ),
		  m_name( name ),
		  m_verbosity( 0 ),
		  m_num_ads( 0 ),
		  m_num_queries( 0 ),
		  m_ad_file( NULL ),
		  m_filter_expr( NULL ),
		  m_query( NULL ),
		  m_view_expr( NULL ),
		  m_two_way( false ),
		  m_random( false )
{
	/* Do nothing */
}

bool
CaBenchQueryOptions::Verify( void ) const
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

void
CaBenchQueryOptions::Usage( void ) const
{
	const char *	usage =
		"Usage: bench_query [options] "
		"<template-file> <num-ads> <num-searchs> <query>\n"
		"  --filter <expr>: Filter ads from file with <expr>\n"
		"  --view <expr>: Use view with <expr>\n"
		"  --disable-view: Disable view\n"
		"  --[en|dis]able-2way: En/Dis-able 2-way matching <disabled>\n"
		"  --[en|dis]able-random: En/Disable randomized collection<disabled>\n"
		"\n"
		"  -d <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --debug <level>: debug level (e.g., D_FULLDEBUG)\n"
		"  --usage|--help|-h: print this message and exit\n"
		"  -v: Increase verbosity level by 1\n"
		"  --verbosity <number>: set verbosity level (default is 1)\n"
		"  --version: print the version number and compile date\n"
		"\n"
		"  <template-file>: file with template ad(s)\n"
		"  <num-ads>: # of ads to put in the collection\n"
		"  <num-queries>: number of queries to perform\n"
		"  <query>: query constraint\n";
	fputs( usage, stdout );
}

bool
CaBenchQueryOptions::ProcessArgs(int argc, const char *argv[] )
{
	int	fixed = 0;
	for ( int index = 1; index < argc;  ) {
		SimpleArg	arg( argv, argc, index );

		if ( arg.Error() ) {
			Usage();
			return false;
		}

		if ( arg.Match( 'd', "debug") ) {
			if ( arg.hasOpt() ) {
				set_debug_flags( arg.getOpt() );
				index = arg.ConsumeOpt( );
			} else {
				fprintf(stderr, "Value needed for %s\n", arg.Arg() );
				Usage();
				return false;
			}

		} else if ( ( arg.Match("usage") )		||
					( arg.Match('h') )			||
					( arg.Match("help") )  )	{
			Usage();
			return false;

		} else if ( arg.Match('v') ) {
			m_verbosity++;

		} else if ( arg.Match("verbosity") ) {
			if ( ! arg.getOpt(m_verbosity) ) {
				fprintf(stderr, "Value needed for %s\n", arg.Arg() );
				Usage();
				return false;
			}

		} else if ( arg.Match( 'V', "version" ) ) {
			printf("test_log_reader: %s, %s\n", m_version, __DATE__);
			return false;

		} else if ( arg.Match( "view" ) ) {
			if ( !m_support_views ) {
				fprintf( stderr, "Views not supported by %s\n", m_name );
				return false;
			}
			m_view_expr = NULL;
			if ( !arg.getOpt( m_view_expr, true ) ) {
				fprintf(stderr, "No view expr specified\n" );
				Usage();
				return false;
			}

		} else if ( arg.Match( "filter" ) ) {
			m_filter_expr = NULL;
			if ( !arg.getOpt( m_filter_expr, true ) ) {
				fprintf(stderr, "No filter expr specified\n" );
				Usage();
				return false;
			}

		} else if ( arg.Match( "disable-view" ) ) {
			m_view_expr = NULL;

		} else if ( arg.Match( "enable-2way" ) ) {
			m_two_way = true;
		} else if ( arg.Match( "disable-2way" ) ) {
			m_two_way = false;

		} else if ( arg.Match( "enable-random" ) ) {
			m_random = true;
		} else if ( arg.Match( "disable-random" ) ) {
			m_random =  false;

		} else if ( ! arg.ArgIsOpt() ) {
			if ( 0 == fixed ) {
				if ( !arg.getOpt( m_ad_file, true ) ) {
					fprintf(stderr, "Invalid file name\n" );
					Usage();
					return false;
				}
			}
			else if ( 1 == fixed ) {
				if ( !arg.getOpt( m_num_ads ) ) {
					fprintf(stderr, "Invalid ad count %s\n", arg.Arg() );
					Usage();
					return false;
				}
			}
			else if ( 2 == fixed ) {
				if ( !arg.getOpt( m_num_queries ) ) {
					fprintf(stderr, "Invalid search count %s\n", arg.Arg() );
					Usage();
					return false;
				}
			}
			else if ( 3 == fixed ) {
				if ( !arg.getOpt( m_query, true ) ) {
					fprintf(stderr, "Invalid query %s\n", arg.Arg() );
					Usage();
					return false;
				}
			}
			else {
				fprintf(stderr, "Unrecognized argument: <%s>\n", arg.Arg() );
				Usage();
				return false;
			}
			fixed++;

		} else {
			fprintf(stderr, "Unrecognized argument: <%s>\n", arg.Arg() );
			Usage();
			return false;
		}
		index = arg.Index();
	}

	return true;
}
