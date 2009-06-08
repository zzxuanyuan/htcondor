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
// for 'daemonCore'
#include "condor_daemon_core.h"
// for 'StatWrapper'
#include "stat_wrapper.h"
// for 'getHostFromAddr' and 'getPortFromAddr'
#include "internet.h"
// implicit declaration for 'ctime_r' on Alpha OSF V5.1 platforms
//#include <time.h>

#include "ReplicatorFileVersion.h"
#include "FilesOperations.h"
#include "condor_fix_fstream.h"

time_t ReplicatorFileReplica::m_lastModifiedTime = -1;

static void
createFile(const MyString& filePath)
{
	StatWrapper statWrapper( filePath );
	// if no state file found, create one
	if ( statWrapper.GetRc( ) && statWrapper.GetErrno() == ENOENT) {
   		ofstream file( filePath.Value( ) );
    }
}

ReplicatorFileVersion::ReplicatorFileVersion( const ReplicatorFile &file ) 
		: m_fileInfo( file ),
		  m_gid( 0 ),
		  m_logicalClock( 0 )
{
}

bool
ReplicatorFileVersion::initialize( const ReplicatorFile &file )
{
	ASSERT( (NULL == m_fileInfo) || (*m_fileInfo != file)  );
	
    m_lastModifiedTime = -1; 
	m_stateFilePath    = pStateFilePath;
	m_versionFilePath  = pVersionFilePath;
    
    if( ! readVersionFile( ) ) {
        writeVersionFile( );
    }
    synchronize( false );
	return true;
}

bool
ReplicatorFileVersion::synchronize(bool isLogicalClockIncremented)
{
	REPLICATION_ASSERT(m_stateFilePath != "" && m_versionFilePath != "");
    dprintf( D_ALWAYS, "::synchronize() started "
			"(is logical clock incremented = %s)\n",
             isLogicalClockIncremented ? "True" : "False" );
    readVersionFile( );        
    createFile( m_stateFilePath );

    StatWrapper statWrapper( m_stateFilePath );
    
	const StatStructType* status = statWrapper.GetBuf( );
    time_t                now = time( NULL );

    // to contain the time strings produced by 'ctime_r' function, which is
    // reentrant unlike 'ctime' one
	MyString lastKnownModifiedTimeString = ctime( &m_lastModifiedTime );
	lastKnownModifiedTimeString.chomp();

	MyString lastModifiedTimeString = ctime( &status->st_mtime );
	lastModifiedTimeString.chomp();

	MyString nowStr = ctime( &now );
	nowStr.chomp();

    // retrieving access status information
    dprintf( D_FULLDEBUG,
			 "::synchronize(): %s before setting last mod. time:\n"
			 "  last known mod. time - %s\n"
			 "  actual mod. time - %s\n"
			 "  current time - %s\n",
			 m_stateFilePath.Value( ),
			 lastKnownModifiedTimeString.Value( ),
			 lastModifiedTimeString.Value( ),
			 currentTimeString.Value( ) );

    // updating the version: by modification time of the underlying file
    // and incrementing the logical version number
    if( m_lastModifiedTime >= status->st_mtime ) {
        return false;
    }
    dprintf( D_FULLDEBUG,
			 "::synchronize(): setting version last modified time\n" );
    m_lastModifiedTime = status->st_mtime;
    
    if( isLogicalClockIncremented && (m_logicalClock < INT_MAX) ) {
		m_logicalClock ++;
        return writeVersionFile( );
    } 

	if ( isLogicalClockIncremented ) {
		// to be on a sure side, when the maximal logical clock value is
		// reached, we terminate the replication daemon
		utilCrucialError( "::synchronize(): "
						  "reached maximal logical clock value\n" );
	}

    return false;
}

bool
ReplicatorFileVersion::isComparable(
	const ReplicatorFileReplica& version ) const
{
    return getGid( ) == version.getGid( ) ;//&&
 // strcmp( getSinfulString( ), version.getSinfulString( ) ) == 0;
}

bool
ReplicatorFileVersion::operator > (
	const ReplicatorFileReplica& version ) const
{
    dprintf( D_FULLDEBUG,
			 "ReplicatorFileVersion::operator > comparing %s vs. %s\n",
			 toString( ).Value( ), version.toString( ).Value( ) );
    
    if( getState( ) == REPLICATION_LEADER &&
        version.getState( ) != REPLICATION_LEADER ) {
        return true;
    }
    if( getState( ) != REPLICATION_LEADER &&
        version.getState( ) == REPLICATION_LEADER ) {
        return false;
    }
    return getLogicalClock( ) > version.getLogicalClock( );
}

bool
ReplicatorFileVersion::operator >= (
	const ReplicatorFileReplica& version) const
{
    dprintf( D_FULLDEBUG, "ReplicatorFileVersion::operator >= started\n" );
    return ! ( version > *this);
}

MyString &
ReplicatorFileVersion::toString( MyString &str ) const
{
    str = "logicalClock = ";

    str += m_logicalClock;
    str += ", gid = ";
    str += m_gid;
    str += ", belongs to ";
    str += m_sinfulString;
    
    return str;
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
ReplicatorFileVersion::readVersionFile( void )
{
    dprintf( D_ALWAYS,
			 "::readVersionFile(): Reading from %s\n",
			 m_versionFilePath.Value( ) );

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
ReplicatorFileVersion::readVersionFile(
	int		&temporaryGid,
	int		&temporaryLogicalClock ) const
{
    char     buffer[BUFSIZ];
    ifstream versionFile( m_versionFilePath.Value( ) );

    if( ! versionFile.is_open( ) ) {
        dprintf( D_FAILURE,
				 "::readVersionFile(): unable to open %s\n",
				 m_versionFilePath.Value( ) );
        return false;
    }
    // read gid
    if( versionFile.eof( ) ) {
        dprintf( D_FAILURE,
				 "::readVersionFile():"
				 " %s format is corrupted, nothing appears inside it\n",
                 m_versionFilePath.Value( ) );
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
                 m_versionFilePath.Value( ) );
        return false;
    }
    versionFile.getline( buffer, BUFSIZ );
    temporaryLogicalClock = atol( buffer );

    dprintf( D_FULLDEBUG,
			 "::readVersionFile(): version = %d\n",
             temporaryLogicalClock );
    REPLICATION_ASSERT(temporaryGid >= 0 && temporaryLogicalClock >= 0);

	return true;
}

/* Function   : writeVersionFile
 * Description: writes the replica object components to the underlying OS file
 */
bool
ReplicatorFileVersion::writeVersionFile( )
{
    dprintf( D_ALWAYS, "::writeVersionFile(): started\n" );

    ofstream versionFile( m_versionFilePath.Value( ) );

    versionFile << m_gid << endl << m_logicalClock;

	return true;
}
