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
#include "ReplicatorFileList.h"

using namespace std;

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

bool
ReplicatorFileList::sendCommand(int command,
								bool send_ad,
								const ReplicatorPeer &peer,
								int &total_errors )
{
	total_errors = 0;

	bool	status = true;
	list <ReplicatorFile *>::iterator iter;
	for( iter = m_fileList.begin(); iter != m_fileList.end(); iter++ ) {
		ReplicatorFile	*f = *iter;
		int		errors = 0;
		if ( !f->sendCommand( command, send_ad, peer, errors ) ) {
			status = false;
		}
		total_errors += errors;
	}
	return status;
}

bool
ReplicatorFileList::sendCommand( int command,
								 bool send_ad,
								 int &total_errors )
{
	total_errors = 0;

	bool	status = true;
	list <ReplicatorFile *>::iterator iter;
	for( iter = m_fileList.begin(); iter != m_fileList.end(); iter++ ) {
		ReplicatorFile	*f = *iter;
		int		errors = 0;
		if ( !f->sendCommandToPeers( command, send_ad, errors ) ) {
			status = false;
		}
		total_errors += errors;
	}
	return status;
}
