#include "../condor_daemon_core.V6/condor_daemon_core.h"

// replication classes
#include "UploadReplicaTransferer.h"
#include "DownloadReplicaTransferer.h"

extern char* myName;
// for daemon core
char *mySubSystem = "TRANSFERER"; 

// single 'condor_transferer' object
BaseReplicaTransferer* replicaTransferer = NULL; 

/* Function: main_init
 * Arguments: argv[0] - name,
 *            argv[1] - up | down,
 *            argv[2] - another communication side daemon's sinful string,
 *            argv[3] - version file,
 *            argv[4] - state file
 * Remarks: flags (like '-f') are being stripped off before this function call
 */
int
main_init( int argc, char *argv[] )
{
     if( argc != 5 ) {
         dprintf( D_PROC, "Transfer error: arguments number differs from 5\n" );

         DC_Exit( 1 );
    }

    if( ! strncmp( argv[1], "down", strlen( "down" ) ) ) {
        replicaTransferer = new DownloadReplicaTransferer( 
								argv[2], 
								argv[3], 
								argv[4] );
    } else if( ! strncmp( argv[1], "up", strlen( "up" ) ) ) {
        replicaTransferer = new UploadReplicaTransferer( 
								argv[2], 
								argv[3], 
								argv[4] );
    } else {
        dprintf( D_PROC, "Transfer error: first parameter must be "
                         "either up or down\n" );
        DC_Exit( 1 );
    }

    int result = replicaTransferer->initialize( );

    DC_Exit( result );

    return TRUE; // compilation reason
}

int
main_shutdown_graceful( )
{
    delete replicaTransferer;
    DC_Exit( 0 );

    return 0; // compilation reason
}

int
main_shutdown_fast( )
{
  delete replicaTransferer;
  DC_Exit( 0 );

  return 0; // compilation reason
}

// no reconfigurations enabled
int
main_config( bool is_full )
{
    return 1;
}

void
main_pre_dc_init( int argc, char* argv[] )
{
}

void
main_pre_command_sock_init( )
{
}
