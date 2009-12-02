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
#include "daemon.h"
#include "daemon_types.h"
#include "internet.h"
#include "FilesOperations.h"

#include "Utils.h"
#include "TransfererDownload.h"
	

//
// DownloadFile class methods
//
DownloadFile::DownloadFile( void )
		: ReplicatorFile( ),
		  m_done( false ),
		  m_remote( NULL ),
		  m_offset( 0 ),
		  m_append( false ),
		  m_skipped( false )
{
}

bool
DownloadFile::init( const ReplicatorFile *remote )
{
	m_remote = remote;
	m_done = false;
	m_offset = 0;
	m_append = false;
	m_skipped = false;
	return true;
}

SafeFileRotator *
DownloadFile::createRotator( void ) const
{
	return new SafeFileRotator(
		m_full_path,
		ReplicationBase::getUploadExtension(),
		getpid(),
		false );
}

SafeFileCopier *
DownloadFile::createCopier( void ) const
{
	return NULL;
}

//
// DownloadFileSet class methods
//
DownloadFileSet::DownloadFileSet( DownloadTransferer &downloader )
: ReplicatorFileSet( ),
	m_dlist( NULL ),
	m_downloader( downloader ),
	m_remoteFileset( NULL )
{
	m_dlist = (list<DownloadFile*> *)(&m_files);
}

DownloadFileSet::~DownloadFileSet( void )
{
	if ( m_remoteFileset ) {
		delete m_remoteFileset;
		m_remoteFileset = NULL;
	}
}

bool
DownloadFileSet::startDownload( Stream* /*stream*/,
								const ReplicatorFileSet *rset )
{
	list <DownloadFile *>::iterator iter;
	for( iter = m_dlist->begin(); iter != m_dlist->end(); iter++ ) {
		DownloadFile			*file = *iter;
		const ReplicatorFile	*rfile = rset->findConstFile( *file );
		if ( !rfile ) {
			dprintf( D_ALWAYS,
					 "Can't find file %s in list from uploader <aborting>",
					 file->getRelPath() );
			return false;
		}
		file->init( rfile );
	}
	m_downloader.reset( BaseTransferer::XFER_TRANSFERING );
	m_remoteFileset = rset;

	return startNextFile( );
}

bool
DownloadFileSet::startNextFile( void )
{
	dprintf( D_FULLDEBUG, "Looking for next file to transfer\n" );
	list <DownloadFile *>::iterator iter;
	for( iter = m_dlist->begin(); iter != m_dlist->end(); iter++ ) {
		DownloadFile	*file = *iter;
		if ( !m_downloader.isTransfering( ) ) {
			dprintf( D_ALWAYS, "Download %s\n", m_downloader.getStatusStr() );
			return false;
		}
		if ( file->isDone() ) {
			continue;
		}
		bool started;
		if ( !m_downloader.downloadFile( file, started ) ) {
			dprintf( D_ALWAYS,
					 "Failed to start download of file %s\n",
					 file->getRelPath() );
			return false;
		}
		if ( started ) {

			dprintf( D_FULLDEBUG,
					 "Initiated download of file %s\n",
					 file->getRelPath() );
			return true;
		}
		else {
			dprintf( D_FULLDEBUG,
					 "No need to download file %s\n",
					 file->getRelPath() );
			continue;
		}
	}
	dprintf( D_FULLDEBUG, "Done with all downloads\n" );
	m_downloader.stopTransfer( BaseTransferer::XFER_COMPLETE );
	return true;
}


//
// DownloadTransferer class methods
//
DownloadTransferer::DownloadTransferer( const Sinful &uploader )
: BaseTransferer( new DownloadFileSet(*this) ),
	m_uploader( new ReplicatorPeer ),
	m_current( NULL ),
	m_enable_rotate( false )
{
	m_uploader->init( uploader );
}

DownloadTransferer::DownloadTransferer( void )
: BaseTransferer( new DownloadFileSet(*this) ),
	m_dfs( NULL ),
	m_uploader( NULL ),
	m_current( NULL )
{
	m_dfs = static_cast<DownloadFileSet *>( m_fileset );
}

DownloadTransferer::~DownloadTransferer( void )
{
	if ( m_uploader ) {
		delete m_uploader;
		m_uploader = NULL;
	}
}

// Initialize the object
bool
DownloadTransferer::preInit( void )
{
    daemonCore->Register_Command(
        REPLICATION_FILESET_DATA, "Fileset Data",
        (CommandHandlercpp) &DownloadTransferer::handleFilesetData,
		"handleFilesetData", this, DAEMON );
    daemonCore->Register_Command(
        REPLICATION_TRANSFER_FILE_DATA, "Transferer File Data",
        (CommandHandlercpp) &DownloadTransferer::handleTransferFileData,
		"handleTransfererFileData", this, DAEMON );
    daemonCore->Register_Command(
        REPLICATION_TRANSFER_NAK, "Transferer NAK",
        (CommandHandlercpp) &DownloadTransferer::handleTransferNak,
		"handleTransfererNak", this, DAEMON );

	return ReplicationBase::preInit( );
}


// Cleanup temp files
bool
DownloadTransferer::cancel( void )
{
	stopTransfer( XFER_CANCELED );
	return cleanupTempFiles( );
}

bool
DownloadTransferer::cleanupTempFiles( void )
{
	return m_dfs->cleanupTempDownloadFiles( );
}

bool
DownloadTransferer::finish( void )
{
	if ( m_enable_rotate ) {
		if ( !m_dfs->installNewFiles() ) {
			dprintf( D_ALWAYS, "Failed to install new files\n" );
			DC_Exit( 1 );
			return false;
		}
	}
	dprintf( D_FULLDEBUG, "Sending transfer complete to uploader\n" );
	if ( !m_uploader->sendMessage( REPLICATION_TRANSFER_COMPLETE, NULL ) ) {
		dprintf( D_ALWAYS, "Failed to send transfer complete to uploader\n" );
		return false;
	}
	DC_Exit( 0 );
	return true;
}


/* Function    : transferFileCommand
 * Return value: true  - upon success,
 *               false - upon failure
 * Description : sends a transfer command to the remote replication daemon,
 *               which creates a uploading 'condor_transferer' process
 * Notes       : sends to the replication daemon a port number, on which it
 *               will be listening to the files uploading requests
 */
bool
DownloadTransferer::contactPeerReplicator( const Sinful &sinful )
{
	ClassAd		ad;

	ReplicatorPeer	replicator;
	if ( !replicator.init(sinful) ) {
		dprintf( D_ALWAYS, "Failed to initialize peer replicator\n" );
		return false;
	}
	if ( !initAd( ad, "Downloader" ) ) {
		dprintf( D_ALWAYS, "Failed to initialize ad\n" );
		return false;
	}
	if ( !updateAd( ad, true ) ) {
		dprintf( D_ALWAYS, "Failed to update ad\n" );
		return false;
	}
	return replicator.sendMessage( REPLICATION_TRANSFER_FILE, &ad );
}

/* Function   : handleFilesetData
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: handles various commands sent to this replication daemon
 *				from the HAD
 */
int
DownloadTransferer::handleFilesetData( int command, Stream *stream )
{
	ASSERT( REPLICATION_FILESET_DATA == command );
	ClassAd			ad;
	ReplicatorPeer	peer;

	if ( !commonCommandHandler(command, stream, "Uploader", ad, peer)  ) {
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "REPLICATION_FILESET_DATA:\n" );
	ad.dPrint( D_FULLDEBUG );

	if ( m_uploader ) {
		delete m_uploader;
		m_uploader = NULL;
	}
	m_uploader = new ReplicatorPeer( peer );

	ReplicatorFileSet	*fileset = new ReplicatorFileSet;
	if ( !fileset->init(ad) ) {
		dprintf( D_ALWAYS,
				 "Peer message %d from %s missing fileset info\n",
				 command, peer.getSinfulStr( ) );
		return FALSE;
	}
	if ( !sameFiles(*fileset, peer) ) {
		return FALSE;
	}

	bool status = m_dfs->startDownload( stream, fileset );
	if ( isDone() ) {
		(void)finish();
	}
	return status;
}

/* Function   : handleTransferFile
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: handle TRANSFER_FILE_DATA command from peer
 */
int
DownloadTransferer::handleTransferFileData( int command, Stream *stream )
{
	ASSERT( REPLICATION_TRANSFER_FILE_DATA == command );
	ClassAd			 ad;
	ReplicatorPeer	 peer;

	if ( !commonCommandHandler(command, stream, "Uploader", ad, peer, false)) {
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "REPLICATION_TRANSFER_FILE_DATA:\n" );
	ad.dPrint( D_FULLDEBUG );

	ASSERT( NULL != m_uploader );
	if ( *m_uploader != peer ) {
		dprintf( D_ALWAYS,
				 "Got TRANSFER_FILE_DATA from unexpected host %s\n",
				 peer.getSinfulStr() );
		stopTransfer( XFER_FAILED );
		return FALSE;
	}

	ReplicatorFile	rfile;
	if ( !rfile.init( ad, ATTR_HAD_REPLICATION_FILE ) ) {
		dprintf( D_ALWAYS, "Failed to initialize from remote ad\n" );
		stopTransfer( XFER_FAILED );
		return FALSE;
	}
	else if ( *m_current != rfile ) {
		dprintf( D_ALWAYS,
				 "Got FILE_DATA for unexpected file %s (expecting %s)\n",
				 rfile.getRelPath(), m_current->getRelPath() );
		stopTransfer( XFER_FAILED );
		return FALSE;
	}

	int			tmpi;
	if ( !ad.LookupInteger( ATTR_HAD_REPLICATION_FILE_OFFSET, tmpi ) ) {
		dprintf( D_ALWAYS, "No file offset in TRANSFER_FILE_DATA ad\n" );
		return FALSE;
	}
	else if ( m_current->isAppend() && (m_current->getOffset() != tmpi) ) {
		dprintf( D_ALWAYS,
				 "TRANSFER_FILE_DATA with wrong offset %d (expecting %ld)\n",
				 tmpi, (unsigned long) m_current->getOffset() );
		stopTransfer( XFER_FAILED );
		return FALSE;
	}

	if( stream->type() != Stream::reli_sock ) {
		dprintf( D_ALWAYS, "Stream is not a reli-sock!\n");
		return FALSE;
	}
	ReliSock	*sock = (ReliSock*)stream;
	const char	*filepath = ( m_current->isAppend() ?
							  m_current->getFullPath() :
							  m_current->getDownloadTmpPath() );
	filesize_t	 actual;

	if ( sock->get_file( &actual, filepath, false, m_current->isAppend() ) ) {
		dprintf( D_ALWAYS,
				 "Failed to get file %s from peer\n", filepath );
		stopTransfer( XFER_FAILED );
		return FALSE;
	}
	struct utimbuf	ut = { rfile.getMtime(), rfile.getMtime() };
	utime( filepath, &ut );

	// Read the EOM
    if( !stream->end_of_message() ) {
        dprintf( D_ALWAYS, "read EOM failed\n" );
		return FALSE;
    }

	// Done.  :)
	m_current->setDone( );
	int status = m_dfs->startNextFile( ) ? TRUE : FALSE;
	if ( isDone() ) {
		dprintf( D_FULLDEBUG, "Done downloading all files\n" );
		return finish( );
	}
	return status;
}

/* Function   : handleTransferNak
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: handle TRANSFER_NAK command from peer
 */
int
DownloadTransferer::handleTransferNak( int command, Stream *stream )
{
	ASSERT( REPLICATION_TRANSFER_NAK == command );

	ClassAd			 ad;
	ReplicatorPeer	 peer;

	if ( !commonCommandHandler(command, stream, "Uploader", ad, peer)  ) {
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "REPLICATION_TRANSFER_NAK:\n" );
	ad.dPrint( D_FULLDEBUG );

	MyString	tmp = "Unknown";
	ad.LookupString( ATTR_HAD_REPLICATION_NAK_REASON, tmp );
	dprintf( D_ALWAYS, "Got NAK (%s) from peer\n", tmp.Value() );

	stopTransfer( XFER_FAILED );
	DC_Exit( 1 );

	return FALSE;
}

/* Function    : downloadFile
 * Arguments   : filePath   - a name of the downloaded file
 *				 extension  - extension of temporary file
 * Return value: true  - upon success,
 *               false - upon failure
 * Description : downloads 'filePath' from the uploading 'condor_transferer'
 *               process
 */
bool
DownloadTransferer::downloadFile( DownloadFile *file, bool &started )
{
	const ReplicatorFile *remote = file->getRemote();
	started = false;

    dprintf( D_FULLDEBUG,
			 "DownloadTransferer::downloadFile '%s' %ld<->%ld %ld<->%ld\n",
			 file->getRelPath(),
			 remote->getSize(), file->getSize(),
			 remote->getMtime(), file->getMtime() );

	if (  ( remote->getSize()  == file->getSize()  )  &&
		  ( remote->getMtime() == file->getMtime() )   ) {
		file->skip();
		return true;
	}

	ClassAd	ad;
	if ( !initAd( ad, "Downloader" ) ) {
		dprintf( D_ALWAYS, "Failed to initialize peer ad\n" );
		return false;
	}
	ad.Assign( ATTR_HAD_REPLICATION_FILE, file->getRelPath() );
	file->updateAd( ad, ATTR_HAD_REPLICATION_FILE );

	bool append = file->isLogFile();
	if ( append ) {
		if (  ( 0 == file->getSize() ) ||
			  ( remote->getSize() < file->getSize() )  ) {
			append = false;
		}
	}
	file->setAppend( append );

	if ( append ) {
		ad.Assign( ATTR_HAD_REPLICATION_FILE_OFFSET, file->getSize() );
		file->setOffset( file->getSize() );
	}
	else {
		ad.Assign( ATTR_HAD_REPLICATION_FILE_OFFSET, -1 );
		file->setOffset( 0 );
	}
	if ( !m_uploader->sendMessage( REPLICATION_TRANSFER_REQUEST, &ad ) ) {
		dprintf( D_ALWAYS, "Failed to send transfer request to uploader\n" );
		return false;
	}
	m_current = file;
	started = true;

	return true;
}

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// c-basic-offset:4 ***
// End: ***
