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
// for copy_file
#include "util_lib_proto.h"

#include "TransfererUpload.h"
#include "FilesOperations.h"
#include "Utils.h"

//
// UploadFile class methods
//
UploadFile::UploadFile( void )
{
}

SafeFileRotator *
UploadFile::createRotator( void ) const
{
	return NULL;
}

SafeFileCopier *
UploadFile::createCopier( void ) const
{
	return new SafeFileCopier(
		m_full_path,
		ReplicationBase::getUploadExtension(),
		getpid(),
		false );
}

bool
UploadFile::copyFile( filesize_t &size ) const
{
	ASSERT( m_upload_copier );
	if ( !m_upload_copier->Copy() ) {
		dprintf( D_ALWAYS, "->Copy failed\n" );
		return false;
	}
	StatWrapper	sw;
	StatAccess	sa;
	if ( sw.Stat(m_upload_copier->getTmpFilePath()) ) {
		dprintf( D_ALWAYS,
				 "stat(%s) failed\n", m_upload_copier->getTmpFilePath() );
		return false;
	}
	sw.GetAccess( sa );
	size = sa.getSize();
	return true;
}


//
// UploadFileSet class methods
//
UploadFileSet::UploadFileSet( UploadTransferer &uploader )
: ReplicatorFileSet( ),
	m_uploader( uploader )
{
}


//
// UploadTransferer class methods
//
UploadTransferer::UploadTransferer( void )
: BaseTransferer( new UploadFileSet(*this) ),
	m_doExit( true )
{
}

bool
UploadTransferer::preInit( void )
{
    daemonCore->Register_Command(
        REPLICATION_TRANSFER_FILE, "Transfer File",
        (CommandHandlercpp) &UploadTransferer::handleTransferFile,
		"handleTransferFile", this, DAEMON );
    daemonCore->Register_Command(
        REPLICATION_TRANSFER_REQUEST, "Transferer Request",
        (CommandHandlercpp) &UploadTransferer::handleTransferRequest,
		"handleTransferRequest", this, DAEMON );
    daemonCore->Register_Command(
        REPLICATION_TRANSFER_COMPLETE, "Transferer Complete",
        (CommandHandlercpp) &UploadTransferer::handleTransferComplete,
		"handleTransferComlete", this, DAEMON );

	return ReplicationBase::preInit( );
}

// Cleanup temp files
bool
UploadTransferer::cancel( void )
{
	stopTransfer( XFER_CANCELED );
	return cleanupTempFiles( );
}

bool
UploadTransferer::cleanupTempFiles( void )
{
	UploadFileSet	*ufs = static_cast<UploadFileSet *>( m_fileset );
	return ufs->cleanupTempUploadFiles( getpid() );
}

bool
UploadTransferer::finish( void )
{
	cleanupTempFiles( );
	if ( m_doExit ) {
		DC_Exit( 0 );
	}
	return true;
}

bool
UploadTransferer::sendFileList( const Sinful &sinful )
{
	ReplicatorPeer	downloader;
	if ( !downloader.init(sinful) ) {
		dprintf( D_ALWAYS,
				 "Failed to initialize with peer %s\n", sinful.getSinful() );
		return false;
	}
	return sendFileList( downloader );
}

bool
UploadTransferer::sendFileList( const ReplicatorPeer &downloader )
{
	ClassAd	ad;
	
	if ( !initAd( ad, "Uploader" ) ) {
		dprintf( D_ALWAYS, "Failed to initialize ad\n" );
		return false;
	}
	if ( !updateAd( ad, true ) ) {
		dprintf( D_ALWAYS, "Failed to update ad\n" );
		return false;
	}
	dprintf( D_FULLDEBUG,
			 "Sending REPLICATION_FILESET_DATA to peer %s:\n",
			 downloader.getSinfulStr() );
	ad.dPrint( D_FULLDEBUG );
	if ( !downloader.sendMessage( REPLICATION_FILESET_DATA, &ad ) ) {
		dprintf( D_ALWAYS, "Failed to send FILESET_DATA message to peer\n" );
		return false;
	}
	return true;
}

int
UploadTransferer::handleTransferFile( int command, Stream *stream )
{
	ASSERT( REPLICATION_TRANSFER_FILE == command );
	ClassAd			ad;
	ReplicatorPeer	peer;

	if ( !commonCommandHandler(command, stream, "Downloader", ad, peer)  ) {
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "REPLICATION_TRANSFER_FILE:\n" );
	ad.dPrint( D_FULLDEBUG );

	ReplicatorPeer downloader;
	if ( !downloader.init(peer) ) {
		dprintf( D_ALWAYS,
				 "Failed to initialize peer from sinful %s\n",
				 peer.getSinfulStr() );
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "Peer set to %s\n", peer.getSinfulStr() );
	if ( !sendFileList(downloader) ) {
		return FALSE;
	}
	return TRUE;
}

int
UploadTransferer::handleTransferRequest( int command, Stream *stream )
{
	ASSERT( REPLICATION_TRANSFER_REQUEST == command );
	ClassAd			ad;
	ReplicatorPeer	peer;
	MyString		tmp;

	if ( !commonCommandHandler(command, stream, "Downloader", ad, peer)  ) {
		return FALSE;
	}
	dprintf( D_FULLDEBUG, "REPLICATION_TRANSFER_REQUEST:\n" );
	ad.dPrint( D_FULLDEBUG );

	ReplicatorPeer downloader;
	if ( !downloader.init(peer) ) {
		dprintf( D_ALWAYS,
				 "Failed to initialize peer from sinful %s\n",
				 peer.getSinfulStr() );
		return FALSE;
	}

	ClassAd	sendAd;
	if ( !initAd( sendAd, "Uploader" ) ) {
		dprintf( D_ALWAYS, "Failed to initialize ad\n" );
		return false;
	}

	MyString	filename;
	if ( !ad.LookupString( ATTR_HAD_REPLICATION_FILE, filename ) ) {
		dprintf( D_ALWAYS,
				 "%s not in TRANSFER_REQUEST ad\n",
				 ATTR_HAD_REPLICATION_FILE );
		downloader.sendMessage( REPLICATION_TRANSFER_NAK, NULL );
		return false;
	}

	sendAd.Assign( ATTR_HAD_REPLICATION_FILE, filename );
	ReplicatorFile	*file = m_fileset->findFile( filename.Value() );
	if ( NULL == file ) {
		tmp.sprintf( "File %s not found in file set", filename.Value() );
		sendAd.Assign( ATTR_HAD_REPLICATION_NAK_REASON, tmp );
		downloader.sendMessage( REPLICATION_TRANSFER_NAK, &sendAd );
		tmp += "\n";
		dprintf( D_ALWAYS, tmp.Value() );
		return false;
	}
	UploadFile	*upload_file = dynamic_cast<UploadFile *>( file );
	ASSERT( upload_file );

	file->updateAd( sendAd, ATTR_HAD_REPLICATION_FILE );
	ReplicatorFile	remote_file;
	if ( !remote_file.init( ad, ATTR_HAD_REPLICATION_FILE ) ) {
		tmp = "Missing attributes in TRANSFER_REQUEST ad";
		sendAd.Assign( ATTR_HAD_REPLICATION_NAK_REASON, tmp );
		downloader.sendMessage( REPLICATION_TRANSFER_NAK, &sendAd );
		tmp += "\n";
		dprintf( D_ALWAYS, tmp.Value() );
		return false;
	}

	filesize_t	 offset;
	filesize_t	 bytes;
	const char	*filepath = NULL;
	if ( upload_file->isLogFile() ) {
		offset = remote_file.getSize();
		bytes = upload_file->getSize() - offset;
		sendAd.Assign( ATTR_HAD_REPLICATION_FILE_OFFSET, (long unsigned)offset );
		filepath = upload_file->getFullPath();
	}
	else {
		offset = 0;
		if ( !upload_file->copyFile( bytes ) ) {
			sendAd.Assign( ATTR_HAD_REPLICATION_NAK_REASON,
						   "Failed to copy file" );
			downloader.sendMessage( REPLICATION_TRANSFER_NAK, &sendAd );
			dprintf( D_ALWAYS,
					 "Failed to copy file to %s\n",
					 upload_file->getUploadTmpPath() );
			return false;
		}
		sendAd.Assign( ATTR_HAD_REPLICATION_FILE_OFFSET, -1 );
		filepath = upload_file->getUploadTmpPath();
	}

	ReliSock	sock;
	if ( !downloader.sendMessage( REPLICATION_TRANSFER_FILE_DATA,
								  &sendAd, &sock ) ) {
		dprintf( D_ALWAYS,
				 "Failed to send TRANSFER_FILE_DATA message to peer\n" );
		return false;
	}

	// Now, send the file itself...
	if ( sock.put_file( &bytes, filepath, offset ) ) {
		dprintf( D_ALWAYS, "Failed to send file %s to peer\n", filepath );
		stopTransfer( XFER_FAILED );
		return FALSE;
	}
	if ( !sock.eom() ) {
		sock.close();
	}

	return true;
}

int
UploadTransferer::handleTransferComplete( int command, Stream *stream )
{
	ASSERT( REPLICATION_TRANSFER_COMPLETE == command );
    if( !stream->end_of_message() ) {
        dprintf( D_ALWAYS,
				 "::handleTransferComplete: read EOM failed from %s\n",
				 stream->peer_description() );
    }
	return finish( ) ? TRUE : FALSE;
}


// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// c-basic-offset:4 ***
// End: ***
