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
#include "stat_wrapper.h"
#include "condor_debug.h"
#include "ReplicatorFile.h"
#include "ReplicationBase.h"


// ===============================
// ==== Replicator File class ====
// ===============================
ReplicatorFile::ReplicatorFile( void )
		: m_initialized( false ),
		  m_full_path( NULL ),
		  m_download_rotator( NULL ),
		  m_upload_copier( NULL ),
		  m_rel_path( NULL ),
		  m_stat( NULL ),
		  m_exists( false ),
		  m_mtime( 0 ),
		  m_size( 0 ),
		  m_is_log( false )
{
}

ReplicatorFile::~ReplicatorFile( void )
{
	if( m_stat ) {
		delete m_stat;
		m_stat = 0;
	}
	if ( m_rel_path ) {
		free( const_cast<char *>(m_rel_path) );
		m_rel_path = NULL;
	}
	if ( m_download_rotator ) {
		delete m_download_rotator;
		m_download_rotator = NULL;
	}
	if ( m_upload_copier ) {
		delete m_upload_copier;
		m_upload_copier = NULL;
	}
	if ( m_full_path ) {
		free( const_cast<char *>(m_full_path) );
		m_full_path = NULL;
	}
	m_initialized = false;
}

SafeFileRotator *
ReplicatorFile::createRotator( void ) const
{
	return new SafeFileRotator(
		m_full_path,
		ReplicationBase::getDownloadExtension(),
		0,
		true );
}

SafeFileCopier *
ReplicatorFile::createCopier( void ) const
{
	return NULL;
}

bool
ReplicatorFile::init( const char &base, const char &rel_path, bool is_log )
{
	ASSERT( false == m_initialized );

	m_is_log = is_log;

	m_rel_path = strdup( &rel_path );
	if ( NULL == m_rel_path ) {
		return false;
	}

	int	len = strlen(&base) + strlen(&rel_path) + 2;
	char *tmp = (char *) malloc( len );
	if ( NULL == tmp ) {
		return false;
	}
	strcpy( tmp, &base );
	strcat( tmp, DIR_DELIM_STRING );
	strcat( tmp, &rel_path );

	m_full_path = tmp;

	// Create the safe rotator for the replicator
	// We do this always so that we can rotate in the first 'copy' of the log, too
	m_download_rotator = createRotator( );

	// Create the safe copier for the upload transferer
	if ( false == m_is_log ) {
		m_upload_copier = createCopier( );
	}

	m_stat = new StatWrapper( m_full_path );
 	(void) checkFile( );

	m_initialized = true;
	return true;
}

bool
ReplicatorFile::init( const ClassAd &ad, const MyString &attr_base )
{
	ASSERT( false == m_initialized );

	MyString	attr;
	MyString	tmp;
	int			tmpi;

	attr  = attr_base;
	attr += "_Name";
	if ( !ad.LookupString( attr.Value(), tmp ) ) {
		return false;
	}
	m_rel_path = strdup( tmp.Value() );

	attr  = attr_base;
	attr += "_Mtime";
	if ( !ad.LookupInteger( attr.Value(), tmpi ) ) {
		return false;
	}
	m_mtime = tmpi;

	attr  = attr_base;
	attr += "_Size";
	if ( !ad.LookupInteger( attr.Value(), tmpi ) ) {
		return false;
	}
	m_size = tmpi;

	attr  = attr_base;
	attr += "_IsLog";
	if ( !ad.LookupBool( attr.Value(), m_is_log ) ) {
		return false;
	}
	m_exists = false;
	dprintf( D_FULLDEBUG, "::init: %s m=%ld s=%ld log=%s\n",
			 m_rel_path, (unsigned long) m_mtime, (unsigned long) m_size,
			 m_is_log ? "true" : "false" );

	m_initialized = true;
	return true;
}

bool
ReplicatorFile::operator == ( const ReplicatorFile &other ) const
{
	return *this == other.getRelPath();
}

bool
ReplicatorFile::operator == ( const MyString &other ) const
{
	return other == m_rel_path;
}

bool
ReplicatorFile::operator == ( const char *other ) const
{
	if ( strcmp(other, m_rel_path) ) {
		return false;
	}
	else {
		return true;
	}
}

bool
ReplicatorFile::checkFile( void )
{
	StatStructTime	mtime;
	bool			newer;
	return checkFile( mtime, newer );
}

bool
ReplicatorFile::checkFile( StatStructTime &mtime, bool &newer )
{
	if ( NULL == m_stat ) {
		return false;
	}
	m_stat->Stat( StatWrapper::STATOP_STAT, true );
	if ( m_stat->GetRc() ) {
		if ( ENOENT == m_stat->GetErrno() ) {
			m_exists = false;
			m_mtime = 0;
			m_size = 0;
			return true;
		}
		else {
			return false;
		}
	}

	m_exists = true;
	mtime = m_stat->GetAccess().getMtime();
	newer = ( mtime > m_mtime );
	if ( newer ) {
		m_mtime = mtime;
		m_size = m_stat->GetAccess().getSize();
	}
	return true;
}

bool
ReplicatorFile::setDownloaderPid( int pid )
{
	if ( shouldRotate() ) {
		return m_download_rotator->setPid( pid );
	}
	else {
		return m_is_log;	// OK if true
	}
}

bool
ReplicatorFile::installNewFile( void )
{
	// If it's an append-only log file, we don't rotate it
	if ( !shouldRotate() ) {
		return true;
	}
	if ( !m_download_rotator->Rotate( ) ) {
		dprintf( D_ALWAYS,
				 "Failed to install new file for %s\n", m_full_path );
		return false;
	}
	return true;
}

bool
ReplicatorFile::cleanupTempDownloadFiles( void )
{
	// If it's an append-only log file, we don't rotate it
	if ( !shouldRotate() ) {
		return m_is_log;	// OK if true
	}
	(void) m_download_rotator->UnlinkTmp( );
	return true;
}

bool
ReplicatorFile::cleanupTempUploadFiles( int pid )
{
	// If PID is specified, create a private object & use it to clean up
	if ( pid ) {
		SafeFileNop	nop( m_full_path,
						 ReplicationBase::getUploadExtension(),
						 pid,
						 false );
		return nop.UnlinkTmp( );
	}

	// If it's an append-only log file, we don't rotate it
	if ( NULL == m_upload_copier ) {
		return m_is_log;	// OK if true
	}
	(void) m_download_rotator->UnlinkTmp( );
	return true;
}

bool
ReplicatorFile::updateAd( ClassAd &ad, const MyString &attr_base ) const
{
	MyString	attr;

	attr  = attr_base;
	attr += "_Name";
	ad.Assign( attr.Value(), m_rel_path );

	attr  = attr_base;
	attr += "_Mtime";
	ad.Assign( attr.Value(), m_mtime );

	StatStructOff	 size = m_stat->GetAccess().getSize();
	attr  = attr_base;
	attr += "_Size";
	ad.Assign( attr.Value(), size );

	attr  = attr_base;
	attr += "_IsLog";
	ad.Assign( attr.Value(), m_is_log );

	return true;
}
