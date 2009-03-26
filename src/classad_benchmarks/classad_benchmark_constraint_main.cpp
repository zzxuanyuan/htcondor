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
#include "condor_attributes.h"
#include "condor_distribution.h"
#include "condor_config.h"
#include "simple_arg.h"

#include <list>

#include "classad_benchmark_constraint_base.h"
#if BENCHMARK_NEW_CLASSADS
#  include "classad_benchmark_constraint_new.h"
   typedef ClassAdConstraintBenchmarkNew Benchmark;
#else
#  include "classad_benchmark_constraint_old.h"
   typedef ClassAdConstraintBenchmarkOld Benchmark;
#endif

#include "debug_timer_dprintf.h"

static const char *	VERSION = "0.1";

// Options
class Options
{
public:
	Options( Benchmark &bench ) :
			m_bench( bench ),
			m_num_ads( 0 ),
			m_num_queries( 0 ),
			m_query( NULL ),
			m_two_way( false )
		{ /* Do nothing */ };
	~Options( void ) { };
	bool Verify( void ) const;

	// Accessors
	Benchmark &getBench( void ) const { return m_bench; };
	bool setNumAds( int num ) { m_num_ads = num; return true; };
	int getNumAds( void ) const { return m_num_ads; };
	bool setNumQueries( int num ) { m_num_queries = num; return true; };
	int getNumQueries( void ) const { return m_num_queries; };
	bool setAdFile( const char *f ) { m_ad_file = f; return true; };
	const char * getAdFile( void ) const { return m_ad_file; };
	bool setQuery( const char *q ) { m_query = q; return true; };
	const char * getQuery( void ) const { return m_query; };
	bool setTwoWay( bool two_way ) { m_two_way = two_way; return true; };
	bool getTwoWay( void ) const { return m_two_way; };

private:
	Benchmark	&m_bench;
	int			 m_num_ads;
	int			 m_num_queries;
	const char	*m_ad_file;
	const char	*m_query;
	bool		 m_two_way;
};

bool
Options::Verify( void ) const
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

// Prototypes
void Usage( void );
bool CheckArgs(int argc, const char **argv, Options &opts);

int main( int argc, const char *argv[] )
{
	DebugFlags = D_FULLDEBUG | D_ALWAYS;

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("CLASSAD_BENCHMARK");

	Benchmark	benchmark;
	Options		opts( benchmark );
	if ( !CheckArgs(argc, argv, opts) ) {
		exit( 1 );
	}
	if ( !opts.Verify( )) {
		exit( 1 );
	}
	if ( !benchmark.readAdFile( opts.getAdFile() ) ) {
		exit( 1 );
	}

	if ( !benchmark.setup( opts.getNumAds() ) ) {
		Usage( );
		exit( 1 );
	}

	if ( !benchmark.runQueries( opts.getNumQueries(),
								opts.getQuery(),
								opts.getTwoWay()
								) ) {
		exit( 1 );
	}
}

void
Usage( void )
{
	const char *	usage =
		"Usage: bench_constraint [options] "
		"<template-file> <num-ads> <num-searchs> [<constraint>]\n"
		"  --const <constraint>: constraint string\n"
		"  --[en|dis]able-views: Enable / disable views\n"
		"  --[en|dis]able-2way: Enable / disable 2-way matching\n"
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
		"  <constraint>: query constraint\n";
	fputs( usage, stdout );
}

bool
CheckArgs(int argc, const char **argv, Options &opts)
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
			opts.getBench().incVerbosity();

		} else if ( arg.Match("verbosity") ) {
			int	verb;
			if ( ! arg.getOpt(verb) ) {
				fprintf(stderr, "Value needed for %s\n", arg.Arg() );
				Usage();
				return false;
			}
			opts.getBench().setVerbosity(verb);

		} else if ( arg.Match( 'V', "version" ) ) {
			printf("test_log_reader: %s, %s\n", VERSION, __DATE__);
			return false;

		} else if ( arg.Match( "enable-views" ) ) {
			opts.getBench().setUseView(true);
		} else if ( arg.Match( "disable-views" ) ) {
			opts.getBench().setUseView(false);

		} else if ( arg.Match( "enable-2way" ) ) {
			opts.setTwoWay(true);
		} else if ( arg.Match( "disable-2way" ) ) {
			opts.setTwoWay(false);

		} else if ( ! arg.ArgIsOpt() ) {
			if ( 0 == fixed ) {
				const char	*file = NULL;
				if ( !arg.getOpt( file, true ) ) {
					fprintf(stderr, "Invalid file name\n" );
					Usage();
					return false;
				}
				if ( !opts.setAdFile( file ) ) {
					fprintf(stderr, "Failed to set ad file to %s\n", file );
					return false;
				}
			}
			else if ( 1 == fixed ) {
				int		num;
				if ( !arg.getOpt(num) ) {
					fprintf(stderr, "Invalid ad count %s\n", arg.Arg() );
					Usage();
					return false;
				}
				opts.setNumAds( num );
			}
			else if ( 2 == fixed ) {
				int		num;
				if ( !arg.getOpt(num) ) {
					fprintf(stderr, "Invalid search count %s\n", arg.Arg() );
					Usage();
					return false;
				}
				opts.setNumQueries( num );
			}
			else if ( 3 == fixed ) {
				opts.setQuery( arg.Arg() );
				arg.ConsumeOpt();
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
