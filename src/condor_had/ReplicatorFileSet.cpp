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
#include "stat_struct.h"

#include "ReplicatorFileSet.h"

using namespace std;

// ========================================
// ==== Replicator File Set base class ====
// ========================================
ReplicatorSimpleFileSet::ReplicatorSimpleFileSet( void )
		: m_file_names( NULL ),
		  m_file_names_str( NULL ),
		  m_logfile_names( NULL ),
		  m_logfile_names_str( NULL )
{
	dprintf( D_FULLDEBUG,
			 "ReplicatorSimpleFileSet ctor(void) [%p]\n", this );
}

ReplicatorSimpleFileSet::ReplicatorSimpleFileSet(
	const ReplicatorSimpleFileSet &other )
		: m_file_names( NULL ),
		  m_file_names_str( NULL ),
		  m_logfile_names( NULL ),
		  m_logfile_names_str( NULL )
{
	bool	modified;
	init( other.getFileList(), other.getLogfileList(), modified );
}

ReplicatorSimpleFileSet::~ReplicatorSimpleFileSet( void )
{
	dprintf( D_FULLDEBUG, "ReplicatorSimpleFileSet dtor [%p]\n", this );
	clear( );
}

bool
ReplicatorSimpleFileSet::init( const MyString &files,
							   const MyString &logfiles,
							   bool &modified )
{
	dprintf( D_FULLDEBUG,
			 "ReplicatorSimpleFileSet::init from MyStrings [%p]\n", this);
	StringList	file_set( files.Value() );
	StringList	log_fileset( logfiles.Value() );
	return init( file_set, log_fileset, modified );
}

bool
ReplicatorSimpleFileSet::init( const StringList &files,
							   const StringList &logfiles,
							   bool &modified )
{
	dprintf( D_FULLDEBUG,
			 "ReplicatorSimpleFileSet::init from StringLists [%p] (%p, %p)\n",
			 this, &files, &logfiles );

	// Update the "normal" file set
	if (  ( NULL == m_file_names ) ||
		  ( !m_file_names->similar(files, false) )  ) {
		modified = true;
		if ( NULL != m_file_names ) {
			delete m_file_names;
			m_file_names = NULL;
		}
		m_file_names = new StringList( files );

		if ( NULL != m_file_names_str ) {
			free( const_cast<char *>(m_file_names_str) );
			m_file_names_str = NULL;
		}
		if ( m_file_names->isEmpty() ) {
			m_file_names_str = strdup( "" );
		}
		else {
			m_file_names_str = m_file_names->print_to_string( );
		}
		ASSERT( m_file_names_str );
	}

	// Update the log (append-only) file set
	if (  ( NULL == m_logfile_names ) ||
		  ( !m_logfile_names->similar(logfiles, false) )  ) {
		modified = true;
		if ( NULL != m_logfile_names ) {
			delete m_logfile_names;
			m_logfile_names = NULL;
		}
		m_logfile_names = new StringList( logfiles );

		if ( NULL != m_logfile_names_str ) {
			free( const_cast<char *>(m_logfile_names_str) );
			m_logfile_names_str = NULL;
		}
		if ( m_logfile_names->isEmpty() ) {
			m_logfile_names_str = strdup( "" );
		}
		else {
			m_logfile_names_str = m_logfile_names->print_to_string( );
		}
		ASSERT( m_logfile_names_str );
	}
	return true;
}

bool
ReplicatorSimpleFileSet::init( const ClassAd &ad )
{
	MyString	tmp;

	dprintf( D_FULLDEBUG,
			 "ReplicatorSimpleFileSet::init from ad [%p]\n", this );

	// Decode the file name sets from the ad
	if ( !ad.LookupString( ATTR_HAD_REPLICATION_FILE_SET, tmp ) ) {
		return false;
	}
	StringList	*files = new StringList( tmp.Value() );
	if ( !ad.LookupString( ATTR_HAD_REPLICATION_LOGFILE_SET, tmp ) ) {
		delete files;
		return false;
	}
	StringList	*logfiles = new StringList( tmp.Value() );

	// Now, fill in the ad
	clear( );
	m_file_names        = files;
	m_file_names_str    = files->print_to_string( );
	m_logfile_names     = logfiles;
	m_logfile_names_str = logfiles->print_to_string( );

	return true;
}

void
ReplicatorSimpleFileSet::clear( void )
{
	dprintf( D_FULLDEBUG, "ReplicatorSimpleFileSet::clear [%p]\n", this );

	if ( m_file_names ) {
		delete m_file_names;
		m_file_names = NULL;
	}
	if ( NULL != m_file_names_str ) {
		free( const_cast<char *>(m_file_names_str) );
		m_file_names_str = NULL;
	}
	if ( m_logfile_names ) {
		delete m_logfile_names;
		m_logfile_names = NULL;
	}
	if ( NULL != m_logfile_names_str ) {
		free( const_cast<char *>(m_logfile_names_str) );
		m_logfile_names_str = NULL;
	}
}

bool
ReplicatorSimpleFileSet::sameFiles(
	const ReplicatorSimpleFileSet &other ) const
{
	if ( !m_file_names || !m_logfile_names ) {
		return false;
	}
	if ( !m_file_names->similar(other.getFileList(), false) ) {
		return false;
	}
	if ( !m_logfile_names->similar(other.getLogfileList(), false) ) {
		return false;
	}
	return true;
}

int
ReplicatorSimpleFileSet::numFileNames( void ) const
{
	int	count = 0;
	if ( m_file_names ) {
		count += m_file_names->number();
	}
	if ( m_logfile_names ) {
		count += m_logfile_names->number();
	}
	return count;
}

// ===================================
// ==== Replicator File Set class ====
// ===================================

// C-Tors / D-Tors
ReplicatorFileSet::ReplicatorFileSet( void )
		: m_base_dir( NULL ),
		  m_max_mtime( 0 ),
		  m_is_remote( false )
{
	dprintf( D_FULLDEBUG, "ReplicatorFileSet ctor(void) [%p]\n", this );
}

ReplicatorFileSet::~ReplicatorFileSet( void )
{
	dprintf( D_FULLDEBUG, "ReplicatorFileSet dtor(void) [%p]\n", this );
	if ( m_base_dir ) {
		free( const_cast<char *>(m_base_dir) );
		m_base_dir = NULL;
	}

	// Delete the list of file objects
	list <ReplicatorFile *>::iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		delete file;
	}
	m_files.clear();
}

bool
ReplicatorFileSet::init( const ClassAd &ad )
{
	dprintf( D_FULLDEBUG, "ReplicatorFileSet init from ad [%p]\n", this );
	clear( );

	m_is_remote = true;
	if ( !ReplicatorSimpleFileSet::init(ad) ) {
		return false;
	}

	MyString	tmp;
	if ( !ad.LookupString(ATTR_HAD_REPLICATION_BASE_DIRECTORY, tmp) ) {
		return false;
	}
	m_base_dir = strdup( tmp.Value() );

	int		count;
	if ( !ad.LookupInteger( ATTR_HAD_REPLICATION_FILE_COUNT, count ) ) {
		return false;
	}
	for( int n = 0;  n < count;  n++ ) {
		ReplicatorFile	*file = createFileObject( );
		MyString	attr_base;
		attr_base.sprintf( ATTR_HAD_REPLICATION_FILE_FORMAT, n );
		if ( !file->init( ad, attr_base ) ) {
			dprintf( D_ALWAYS, "Failed to decode file %d from ad\n", n );
			continue;
		}
		m_files.push_back( file );
	}
	return true;
}

bool
ReplicatorFileSet::init( const char &base_dir,
						 const StringList &files,
						 const StringList &logfiles,
						 bool &modified )
{
	dprintf( D_FULLDEBUG,
			 "ReplicatorFileSet init from StringLists [%p]\n", this );
	modified = false;
	m_is_remote = false;

	// Update the base directory
	if (  ( NULL == m_base_dir ) ||
		  ( strcmp(&base_dir, m_base_dir) )  ) {
		modified = true;
		if ( NULL != m_base_dir ) {
			free( const_cast<char *>(m_base_dir) );
			m_base_dir = NULL;
		}
		m_base_dir = strdup( &base_dir );
		if ( !m_base_dir ) {
			dprintf( D_ALWAYS,
					 "Failed to duplicate base directory '%s'\n", &base_dir );
			return false;
		}
	}

	// Initialize the file sets
	if ( !ReplicatorSimpleFileSet::init( files, logfiles, modified ) ) {
		return false;
	}
	if ( !modified ) {
		dprintf( D_FULLDEBUG, "File set unchanged\n" );
		return true;
	}

	dprintf( D_FULLDEBUG,
			 "File set modified; rebuilding file objects\n" );
	if ( !rebuild( ) ) {
		dprintf( D_ALWAYS, "Rebuild failed\n" );
		return false;
	}

	// Force checking of the files' mtimes, ignore the results
	StatStructTime	mtime;
	bool			newer;
	int				count;
	checkFiles( mtime, newer, count );

	return true;
}

bool
ReplicatorFileSet::rebuild( void )
{
	dprintf( D_FULLDEBUG,
			 "ReplicatorFileSet::rebuild [%p]\n", this );

	// First, delete all of the current file objects
	list <ReplicatorFile *>::iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		delete file;
	}
	m_files.clear();

	ListIterator<char>	siter;
	char *path;

	// Next, create a file object for each of the 'normal' data files
	siter.Initialize( m_file_names->getList() );
	siter.ToBeforeFirst( );
	while ( siter.Next( path ) ) {
		ReplicatorFile	*file = createFileObject( );
		if ( !file->init( *m_base_dir, *path, false ) ) {
			dprintf( D_ALWAYS, "Failed to initialize file '%s' '%s'\n",
					 m_base_dir, path );
			return false;
		}	
		m_files.push_back( file );
	}

	// Finally, create a file object for each of the 
	// log (append-only) data files
	siter.Initialize( m_logfile_names->getList() );
	siter.ToBeforeFirst( );
	while ( siter.Next( path ) ) {
		ReplicatorFile	*file = createFileObject( );
		if ( !file->init( *m_base_dir, *path, true ) ) {
			dprintf( D_ALWAYS,
					 "Failed to initialize log file '%s' '%s'\n",
					 m_base_dir, path );
			return false;
		}
		m_files.push_back( file );
	}
	return true;
}

bool
ReplicatorFileSet::sameFiles(
	const ReplicatorFileSet &other ) const
{
	if ( !ReplicatorSimpleFileSet::sameFiles( other ) ) {
		return false;
	}
	if ( strcmp(m_base_dir, other.getBaseDir()) == 0 ) {
		return true;
	}
	return false;
}

ReplicatorFile *
ReplicatorFileSet::findFile( const ReplicatorFile &other )
{
	list <ReplicatorFile *>::const_iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		if ( *file == other ) {
			return file;
		}
	}
	return NULL;
}

ReplicatorFile *
ReplicatorFileSet::findFile( const char *other )
{
	list <ReplicatorFile *>::const_iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		if ( *file == other ) {
			return file;
		}
	}
	return NULL;
}

const ReplicatorFile *
ReplicatorFileSet::findConstFile( const ReplicatorFile &other ) const
{
	list <ReplicatorFile *>::const_iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		const ReplicatorFile	*file = *iter;
		if ( *file == other ) {
			return file;
		}
	}
	return NULL;
}

bool
ReplicatorFileSet::installNewFiles( void ) const
{
	bool	status = true;

	list <ReplicatorFile *>::const_iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		if ( !file->installNewFile( ) ) {
			status = false;
		}
	}
	return status;
}

bool
ReplicatorFileSet::setDownloaderPid( int pid )
{
	bool	status = true;

	list <ReplicatorFile *>::const_iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		if ( !file->setDownloaderPid( pid ) ) {
			status = false;
		}
	}
	return status;
}

bool
ReplicatorFileSet::cleanupTempDownloadFiles( void ) const
{
	bool	status = true;

	list <ReplicatorFile *>::const_iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		if ( !file->cleanupTempDownloadFiles( ) ) {
			status = false;
		}
	}
	return status;
}

bool
ReplicatorFileSet::cleanupTempUploadFiles( int pid ) const
{
	bool	status = true;

	list <ReplicatorFile *>::const_iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;
		if ( !file->cleanupTempUploadFiles( pid ) ) {
			status = false;
		}
	}
	return status;
}

bool
ReplicatorFileSet::checkFiles( StatStructTime &max_mtime,
							   bool &newer,
							   int &num_mtimes )
{
	bool	status = true;

	num_mtimes = 0;
	newer = false;
	max_mtime = 0;
	list <ReplicatorFile *>::iterator iter;
	for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
		ReplicatorFile	*file = *iter;

		StatStructTime	mtime;
		bool			tnewer;
		if ( !file->checkFile( mtime, tnewer ) ) {
			dprintf( D_ALWAYS, "Warning: Failed to get mtime for %s\n",
					 file->getFullPath() );
			continue;
		}
		num_mtimes++;
		if ( mtime > max_mtime ) {
			max_mtime = mtime;
			newer = true;
		}
		else if ( tnewer ) {
			newer = true;
		}
	}
	if ( num_mtimes ) {
		m_max_mtime = max_mtime;
	}
	return status;
}

bool
ReplicatorFileSet::updateAd( ClassAd &ad, bool details ) const
{
	dprintf( D_FULLDEBUG,
			 "ReplicatorFileSet::updateAd [%p]\n", this );

	// Base directory
	ad.Assign( ATTR_HAD_REPLICATION_BASE_DIRECTORY, m_base_dir );
	ad.Assign( ATTR_HAD_REPLICATION_FILE_SET, m_file_names_str );
	ad.Assign( ATTR_HAD_REPLICATION_LOGFILE_SET, m_logfile_names_str );
	ad.Assign( ATTR_HAD_REPLICATION_MTIME, m_max_mtime );

	if ( details ) {
		int		n = 0;
		list <ReplicatorFile *>::const_iterator iter;
		for( iter = m_files.begin(); iter != m_files.end(); iter++ ) {
			const ReplicatorFile	*file = *iter;
			MyString				attr_base;
			attr_base.sprintf( ATTR_HAD_REPLICATION_FILE_FORMAT, n++ );
			file->updateAd( ad, attr_base );
		}
	}
	ad.Assign( ATTR_HAD_REPLICATION_FILE_COUNT, m_files.size() );

	if ( DebugFlags & D_FULLDEBUG ) {
		MyString	s;
		ad.sPrint(s);
		dprintf( D_FULLDEBUG, "FileSet::updateAd =\n%s\n", s.Value() );
	}
	return true;
}

bool
ReplicatorFileSet::publishFileDetails( ClassAd &ad, unsigned fileno ) const
{
	if ( fileno >= m_files.size() ) {
		return false;
	}

	unsigned		n;
	list <ReplicatorFile *>::const_iterator iter;
	for( n = 0, iter = m_files.begin(); iter != m_files.end(); iter++, n++ ) {
		if ( n == fileno ) {
			const ReplicatorFile	*file = *iter;
			if ( !file->updateAd(ad, ATTR_HAD_REPLICATION_FILE) ) {
				return false;
			}
			break;
		}
	}
	return true;
}
