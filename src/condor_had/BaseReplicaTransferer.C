#include "../condor_daemon_core.V6/condor_daemon_core.h"
// for 'param' function
#include "condor_config.h"

#include "BaseReplicaTransferer.h"
#include "FilesOperations.h"
#include "Utils.h"

extern int main_shutdown_graceful();

BaseReplicaTransferer::BaseReplicaTransferer(
                                 const MyString&  pDaemonSinfulString,
                                 const MyString&  pVersionFilePath,
                                 const MyString&  pStateFilePath ):
   daemonSinfulString( pDaemonSinfulString ),
   versionFilePath( pVersionFilePath ),
   stateFilePath( pStateFilePath ), socket( 0 ),
   connectionTimeout( DEFAULT_SEND_COMMAND_TIMEOUT )
{
}

BaseReplicaTransferer::~BaseReplicaTransferer()
{
        delete socket;
}

int
BaseReplicaTransferer::reinitialize( )
{
    dprintf( D_ALWAYS, "BaseReplicaTransferer::reinitialize started\n" );
    
    char* buffer = param( "HAD_CONNECTION_TIMEOUT" );
    
    if( buffer ) {
        bool result = false;

		connectionTimeout = utilAtoi( buffer, &result ); 
		//strtol( buffer, 0, 10 );
        
        if( ! result || connectionTimeout <= 0 ) {
            dprintf( D_FAILURE, const_cast<char*>(
                utilConfigurationError("HAD_CONNECTION_TIMEOUT", 
									   "HAD").GetCStr( ) ) );
            main_shutdown_graceful( );
        }
        free( buffer );
    } else {
        dprintf( D_FAILURE, const_cast<char*>(
       		utilNoParameterError("HAD_CONNECTION_TIMEOUT", "HAD").GetCStr( ) ));
        main_shutdown_graceful( );
    }

    return TRANSFERER_TRUE;
}
