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

// Prototypes
void Usage( void );
bool CheckArgs(int argc, const char **argv,
			   ClassAdConstraintBenchmarkOptions &opts);

int main( int argc, const char *argv[] )
{
	DebugFlags = D_FULLDEBUG | D_ALWAYS;

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("CLASSAD_BENCHMARK");

	ClassAdConstraintBenchmarkOptions	opts;
	Benchmark							benchmark( opts );
	if ( !CheckArgs(argc, argv, opts) ) {
		exit( 1 );
	}
	if ( !opts.Verify( )) {
		exit( 1 );
	}
	if ( !benchmark.readAdFile( opts.getAdFile() ) ) {
		exit( 1 );
	}

	if ( !benchmark.setup( opts.getNumAds(), opts.getViewExpr() ) ) {
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
		"<template-file> <num-ads> <num-searchs> <constraint>\n"
		"  --view <expr>: Use view with <expr>\n"
		"  --disable-view: Disable view\n"
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
CheckArgs(int argc, const char **argv, ClassAdConstraintBenchmarkOptions &opts)
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
			opts.incVerbosity();

		} else if ( arg.Match("verbosity") ) {
			int	verb;
			if ( ! arg.getOpt(verb) ) {
				fprintf(stderr, "Value needed for %s\n", arg.Arg() );
				Usage();
				return false;
			}
			opts.setVerbosity(verb);

		} else if ( arg.Match( 'V', "version" ) ) {
			printf("test_log_reader: %s, %s\n", VERSION, __DATE__);
			return false;

		} else if ( arg.Match( "view" ) ) {
			const char	*expr = NULL;
			if ( !arg.getOpt( expr, true ) ) {
				fprintf(stderr, "No view expr specified\n" );
				Usage();
				return false;
			}
			opts.setViewExpr( expr );

		} else if ( arg.Match( "disable-view" ) ) {
			opts.setViewExpr(NULL);

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
				opts.setAdFile( file );
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
