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
#include "stat_wrapper.h"
#include "internet.h"

#include "ReplicatorFile.h"
#include "ReplicatorLocalVersion.h"
#include "FilesOperations.h"
#include <fstream>

ReplicatorLocalVersion::ReplicatorLocalVersion( ReplicatorFileSet &file_set,
												const MyString &version_file )
		: ReplicatorVersion( file_set, STATE_REQUESTING ),
		  m_versionFilePath( version_file ),
		  m_realFileset( file_set )
{
}

ReplicatorLocalVersion::~ReplicatorLocalVersion( void )
{
}

bool
ReplicatorLocalVersion::init( void )
{
	if ( m_versionFilePath == "" ) {
		dprintf( D_ALWAYS, "LocalVersion::init(): No version file path!\n" );
		return false;
	}

	m_mtime = -1;
	bool	status = true;
	if ( !readVersionFile() ) {
		if ( !writeVersionFile() ) {
			dprintf( D_ALWAYS,
					 "LocalVersion::init: Failed to write to version file!\n" );
			status = false;
		}
	}
	return synchronize( false );
}

bool
ReplicatorLocalVersion::updateMtime( bool &newer )
{

    time_t			now = time( NULL );
	StatStructTime	old_mtime = m_mtime;
	int				count;
	if ( ! m_realFileset.checkFiles( m_mtime, newer, count ) ) {
		dprintf( D_ALWAYS,
				 "::updateMtime(): Failed to get mtime for file set\n" );
		m_mtime = 0;
	}

    // to contain the time strings produced by 'ctime_r' function, which is
    // reentrant unlike 'ctime' one
	MyString lastKnownMtimeStr = ctime( &old_mtime );
	lastKnownMtimeStr.chomp();

	MyString lastMtimeStr = ctime( &m_mtime );
	lastMtimeStr.chomp();
	MyString nowStr = ctime( &now );
	nowStr.chomp();

    // retrieving access status information
    dprintf( D_FULLDEBUG,
			 "::updateMtime(): %s before setting last mod. time:\n"
			 "  last known mod. time - %s\n"
			 "  actual mod. time - %s\n"
			 "  current time - %s\n",
			 m_versionFilePath.Value(),
			 lastKnownMtimeStr.Value( ),
			 lastMtimeStr.Value( ),
			 nowStr.Value( ) );
	return true;
}

bool
ReplicatorLocalVersion::synchronize(bool isLogicalClockIncremented)
{
    dprintf( D_FULLDEBUG,
			 "::synchronize() started (is logical clock incremented = %s)\n",
             isLogicalClockIncremented ? "True" : "False" );

    readVersionFile( );        
    createFile( );

    // updating the version: by modification time of the underlying file
    // and incrementing the logical version number
	bool newer;
	updateMtime( newer );
    if( ! newer ) {
        return true;
    }
    dprintf( D_FULLDEBUG,
			 "::synchronize(): setting version last modified time\n" );
    
    if( isLogicalClockIncremented && (m_logicalClock < INT_MAX) ) {
		m_logicalClock++;
        return writeVersionFile( );
    } 

	if ( isLogicalClockIncremented ) {
		// to be on a sure side, when the maximal logical clock value is
		// reached, we terminate the replication daemon
		EXCEPT( "::synchronize(): reached maximal logical clock value\n" );
	}

    return true;
}

#if 0
bool
ReplicatorLocalVersion::isComparable(
	const ReplicatorLocalVersion &other ) const
{
    return getGid( ) == other.getGid( ) ;
}
#endif

bool
ReplicatorLocalVersion::createFile( void ) const
{
	StatWrapper statWrapper( m_versionFilePath );

	// if no state file found, create one
	if ( statWrapper.GetRc( ) && statWrapper.GetErrno() == ENOENT) {
   		ofstream file( m_versionFilePath.Value( ) );
    }
	return true;
}

const char *
ReplicatorLocalVersion::toString( MyString &str ) const
{
	str =  "Local version: ";
    str += "logicalClock = ";
    str += m_logicalClock;
    str += ", gid = ";
    str += m_gid;
    
    return str.Value();
}

/* Function    : readVersionFile
 * Return value: bool - success/failure value
 * Description : loads ReplicatorFileReplica components from the underlying
 *				 OS file
 *				 to the appropriate object data members
 * Note        : the function is like public 'load' with one only difference - 
 *				 it changes the state of the object itself
 */
bool
ReplicatorLocalVersion::readVersionFile( void )
{
    dprintf( D_FULLDEBUG,
			 "::readVersionFile(): Reading from %s\n",
			 m_versionFilePath.Value() );

	int temporaryGid = -1;
	int temporaryLogicalClock = -1;
    
	if( ! readVersionFile( temporaryGid, temporaryLogicalClock ) ) {
		return false;
	}
	
	m_gid          = temporaryGid;
    m_logicalClock = temporaryLogicalClock;

    return true;
}

bool
ReplicatorLocalVersion::readVersionFile(
	int		&temporaryGid,
	int		&temporaryLogicalClock ) const
{
    char		 buffer[BUFSIZ];
	const char	*path = m_versionFilePath.Value();

    ifstream versionFile( path );
    if( ! versionFile.is_open( ) ) {
        dprintf( D_FAILURE,
				 "::readVersionFile(): unable to open %s\n", path );
        return false;
    }
    // read gid
    if( versionFile.eof( ) ) {
        dprintf( D_FAILURE,
				 "::readVersionFile():"
				 " %s format is corrupted, nothing appears inside it\n",
                 path );
        return false;
    }
    versionFile.getline( buffer, BUFSIZ );

    temporaryGid = atol( buffer );

    dprintf( D_FULLDEBUG, "::readVersionFile(): gid = %d\n", temporaryGid );
    // read version
    if( versionFile.eof( ) ) {
        dprintf( D_FAILURE,
				 "::readVersionFile(): "
				 "%s format is corrupted, only gid appears inside it\n",
                 path );
        return false;
    }
    versionFile.getline( buffer, BUFSIZ );
    temporaryLogicalClock = atol( buffer );

    dprintf( D_FULLDEBUG,
			 "::readVersionFile(): version = %d\n",
             temporaryLogicalClock );
    ASSERT( (temporaryGid >= 0) && (temporaryLogicalClock >= 0) );

	return true;
}

/* Function   : writeVersionFile
 * Description: writes the replica object components to the underlying OS file
 */
bool
ReplicatorLocalVersion::writeVersionFile( void )
{
    dprintf( D_ALWAYS, "::writeVersionFile(): started\n" );

    ofstream versionFile( m_versionFilePath.Value() );

    versionFile << m_gid << endl << m_logicalClock;

	return true;
}

/* Function   : setGid
 * Arguments  : newGid - new gid of the version
 * Description: sets the gid of the version
 */
void
ReplicatorLocalVersion::setGid( int newGid, bool save )
{
	setGid( newGid );
	if (save) {
		writeVersionFile( );
	}
}

/* Function   : setLogicalClock
 * Arguments  : newLogicalClock - new logical clock of the version
 * Description: sets the logical clock of the version
 */
void
ReplicatorLocalVersion::setLogicalClock(int clock, bool save)
{
	setLogicalClock( clock );
	if (save) {
		writeVersionFile( );
	}
}

bool
ReplicatorLocalVersion::sameFiles( const ReplicatorLocalVersion &other ) const
{
	return m_realFileset.sameFiles( other.getRealFileset() );
}

bool
ReplicatorLocalVersion::updateAd( ClassAd &ad ) const
{
	ad.Assign( ATTR_HAD_REPLICATION_GID, m_gid );
	ad.Assign( ATTR_HAD_REPLICATION_LOGICAL_CLOCK, m_logicalClock );
	ad.Assign( ATTR_HAD_REPLICATION_IS_PRIMARY, m_isPrimary );
	ad.Assign( ATTR_HAD_REPLICATION_MTIME, m_mtime );
	ad.Assign( ATTR_HAD_REPLICATION_STATE_NUM, (int) m_state );
	ad.Assign( ATTR_HAD_REPLICATION_STATE_NAME, getStateName(m_state) );

	if ( DebugFlags & D_FULLDEBUG ) {
		MyString	s;
		ad.sPrint(s);
		dprintf( D_FULLDEBUG, "LocalVersion::updateAd =\n%s\n", s.Value() );
	}

	return true;
}
