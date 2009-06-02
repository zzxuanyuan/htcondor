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

#include "condor_daemon_core.h"
#include "condor_config.h"
#include "basename.h"

#include "ReplicatorFile.h"
#include "ReplicatorFileVersion.h"

using namespace std;

// C-Tors / D-Tors
ReplicatorFile::ReplicatorFile( const char *spool, const char *path )
		: m_filePath( path ),
		  m_versionFilePath( "" ),
		  m_myVersion( *this ),
		  m_downloadProcessData( *this )
{
	m_versionFilePath  = spool;
	m_versionFilePath += "/_Version.";
	m_versionFilePath = condor_basename( path );
}

ReplicatorFile::~ReplicatorFile( void )
{
}

// Comparison operators
bool
ReplicatorFile::operator == ( const ReplicatorFile &other ) const
{
	return m_filePath == other.getFilePath( );
}

bool
ReplicatorFile::operator == ( const char *file_path ) const
{
	return m_filePath == file_path;
}

// Version object operators
bool
ReplicatorFile::findVersion( const char *hostname,
							 const ReplicatorFileVersion *&result ) const
{
	list <ReplicatorFileVersion *>::const_iterator iter;
	for( iter = m_versionList.begin(); iter != m_versionList.end(); iter++ ) {
		const ReplicatorFileVersion	*ver = *iter;
		if ( ver->isSameHost( hostname ) ) {
			result = ver;
			return true;
		}
	}
	return false;
}

bool
ReplicatorFile::hasVersion( const ReplicatorFileVersion &other ) const
{
	list <ReplicatorFileVersion *>::const_iterator iter;
	for( iter = m_versionList.begin(); iter != m_versionList.end(); iter++ ) {
		const ReplicatorFileVersion	*ver = *iter;
		if ( ver->isSameHost( other ) ) {
			return true;
		}
	}
	return false;
}

bool
ReplicatorFile::updateVersion ( ReplicatorFileVersion &new_version )
{
	list <ReplicatorFileVersion *>::iterator iter;
	ReplicatorFileVersion *replace = NULL;
	for( iter = m_versionList.begin(); iter != m_versionList.end(); iter++ ) {
		ReplicatorFileVersion	*ver = *iter;
		if ( ver->isSameHost( new_version ) ) {
			replace = ver;
			break;
		}
	}
	if ( NULL != replace ) {
		m_versionList.remove( replace );
		delete replace;
	}
	m_versionList.push_back( &new_version );
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

// ==== Replicator File List class ====

ReplicatorFileList::ReplicatorFileList( void )
{
}

ReplicatorFileList::~ReplicatorFileList( void )
{
	list <ReplicatorFile *>::iterator iter;
	for( iter = m_fileList.begin(); iter != m_fileList.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		delete file;
	}
	m_fileList.clear( );
}

bool
ReplicatorFileList::registerFile( ReplicatorFile *file )
{
	if ( hasFile(file) ) {
		return false;
	}
	m_fileList.push_back( file );
	return true;
}

bool
ReplicatorFileList::hasFile( const ReplicatorFile *file ) const
{
	list <ReplicatorFile *>::const_iterator iter;
	for( iter = m_fileList.begin(); iter != m_fileList.end(); iter++ ) {
		ReplicatorFile	*f = *iter;
		if ( f == file ) {
			return true;
		}
	}
	return false;
}

bool
ReplicatorFileList::findFile( const char *path, ReplicatorFile **file )
{
	list <ReplicatorFile *>::iterator iter;
	for( iter = m_fileList.begin(); iter != m_fileList.end(); iter++ ) {
		ReplicatorFile	*f = *iter;
		if ( *f == path ) {
			*file = f;
			return true;
		}
	}
	*file = NULL;
	return false;
}
