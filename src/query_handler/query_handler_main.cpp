/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "classad_collection.h"

#include "QueryHandler.h"


QueryHandler *query_handler;


ClassAdLog::filter_iterator
BeginIterator(const classad::ExprTree &requirements, int timeslice_ms)
{
	dprintf(D_ALWAYS, "Begin iterator.\n");
        ClassAdLog::filter_iterator it(query_handler ? &(query_handler->GetClassAds()->table) : NULL, &requirements, timeslice_ms);
        return it;
}


ClassAdLog::filter_iterator
EndIterator()
{
        ClassAdLog::filter_iterator it(query_handler ? &query_handler->GetClassAds()->table : NULL, NULL, 0, true);
        return it;
}

//-------------------------------------------------------------

void main_init(int   /*argc*/, char ** /*argv*/)
{
	query_handler = new QueryHandler();
	query_handler->init();
}

//-------------------------------------------------------------

void 
main_config()
{
	query_handler->config();
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	query_handler->stop();
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	query_handler->stop();
}

//-------------------------------------------------------------

int
main( int argc, char **argv )
{
	set_mySubSystem("QUERY_HANDLER", SUBSYSTEM_TYPE_SCHEDD );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}

