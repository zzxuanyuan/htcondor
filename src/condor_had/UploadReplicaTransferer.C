#include "../condor_daemon_core.V6/condor_daemon_core.h"
// for copy_file
#include "util_lib_proto.h"

#include "UploadReplicaTransferer.h"
#include "FilesOperations.h"
#include "Utils.h"

/*
static int
getFileSize( const MyString& filePath )
{
	int begin, end;
	
	ifstream file ( filePath.GetCStr( ) );

	begin = file.tellg( );
	file.seekg( 0, ios::end );
	end = file.tellg( );
	file.close( );

	return end - begin;
}*/

static void
safeUnlinkStateAndVersionFiles( const MyString& stateFilePath,
							    const MyString& versionFilePath,
							    const MyString& extension )
{
	FilesOperations::safeUnlinkFile( versionFilePath.GetCStr( ),
                                     extension.GetCStr( ) );
    FilesOperations::safeUnlinkFile( stateFilePath.GetCStr( ),
                                     extension.GetCStr( ) );
}

int
UploadReplicaTransferer::initialize( )
{
    reinitialize( );

    socket = new ReliSock( );

	// enable timeouts
    //socket->set_timeout_multiplier( 1 );
    socket->timeout( INT_MAX ); // connectionTimeout );
    // no retries after 'connectionTimeout' seconds of unsuccessful connection
    socket->doNotEnforceMinimalCONNECT_TIMEOUT( );

    if( ! socket->connect( const_cast<char*>( daemonSinfulString.GetCStr( ) ), 
						   0, 
						   false ) ) {
        dprintf( D_ALWAYS, 
				"UploadReplicaTransferer::initialize cannot connect to %s\n",
                 daemonSinfulString.GetCStr( ) );
        return TRANSFERER_FALSE;
    }
    // send accounting information and version files
    return upload( );
}

/* Function    : upload
 * Return value: TRANSFERER_TRUE  - upon success
 *               TRANSFERER_FALSE - upon failure
 * Description : uploads state and version files to the
 *               downloading 'condor_transferer' process
 */
int
UploadReplicaTransferer::upload( )
{
    dprintf( D_ALWAYS, "UploadReplicaTransferer::upload started\n" );
// stress testing
// 	dprintf( D_FULLDEBUG, "UploadReplicaTransferer::upload stalling "
//                       	  "uploading process\n" );
//	sleep(300);
//	int bytesTotal = getFileSize( stateFilePath );
	// setting the timeout depending on the file size and aknowledging the 
	// receiving side about that in order to copy the file to the temporary one
	// and not to get the socket closed at the end of copy operation
//	int newTimeout = std::max( int( bytesTotal * TRANSFER_SAFETY_COEFFICIENT / 
//												 DISK_WRITE_RATIO + 0.5 ),
//                               10 );
//	int oldTimeout = socket->timeout( newTimeout );
//	dprintf( D_ALWAYS, "UploadReplicaTransferer::upload set new timeout %d, "
//					   "old timeout %d, file size %d\n",
//			 newTimeout, oldTimeout, bytesTotal );
//	socket->encode( );
//	if( ! socket->code( bytesTotal ) || 
//		! socket->eom( ) ) {
//		 dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to send "
//							"the state file size (%d) or to code the end of "
//							"message\n", bytesTotal );
//		return TRANSFERER_FALSE;
//	}
    MyString extension( daemonCore->getpid( ) );
	// the .up ending is needed in order not to confuse between upload and
    // download processes temporary files
    extension += ".";
    extension += UPLOADING_TEMPORARY_FILES_EXTENSION;

	char* temporaryVersionFilePath =
			const_cast<char*>(versionFilePath.GetCStr());
	char* temporaryStateFilePath   = const_cast<char*>(stateFilePath.GetCStr());
	char* temporaryExtension       = const_cast<char*>(extension.GetCStr());

    if( ! FilesOperations::safeCopyFile( temporaryVersionFilePath,
										 temporaryExtension ) ) {
		dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to copy "
						   "version file %s\n", temporaryVersionFilePath );
		FilesOperations::safeUnlinkFile( temporaryVersionFilePath,
										 temporaryExtension );
		return TRANSFERER_FALSE;
	}
	if( ! FilesOperations::safeCopyFile( temporaryStateFilePath,
                                         temporaryExtension ) ) {
        dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to copy "
                           "state file %s\n", temporaryStateFilePath );
		safeUnlinkStateAndVersionFiles( temporaryStateFilePath,
                                        temporaryVersionFilePath,
									    temporaryExtension );
        return TRANSFERER_FALSE;
    }
    // upload version file
    if( uploadFile( versionFilePath, extension ) == TRANSFERER_FALSE){
		dprintf( D_ALWAYS, "UploadReplicaTransferer::upload unable to upload "
						   "version file %s\n", temporaryVersionFilePath );
		FilesOperations::safeUnlinkFile( temporaryStateFilePath, 
										 temporaryExtension );
		return TRANSFERER_FALSE;
    }
	// trying to unlink the temporary files; upon failure we still return the
    // status of uploading the files, since the most important thing here is
    // that the files were uploaded successfully
	FilesOperations::safeUnlinkFile( temporaryVersionFilePath, 
									 temporaryExtension );
    if( uploadFile( stateFilePath, extension ) == TRANSFERER_FALSE ){
		return TRANSFERER_FALSE;
	}
	FilesOperations::safeUnlinkFile( temporaryStateFilePath,
									 temporaryExtension );
	return TRANSFERER_TRUE;
}

/* Function    : uploadFile
 * Arguments   : filePath   - the name of uploaded file
 *               extension  - temporary file extension
 * Return value: TRANSFERER_TRUE  - upon success
 *               TRANSFERER_FALSE - upon failure
 * Description : uploads the specified file to the downloading process through
 *               the transferer socket
 */
int
UploadReplicaTransferer::uploadFile( MyString& filePath, MyString& extension )
{
    dprintf( D_ALWAYS, "UploadReplicaTransferer::uploadFile %s.%s started\n", 
			 filePath.GetCStr( ), extension.GetCStr( ) );
    // sending the temporary file through the opened socket
	if( ! utilSafePutFile( *socket, filePath + "." + extension ) ){
		dprintf( D_ALWAYS, "UploadReplicaTransferer::uploadFile failed, "
                "unlinking %s.%s\n", filePath.GetCStr(), extension.GetCStr());
		FilesOperations::safeUnlinkFile( filePath.GetCStr( ), 
										 extension.GetCStr( ) );
		return TRANSFERER_FALSE;
	}
	return TRANSFERER_TRUE;
}
