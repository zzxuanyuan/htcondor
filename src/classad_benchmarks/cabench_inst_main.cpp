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

#include "cabench_inst_base.h"

#if   defined BENCHMARK_NEW
#  include "cabench_inst_new.h"
   typedef CaBenchInstNew CaBench;
#elif defined BENCHMARK_OLD
#  include "cabench_inst_old.h"
   typedef CaBenchInstOld CaBench;
#endif

#include "debug_timer_dprintf.h"

static const char *	VERSION = "0.1";

// Prototypes

int main( int argc, const char *argv[] )
{
	DebugFlags = D_FULLDEBUG | D_ALWAYS;

		// initialize to read from config file
	myDistro->Init( argc, argv );
	config();

		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("CABENCH_");

	CaBenchInstOptions	opts( VERSION,
							  CaBench::Name() );
	if ( !opts.ProcessArgs(argc, argv) ) {
		exit( 1 );
	}
	if ( !opts.Verify( )) {
		exit( 1 );
	}

	CaBench		benchmark( opts );
	if ( !benchmark.setup( ) ) {
		exit( 1 );
	}

	if ( !benchmark.runLoops( ) ) {
		exit( 1 );
	}

	if ( !benchmark.finish( ) ) {
		exit( 1 );
	}
	return 0;
}
