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
#include "condor_daemon_core.h"
#include "subsystem_info.h"

#include "FilesOperations.h"
#include "Utils.h"

#ifdef UPLOADER
   DECL_SUBSYSTEM( "UPLOADER", SUBSYSTEM_TYPE_DAEMON );
#  include "TransfererUpload.h"
#else
   DECL_SUBSYSTEM( "DOWNLOADER", SUBSYSTEM_TYPE_DAEMON );
#  include "TransfererDownload.h"
#endif
static BaseTransferer	*transferer;

static int
terminateSignalHandler(Service * /*service*/, int signalNumber)
{
    dprintf( D_ALWAYS, "terminateSignalHandler()\n" );
    ASSERT( signalNumber == 100 );

	transferer->stopTransfer( );
	transferer->finish( );

    exit(1);
	return 0;
}

/*
 * Function: main_init
 */
int
main_init( int argc, char *argv[] )
{
# ifdef DOWNLOADER
	Sinful	*replicator_sinful = NULL;
	bool	 rotate = false;
# else	/* Uploader */
	Sinful	*downloader_sinful = NULL;
	bool	 wait_for_peer = false;
	bool	 forever = false;
# endif

	int		 skip = 0;
	for( int argno = 1;  argno < argc;	argno++ ) {
		if ( skip ) {
			skip--;
			continue;
		}
		const char	*arg = argv[argno];
		const char	*arg1 = (argno <= argc-1) ? argv[argno+1] : NULL;
		if ( 0 ) {
			// place holder
		}
#     ifdef DOWNLOADER
		else if ( !strcmp( arg, "--replicator" ) ) {
			if ( NULL == arg1 ) {
				dprintf( D_ALWAYS, "--replicator requires an argument\n" );
				DC_Exit( 1 );
			}
			replicator_sinful = new Sinful(arg1);
			if ( !replicator_sinful->valid() ) {
				dprintf( D_ALWAYS,
						 "invalid sinful parameter to --replicator '%s'\n",
						 arg1 );
			}
			skip = 1;
		}
		else if ( !strcmp( arg, "--rotate" ) ) {
			rotate = true;
		}
#     else	/* UPLOADER */
		else if ( !strcmp( arg, "--downloader" ) ) {
			if ( NULL == arg1 ) {
				dprintf( D_ALWAYS, "--downloader requires an argument\n" );
				DC_Exit( 1 );
			}
			downloader_sinful = new Sinful( arg );
			if ( !downloader_sinful->valid() ) {
				dprintf( D_ALWAYS,
						 "invalid sinful parameter to --downloader '%s'\n",
						 arg );
				DC_Exit( 1 );
			}
			skip = 1;
		}
		else if ( !strcmp( arg, "--wait" ) ) {
			wait_for_peer = true;
		}
		else if ( !strcmp( arg, "--forever" ) ) {
			forever = true;
		}
#     endif
		else if ( (!strcmp( arg, "-help" ))  ||
				  (!strcmp( arg, "--help" )) ) {
#     ifdef DOWNLOADER
			dprintf( D_ALWAYS,
					 "usage: %s [--replicator <sinful>] [--rotate]\n",
					 argv[0] );
#     else
			dprintf( D_ALWAYS,
					 "usage: %s [--downloader <sinful>] [--wait] [--forever]\n",
					 argv[0] );
#     endif
			DC_Exit( 0 );
		}
		else {
			dprintf( D_ALWAYS, "Unknown argument '%s'\n", arg );
			DC_Exit( 1 );
		}
	}

# ifdef DOWNLOADER
	if ( NULL == replicator_sinful ) {
		dprintf( D_ALWAYS,
				 "No replicator specified (-help for help)\n" );
		DC_Exit( 1 );
	}

	DownloadTransferer	*downloader = new DownloadTransferer( );
	if ( rotate ) {
		downloader->enableRotate( );
	}
	transferer = downloader;
	if ( !downloader->initializeAll( ) ) {
		dprintf( D_ALWAYS, "Failed to initialize downloader\n" );
		DC_Exit( 1 );
	}
	if ( !downloader->contactPeerReplicator(*replicator_sinful) ) {
		dprintf( D_ALWAYS,
				 "Unable to contact peer replicator %s\n",
				 replicator_sinful->getSinful() );
		DC_Exit( 1 );
	}
	delete replicator_sinful;
# else
	if ( (NULL == downloader_sinful) && (!wait_for_peer) ) {
		dprintf( D_ALWAYS,
				 "No downloader or -wait specified (-help for help)\n" );
		DC_Exit( 1 );
	}

	UploadTransferer *uploader = new UploadTransferer( );
	transferer = uploader;
	if ( !uploader->initializeAll() ) {
		dprintf( D_ALWAYS, "Failed to initialize uploader\n" );
		DC_Exit( 1 );
	}
	if ( forever ) {
		uploader->runForever( );
	}
	if ( downloader_sinful ) {
		if ( !uploader->sendFileList( *downloader_sinful ) ) {
		}
	}
	else /* if (wait_for_peer) */ {
		dprintf( D_ALWAYS,
				 "Testing / wait mode: waiting for contact from peer\n" );
	}
	delete downloader_sinful;
# endif

    return TRUE;
}

int
main_shutdown_graceful( void )
{
	if ( transferer ) {
		transferer->cleanupTempFiles( );
		delete transferer;
	}
    DC_Exit( 0 );

    return 0; // compilation reason
}

int
main_shutdown_fast( void )
{
	if ( transferer ) {
		transferer->cleanupTempFiles( );
		delete transferer;
	}
	DC_Exit( 0 );

	return 0; // compilation reason
}

// no reconfigurations enabled
int
main_config( bool /*is_full*/ )
{
    return 1;
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
