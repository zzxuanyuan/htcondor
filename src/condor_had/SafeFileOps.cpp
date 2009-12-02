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
#include "SafeFileOps.h"
#include "Utils.h"
#include "util_lib_proto.h"
#include "stat_wrapper.h"

/*
 * Safe File ops base class
 */
SafeFileOp::SafeFileOp( bool auto_cleanup )
		: m_auto_cleanup( auto_cleanup )
{
}

SafeFileOp::SafeFileOp( const char *path, const char *ext,
						  int pid, bool auto_cleanup )
		: m_auto_cleanup( auto_cleanup )
{
	init( path, ext, pid );
}

bool
SafeFileOp::init( const MyString &path, const char *ext, int pid )
{
	return init( path.Value(), ext, pid );
}

bool
SafeFileOp::init( const char *path, const char *ext, int pid )
{
	m_filePath = path;
	m_ext = ext;
	return buildPath( pid );
}

bool
SafeFileOp::setPid( int pid )
{
	return buildPath( pid );
}

// Build the temp file path
bool
SafeFileOp::buildPath( int pid )
{
	m_tmpFilePath = m_filePath;
	m_tmpFilePath += m_ext;
	m_tmpFilePath += ".";
	m_tmpFilePath += pid;

	return true;
}

bool
SafeFileOp::doOperation( void )
{
    int retryTimes = 0;

    while( ( retryTimes < IO_RETRY_TIMES ) && ( Operate( ) == false ) ) {
        retryTimes++;
    }
    if ( retryTimes == IO_RETRY_TIMES ) {
        dprintf( D_ALWAYS,
				 "%s failed with %s.%s %d times\n",
				 OpName(), m_filePath.Value(), m_ext.Value(), IO_RETRY_TIMES );
        return false;
    }
    return true;
}

SafeFileOp::~SafeFileOp( void )
{
	if ( m_auto_cleanup ) {
		UnlinkTmp( );
	}
}

bool
SafeFileOp::UnlinkTmp( void ) const
{
	if( unlink( m_tmpFilePath.Value( ) ) != 0 ) {
		if ( ENOENT != errno ) {
			dprintf( D_ALWAYS,
					 "unlinkFile unable to unlink %s, reason: %s\n",
					 m_tmpFilePath.Value( ), strerror(errno));
			return false;
		}
	}
    return true;
}


/*
 * Safe File NO-OP class
 */
SafeFileNop::SafeFileNop( bool auto_cleanup )
		: SafeFileOp( auto_cleanup )
{
}
SafeFileNop::SafeFileNop(
	const char *path, const char *ext, int pid, bool auto_cleanup )
		: SafeFileOp( path, ext, pid, auto_cleanup )
{
}

SafeFileNop::SafeFileNop(
	const MyString &path, const char *ext, int pid, bool auto_cleanup )
		: SafeFileOp( path.Value(), ext, pid, auto_cleanup )
{
}


/*
 * Safe File Rotator class
 */
SafeFileRotator::SafeFileRotator( bool auto_cleanup )
		: SafeFileOp( auto_cleanup )
{
}
SafeFileRotator::SafeFileRotator(
	const char *path, const char *ext, int pid, bool auto_cleanup )
		: SafeFileOp( path, ext, pid, auto_cleanup )
{
}

SafeFileRotator::SafeFileRotator(
	const MyString &path, const char *ext, int pid, bool auto_cleanup )
		: SafeFileOp( path.Value(), ext, pid, auto_cleanup )
{
}

bool
SafeFileRotator::Operate( void )
{
    dprintf( D_ALWAYS,
			 "Rotate %s with extension %s started\n",
             m_filePath.Value(), m_ext.Value() );

    if( rotate_file( m_tmpFilePath.Value( ), m_filePath.Value() ) < 0 ) {
        dprintf( D_ALWAYS,
				 "Rotate: cannot rotate file %s\n", m_filePath.Value() );
        return false;
    }
    return true;
}


/*
 * Safe File Copier class
 */
SafeFileCopier::SafeFileCopier( bool auto_cleanup )
		: SafeFileOp( auto_cleanup )
{
}
SafeFileCopier::SafeFileCopier(
	const char *path, const char *ext, int pid, bool auto_cleanup )
		: SafeFileOp( path, ext, pid, auto_cleanup )
{
}

SafeFileCopier::SafeFileCopier(
	const MyString &path, const char *ext, int pid, bool auto_cleanup )
		: SafeFileOp( path.Value(), ext, pid, auto_cleanup )
{
}

bool
SafeFileCopier::Operate( void )
{
    dprintf( D_ALWAYS,
			 "Copy %s with extension %s started\n",
             m_filePath.Value(), m_ext.Value() );

    if( copy_file( m_filePath.Value(), m_tmpFilePath.Value( ) ) ) {
        dprintf( D_ALWAYS,
				 "Safe copy: unable to copy %s to %s\n",
				 m_filePath.Value(), m_tmpFilePath.Value( ) );
        return false;
    }
    return true;
}
