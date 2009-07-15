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

#include "ReplicatorFileSet.h"

using namespace std;


// ===================================
// ==== Replicator File Set class ====
// ===================================

// C-Tors / D-Tors
ReplicatorFileSet::ReplicatorFileSet( const char *spool, StringList *paths )
		: ReplicatorFileBase( spool ),
		  m_nameList( NULL ),
		  m_nameListStr( NULL ),
		  m_pathList( paths ),
		  m_pathListStr( paths->print_to_string() )
{
	setNamesFromPaths( paths );
}

ReplicatorFileSet::ReplicatorFileSet( void )
		: ReplicatorFileBase( ),
		  m_nameList( NULL ),
		  m_nameListStr( NULL ),
		  m_pathList( NULL ),
		  m_pathListStr( NULL )
{
}

void
ReplicatorFileSet::setPaths( StringList *paths )
{
	m_pathList = paths;
	m_pathListStr = paths->print_to_string();
	setNamesFromPaths( paths );
}

void
ReplicatorFileSet::setNamesFromPaths( StringList *paths )
{
	char	*path;

    paths->rewind( );
	m_nameList = new StringList;
    while( (path = paths->next()) != NULL ) {
		m_nameList->append( condor_basename(path) );
	}
	m_nameListStr = m_nameList->print_to_string();
}

void
ReplicatorFileSet::setNames( StringList *names )
{
	m_nameList = names;
	m_nameListStr = m_nameList->print_to_string();
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
