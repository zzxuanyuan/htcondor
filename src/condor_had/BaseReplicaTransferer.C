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
   m_daemonSinfulString( pDaemonSinfulString ),
   m_versionFilePath( pVersionFilePath ),
   m_stateFilePath( pStateFilePath ), m_socket( 0 ),
   m_connectionTimeout( DEFAULT_SEND_COMMAND_TIMEOUT ),
   m_maxTransferLifetime( DEFAULT_MAX_TRANSFER_LIFETIME )
{
}

BaseReplicaTransferer::~BaseReplicaTransferer()
{
        delete m_socket;
}

int
BaseReplicaTransferer::reinitialize( )
{
    dprintf( D_ALWAYS, "BaseReplicaTransferer::reinitialize started\n" );
    
    char* buffer = param( "HAD_CONNECTION_TIMEOUT" );
    
    if( buffer ) {
        bool result = false;

		m_connectionTimeout = utilAtoi( buffer, &result ); 
		//strtol( buffer, 0, 10 );
        
        if( ! result || m_connectionTimeout <= 0 ) {
            utilCrucialError(
				utilConfigurationError("HAD_CONNECTION_TIMEOUT", 
									   "HAD").GetCStr( ) );
        }
        free( buffer );
    } else {
		utilCrucialError(
       		utilNoParameterError("HAD_CONNECTION_TIMEOUT", "HAD").GetCStr( ) );
    }

	m_maxTransferLifetime = param_integer( "MAX_TRANSFER_LIFETIME",
											DEFAULT_MAX_TRANSFER_LIFETIME );
    return TRANSFERER_TRUE;
}
