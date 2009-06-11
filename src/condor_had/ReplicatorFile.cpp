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
#include "stat_wrapper.h"

#include "ReplicatorFile.h"
#include "ReplicatorFileReplica.h"
#include "FilesOperations.h"

using namespace std;


// ==========================================
// === Replicator File Base class methods ===
// ==========================================

// C-Tors / D-Tors
ReplicatorFileBase::ReplicatorFileBase( const char *spool, const char *path )
		: m_versionFilePath( "" ),
		  m_myVersion( *this ),
		  m_downloader( *this )
{
	m_versionFilePath  = spool;
	m_versionFilePath += "/_Version.";
	m_versionFilePath = condor_basename( path );

    m_classAd.SetMyTypeName( REPLICATOR_ADTYPE );
    m_classAd.SetTargetTypeName( "" );

	//m_classAd.Assign( );
}

ReplicatorFileBase::ReplicatorFileBase( const char * /*path*/ )
		: m_versionFilePath( "" ),
		  m_myVersion( *this ),
		  m_downloader( *this )
{
    m_classAd.SetMyTypeName( REPLICATOR_ADTYPE );
    m_classAd.SetTargetTypeName( "" );

	//m_classAd.Assign( );
}

ReplicatorFileBase::~ReplicatorFileBase( void )
{
}

bool
ReplicatorFileBase::setPeers( const ReplicatorPeerList &peers )
{
	list <ReplicatorPeer *>::const_iterator iter;
	for( iter = peers.getPeersConst().begin();
		 iter != peers.getPeersConst().end();
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
	ReplicatorTransfererList &transferers ) const
{
	bool	status = true;
	list <ReplicatorFileReplica *>::const_iterator iter;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		const ReplicatorFileReplica *replica = *iter;
		if ( !replica->registerUploaders(transferers) ) {
			status = false;
		}
	}
	return status;
}

bool
ReplicatorFileBase::registerDownloaders(
	ReplicatorTransfererList &transferers ) const
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
		if ( replica->getUploader().isActive() ) {
			num++;
		}
	}
	return num;
};

// Replica object operators
bool
ReplicatorFileBase::findReplica( const char *hostname,
								 const ReplicatorFileReplica *&result ) const
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
ReplicatorFileBase::hasReplica( const ReplicatorFileReplica &replica ) const
{
	list <ReplicatorFileReplica *>::const_iterator iter;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		const ReplicatorFileReplica	*tmp = *iter;
		if ( tmp->isSameHost( replica ) ) {
			return true;
		}
	}
	return false;
}

bool
ReplicatorFileBase::addReplica( ReplicatorFileReplica *new_replica )
{
	list <ReplicatorFileReplica *>::iterator iter;
	ReplicatorFileReplica *replace = NULL;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		ReplicatorFileReplica	*replica = *iter;
		if ( replica->isSameHost( *new_replica ) ) {
			replace = replica;
			break;
		}
	}
	if ( NULL != replace ) {
		m_replicaList.remove( replace );
		delete replace;
	}
	m_replicaList.push_back( new_replica );
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
ReplicatorFileBase::getFileMtime( const char *path, time_t &mtime )
{
	StatWrapper	swrap;
	if ( swrap.Stat(path) ) {
		return false;
	}
	StatStructType	data;
	if ( swrap.GetBuf(data) ) {
		return false;
	}
	mtime = data.st_mtime;
	return true;
}

bool
ReplicatorFileBase::sendMessage(
	int		 command,
	bool	 send_ad,
	const ReplicatorPeer &peer,
	int		&errors )
{
	return sendMessage( command, (send_ad ? &m_classAd : NULL), peer, errors );
}

bool
ReplicatorFileBase::sendMessage(
	int		 command,
	bool	 send_ad,
	int		&total_errors )
{
	total_errors = 0;

	list <ReplicatorFileReplica *>::iterator iter;
	for( iter = m_replicaList.begin(); iter != m_replicaList.end(); iter++ ) {
		ReplicatorFileReplica	*replica = *iter;
		const ReplicatorPeer	&peer = replica->getPeerInfo( );

		int		errors = 0;
		sendMessage( command, (send_ad ? &m_classAd : NULL), peer, errors );
		total_errors += errors;
	}

	return total_errors == 0 ? true : false ;
}

// Internal sendMessage method
bool
ReplicatorFileBase::sendMessage(
	int						 command,
	const ClassAd			*ad,
	const ReplicatorPeer	&peer,
	int						&total_errors )
{
	total_errors = 0;
	bool	status = true;

	// Send the message
	if ( !peer.sendMessage( command, ad ) ) {
		total_errors++;
	}

	return status;
}


// ===============================
// ==== Replicator File class ====
// ===============================

// C-Tors / D-Tors
ReplicatorFile::ReplicatorFile( const char *spool, const char *path )
		: ReplicatorFileBase( spool, path ),
		  m_filePath( path )
{
	// m_classAd.Assign( ); TODO
}

ReplicatorFile::~ReplicatorFile( void )
{
}

// Comparison operators
bool
ReplicatorFile::operator == ( const ReplicatorFileBase &other ) const
{
	const ReplicatorFile *file =
		dynamic_cast<const ReplicatorFile*>( &other );
	if ( !file ) {
		return false;
	}
	return *this == *file;
}

bool
ReplicatorFile::getMtime( time_t &mtime ) const
{
	return getFileMtime( m_filePath.Value(), mtime );
}


// ===================================
// ==== Replicator File Set class ====
// ===================================

// C-Tors / D-Tors
ReplicatorFileSet::ReplicatorFileSet( const char *spool, StringList *files )
		: ReplicatorFileBase( spool, files->first() ),
		  m_nameList( NULL ),
		  m_nameListStr( NULL ),
		  m_pathList( files ),
		  m_pathListStr( files->print_to_string() )
{

	char	*path;

    files->rewind( );
	m_nameList = new StringList;
    while( (path = files->next()) != NULL ) {
		m_nameList->append( condor_basename(path) );
	}
	m_nameListStr = m_nameList->print_to_string();
}

ReplicatorFileSet::ReplicatorFileSet( StringList *names )
		: ReplicatorFileBase( names->first() ),
		  m_nameList( names ),
		  m_nameListStr( names->print_to_string() ),
		  m_pathList( NULL ),
		  m_pathListStr( NULL )
{
}

ReplicatorFileSet::~ReplicatorFileSet( void )
{

	if ( m_nameList ) {
		delete m_nameList;
		m_nameList = NULL;
	}
	if ( m_nameListStr ) {
		free( m_nameListStr );
		m_nameListStr = NULL;
	}

	if ( m_pathList ) {
		delete m_pathList;
		m_pathList = NULL;
	}
	if ( m_pathListStr ) {
		free( m_pathListStr );
		m_pathListStr = NULL;
	}

}

bool
ReplicatorFileSet::operator == ( const ReplicatorFileBase &other ) const
{
	const ReplicatorFileSet	*file_set =
		dynamic_cast<const ReplicatorFileSet*>( &other );
	if ( !file_set ) {
		return false;
	}
	return *this == *file_set;
}

bool
ReplicatorFileSet::getMtime( time_t &mtime ) const
{
	mtime = 0;

	if ( !m_pathList ) {
		return false;
	}

	char	*path;
	m_pathList->rewind();
	while(  ( path = m_pathList->next() ) != NULL ) {
		time_t	ttime;
		if ( getFileMtime( path, ttime ) ) {
			if ( ttime > mtime ) {
				mtime = ttime;
			}
		}
		else {
			dprintf( D_FULLDEBUG,
					 "Warning: Unable to stat '%s' for replication\n", path );
		}
	}
	return mtime ? true : false;
}
