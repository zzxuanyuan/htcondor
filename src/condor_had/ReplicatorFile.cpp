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

#include "condor_daemon_core.h"
#include "condor_config.h"
#include "basename.h"

#include "ReplicatorFile.h"
#include "ReplicatorFileReplica.h"

using namespace std;

// === ReplicatorFileBase methods ===

// C-Tors / D-Tors
ReplicatorFileBase::ReplicatorFileBase( const char *spool, const char *path )
		: m_filePath( path ),
		  m_versionFilePath( "" ),
		  m_myVersion( *this ),
		  m_downloadProcessData( *this )
{
	m_versionFilePath  = spool;
	m_versionFilePath += "/_Version.";
	m_versionFilePath = condor_basename( path );

    m_classAd.SetMyTypeName( REPLICATOR_ADTYPE );
    m_classAd.SetTargetTypeName( "" );

	m_classAd.Assign( );
}

ReplicatorFileBase::~ReplicatorFileBase( void )
{
}

bool
ReplicatorFileBase::setPeers( const ReplicatorPeerList &peers )
{
	list <const ReplicatorPeer *>::const_iterator iter;
	for( iter = peers.getPeers().begin();
		 iter != peers.getPeers().end();
		 iter++ ) {
		const ReplicatorPeer	*peer = *iter;
		ReplicatorFileReplica	*replica =
			new ReplicatorFileReplica( *this, *peer );
		addReplica( replica );
	}
	return true;
}

bool
ReplicatorFileBase::registerUploaders(
	ReplicatorTransferList &transferers ) const
{
	bool	status = true;
	list <const ReplicatorFileReplica *>::const_iterator iter;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end();  iter++ ){
		const ReplicatorFileReplica *replica = *iter;
		if ( !replica->registerUploaders(transferers) ) {
			status = false;
		}
	}
	return status;
}

bool
ReplicatorFileBase::registerDownloaders(
	ReplicatorTransferList &transferers ) const
{
	return transferers.Register( m_downloader );
}

int
ReplicatorFileBase::numActiveUploads( void ) const
{
	int		num = 0;
	list <ReplicatorFileReplica *>::const_iterator iter;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		const ReplicatorFileReplica	*replica = *iter;
		if ( replica->getDownloader.isActive() ) {
			num++;
		}
	}
	return num;
};

// Replica object operators
bool
ReplicatorFileBase::findVersion( const char *hostname,
								 const ReplicatorFileVersion *&result ) const
{
	list <ReplicatorFileReplica *>::const_iterator iter;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		const ReplicatorFileReplica	*replica = *iter;
		if ( replica->isSameHost( hostname ) ) {
			result = replica;
			return true;
		}
	}
	return false;
}

bool
ReplicatorFile::hasVersion( const ReplicatorFileVersion &version ) const
{
	list <ReplicatorFileReplica *>::const_iterator iter;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		const ReplicatorFileReplica	*replica = *iter;
		if ( replica->isSameHost( version ) ) {
			return true;
		}
	}
	return false;
}

bool
ReplicatorFile::updateVersion ( ReplicatorFileVersion &new_version )
{
	list <ReplicatorFileReplica *>::iterator iter;
	ReplicatorFileReplica *replace = NULL;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		ReplicatorFileReplica	*replica = *iter;
		if ( replica->isSameHost( new_version ) ) {
			replace = replica;
			break;
		}
	}
	if ( NULL != replace ) {
		m_replicaList.remove( replace );
		delete replace;
	}
	m_replicaList.push_back( &new_version );
	return false;
}

bool
ReplicatorFile::rotateFile ( int pid ) const
{
    MyString tmp( pid );
    tmp += ".";
    tmp += DOWNLOADING_TEMPORARY_FILES_EXTENSION;

    // the rotation and the version synchronization appear in the code
    // sequentially, trying to make the gap between them as less as possible;
    // upon failure we do not synchronize the local version, since such
	// downloading is considered invalid
	if(  !FilesOperations::safeRotateFile(
			 m_versionFilePath.Value(), tmp.Value())  ) {
		dprintf( D_ALWAYS, "Failed to rotate %s with %s\n",
				 m_versionFilePath.Value(), tmp.Value() );
		return false;
	}
	if ( !FilesOperations::safeRotateFile(
			 m_filePath.Value(), tmp.Value())  ) {
		dprintf( D_ALWAYS, "Failed to rotate %s with %s\n",
				 m_filePath.Value(), tmp.Value() );
		return false;
	}
	return true;
}

bool
ReplicatorFileBase::sendCommand(
	int						 command,
	bool					 send_ad,
	const ReplicatorPeer	&peer,
	int						&total_errors )
{
	return sendMessage( command,
						send_ad ? m_classAd : NULL,
						peer,
						total_errors );
}

bool
ReplicatorFileBase::sendCommand(
	int		 command,
	bool	 send_ad,
	int		&total_errors )
{
	total_errors = 0;

	list <ReplicatordFileReplica *>::iterator iter;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		ReplicatorFileReplica	*replica = *iter;
		ReplicatorPeer			*peer = replica->getPeerInfo( );

		int		errors = 0;
		sendMessage( command, send_ad ? m_classAd : NULL, peer, errors );
		total_errors += errors;
	}

	return total_errors == 0 : true : false;
}

// Internal sendCommand method
bool
ReplicatorFileBase::sendCommand(
	int						 command,
	const ClassAd			*ad,
	const ReplicatorPeer	&peer,
	int						&total_errors )
{
	total_errors = 0;
	bool	status = true;

	// Send the message
	if ( !peer->sendMessage( command, ad, m_classAd ) ) {
		total_errors++;
	}

	return status;
}


// ==== Replicator File class ====

// C-Tors / D-Tors
ReplicatorFile::ReplicatorFile( const char *spool, const char *path )
		: ReplicatorFileBase( spool, path ),
		  m_filePath( path )
{
	m_classAd.Assign( );
}

ReplicatorFile::~ReplicatorFile( void )
{
}

// Comparison operators
bool
ReplicatorFile::operator == ( const ReplicatorFileBase &other ) const
{
	ReplicatorFile	*file = dynamic_cast<ReplicatorFile*>( &other );
	if ( !file ) {
		return false;
	}
	return *this == *file;
}


// ==== Replicator File Set class ====

// C-Tors / D-Tors
ReplicatorFileSet::ReplicatorFileSet( const char *spool, StringList *files )
		: ReplicatorFileBase( spool, files.first() ),
		  m_fileList( files ),
		  m_fileListStr( files.print_to_string() )
{
}

ReplicatorFileSet::~ReplicatorFileSet( void )
{
	if ( m_fileList ) {
		delete m_fileList;
	}
	if ( m_fileListStr ) {
		free( m_fileListStr );
	}
}

bool
ReplicatorFileSet::operator == ( const ReplicatorFileBase &other ) const
{
	ReplicatorFileSet	*file_set = dynamic_cast<ReplicatorFileSet*>( &other );
	if ( !file_set ) {
		return false;
	}
	return *this == *file_set;
}
