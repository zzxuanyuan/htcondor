/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "subsystem_info.h"

#include "RsmStandard.h"

/* daemon core needs this variable to associate its entries
 * inside $CONDOR_CONFIG file
 */
DECL_SUBSYSTEM( "Replicator", SUBSYSTEM_TYPE_DAEMON );

// replication daemon single object
StandardRsm* stateMachine = NULL;

int
main_init( int , char *[] )
{
    dprintf(D_FULLDEBUG, "main_init replication daemon started\n");

    try {
        stateMachine = new StandardRsm( );
        if ( !stateMachine->initializeAll( ) ) {
			dprintf( D_ALWAYS, "State machine failed to initialize!\n" );
			return FALSE;
		}
        return TRUE;
    }
    catch( char* exceptionString ) {
        dprintf( D_FAILURE,
				 "main_init exception thrown %s\n", exceptionString );
        return FALSE;
    }
}

int
main_shutdown_graceful( void )
{
    dprintf( D_FULLDEBUG, "main_shutdown_graceful started\n" );

    delete stateMachine;
    DC_Exit( 0 );

    return 0;
}

int
main_shutdown_fast( void )
{
    delete stateMachine;
    DC_Exit( 0 );

    return 0;
}

int
main_config( bool /*isFull*/ )
{
    // NOTE: restart functionality instead of reconfig
	stateMachine->reconfig( );
    
    return 0;
}

void
main_pre_dc_init( int /*argc*/, char* /*argv*/[] )
{
}

void
main_pre_command_sock_init( void )
{
}

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// c-basic-offset:4 ***
// End: ***
