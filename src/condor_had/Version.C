// for 'daemonCore'
#include "../condor_daemon_core.V6/condor_daemon_core.h"
// for 'StatWrapper'
#include "stat_wrapper.h"
// for 'getHostFromAddr' and 'getPortFromAddr'
#include "internet.h"

#include "Version.h"
#include "FilesOperations.h"

time_t Version::lastModifiedTime = -1;

static void
createFile(const MyString& filePath)
{
	StatWrapper statWrapper( filePath.GetCStr( ) );
	// if no state file found, create one
	if ( statWrapper.GetStatus( ) && statWrapper.GetErrno() == ENOENT) {
   		ofstream file( filePath.GetCStr( ) );
    }
}

Version::Version():
    gid( 0 ), logicalClock( 0 ), state( VERSION_REQUESTING )
{
}

void
Version::initialize( const MyString& pStateFilePath, 
					 const MyString& pVersionFilePath )
{
	REPLICATION_ASSERT(pStateFilePath != "" && pVersionFilePath != "");
	
    lastModifiedTime = -1; 
	stateFilePath    = pStateFilePath;
	versionFilePath  = pVersionFilePath;
    
    if( ! load( ) ) {
        save( );
    }
    synchronize( false );

    sinfulString = daemonCore->InfoCommandSinfulString( );
//char* sinfulStringString = 0;
//    get_full_hostname( hostNameString );
//    hostName = hostNameString;
//    delete [] hostNameString;
}

bool
Version::synchronize(bool isLogicalClockIncremented)
{
	REPLICATION_ASSERT(stateFilePath != "" && versionFilePath != "");
    dprintf( D_ALWAYS, "Version::synchronize started "
			"(is logical clock incremented = %d)\n",
             int( isLogicalClockIncremented ) );
    load( );        
    createFile( stateFilePath );

    StatWrapper statWrapper( stateFilePath.GetCStr( ) );
    
	const StatStructType* status      = statWrapper.GetStatBuf( );
    time_t                currentTime = time( NULL );
    // to contain the time strings produced by 'ctime_r' function
    char                  timeBuffer[BUFSIZ];
    // retrieving access status information
    dprintf( D_FULLDEBUG,
                    "Version::synchronize %s "
                    "before setting last mod. time:\n"
                    "last known mod. time - %sactual mod. time - %s"
                    "current time - %s",
               stateFilePath.GetCStr( ),
               ctime_r( &lastModifiedTime, timeBuffer ),
               ctime_r( &status->st_mtime, timeBuffer + BUFSIZ / 3 ),
               ctime_r( &currentTime, timeBuffer + 2 * BUFSIZ / 3 ) );
    // updating the version: by modification time of the underlying file
    // and incrementing the logical version number
    if( lastModifiedTime >= status->st_mtime ) {
        return false;
    }
    dprintf( D_FULLDEBUG, "Version::synchronize "
                          "setting version last modified time\n" );
    lastModifiedTime = status->st_mtime;
    
    if( isLogicalClockIncremented ) {
        logicalClock ++;
        save( );

        return true;
    }

    return false;
}

bool
Version::code( ReliSock& socket )
{
    dprintf( D_ALWAYS, "Version::code started\n" );
    socket.encode( );

    char* temporarySinfulString = const_cast<char*>( sinfulString.GetCStr( ) );
    
    if( ! socket.code( gid )          /*|| ! socket.eom( )*/ ||
        ! socket.code( logicalClock ) /*|| ! socket.eom( )*/ ||
        ! socket.code( temporarySinfulString ) /*|| ! socket.eom( )*/ ) {
        dprintf( D_NETWORK, "Version::code "
                            "unable to code the version\n");
        return false;
    }
    return true;
}

bool
Version::decode( Stream* stream )
{
    dprintf( D_ALWAYS, "Version::decode started\n" );
    
    int   temporaryGid          = -1;
    int   temporaryLogicalClock = -1;
    char* temporarySinfulString = 0;

    stream->decode( );

    if( ! stream->code( temporaryGid ) ) {
        dprintf( D_NETWORK, "Version::decode "
                            "unable to decode the gid\n" );
        return false;
    }
    stream->decode( );

    if( ! stream->code( temporaryLogicalClock ) ) {
        dprintf( D_NETWORK, "Version::decode "
                            "unable to decode the logical clock\n" );
        return false;
    }
    stream->decode( );

    if( ! stream->code( temporarySinfulString ) ) {
        dprintf( D_NETWORK, "Version::decode "
                            "unable to decode the sinful string\n" );
        return false;
    }
    gid          = temporaryGid;
    logicalClock = temporaryLogicalClock;
    sinfulString = temporarySinfulString;
    dprintf( D_FULLDEBUG, "Version::decode remote version %s\n", 
			 toString( ).GetCStr( ) );
    free( temporarySinfulString );

    return true;
}

MyString
Version::getHostName( ) const
{
    char*     hostNameString = getHostFromAddr( sinfulString.GetCStr( ) );
    MyString  hostName       = hostNameString;

    free( hostNameString );
    dprintf( D_FULLDEBUG, "Version::getHostName returned %s\n", 
			 hostName.GetCStr( ) );
    return hostName;
}

bool
Version::isComparable( const Version& version ) const
{
    return getGid( ) == version.getGid( ) ;//&&
 // strcmp( getSinfulString( ), version.getSinfulString( ) ) == 0;
}

bool
Version::operator > ( const Version& version ) const
{
    dprintf( D_FULLDEBUG, "Version::operator > comparing %s vs. %s\n",
               toString( ).GetCStr( ), version.toString( ).GetCStr( ) );
    
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
Version::operator >= (const Version& version) const
{
    dprintf( D_FULLDEBUG, "Version::operator >= started\n" );
    return ! ( version > *this);
}

MyString
Version::toString( ) const
{
    MyString versionAsString = "logicalClock = ";

    versionAsString += logicalClock;
    versionAsString += ", gid = ";
    versionAsString += gid;
    versionAsString += ", belongs to ";
    versionAsString += sinfulString;
    
    return versionAsString;
}
/* Function    : load
 * Return value: bool - success/failure value
 * Description : loads Version components from the underlying OS file
 *				 to the appropriate object data members
 * Note        : the function is like public 'load' with one only difference - it changes the
 *				 state of the object itself
 */
bool
Version::load( )
{
    dprintf( D_ALWAYS, "Version::load of %s started\n", 
			 versionFilePath.GetCStr( ) );
//    char     buffer[BUFSIZ];
//    ifstream versionFile( versionFilePath.GetCStr( ) );
//
//    if( ! versionFile.is_open( ) ) {
//        dprintf( D_FAILURE, "Version::load unable to open %s\n",
//                 versionFilePath.GetCStr( ) );
//        return false;
//    }
    // read gid
//    if( versionFile.eof( ) ) {
//        dprintf( D_FAILURE, "Version::load %s format is corrupted, "
//                            "nothing appears inside it\n", 
//				 versionFilePath.GetCStr( ) );
//        return false;
//    }
//    versionFile.getline( buffer, BUFSIZ );
//
//    int temporaryGid = atol( buffer );
//
//    dprintf( D_FULLDEBUG, "Version::load gid = %d\n", temporaryGid );
    // read version
//    if( versionFile.eof( ) ) {
//        dprintf( D_FAILURE, "Version::load %s format is corrupted, "
//                			"only gid appears inside it\n", 
//				 versionFilePath.GetCStr( ) );
//        return false;
//    }
//
//    versionFile.getline( buffer, BUFSIZ );
//    int temporaryLogicalClock = atol( buffer );
//
//    dprintf( D_FULLDEBUG, "Version::load version = %d\n", 
//			 temporaryLogicalClock );
	int temporaryGid = -1, temporaryLogicalClock = -1;
    
	if( ! load( temporaryGid, temporaryLogicalClock ) ) {
		return false;
	}
	
	gid          = temporaryGid;
    logicalClock = temporaryLogicalClock;

    return true;
}

bool
Version::load( int& temporaryGid, int& temporaryLogicalClock ) const
{
    char     buffer[BUFSIZ];
    ifstream versionFile( versionFilePath.GetCStr( ) );

    if( ! versionFile.is_open( ) ) {
        dprintf( D_FAILURE, "Version::load unable to open %s\n",
                 versionFilePath.GetCStr( ) );
        return false;
    }
    // read gid
    if( versionFile.eof( ) ) {
        dprintf( D_FAILURE, "Version::load %s format is corrupted, "
                            "nothing appears inside it\n",
                 versionFilePath.GetCStr( ) );
        return false;
    }
    versionFile.getline( buffer, BUFSIZ );

    temporaryGid = atol( buffer );

    dprintf( D_FULLDEBUG, "Version::load gid = %d\n", temporaryGid );
    // read version
    if( versionFile.eof( ) ) {
        dprintf( D_FAILURE, "Version::load %s format is corrupted, "
                            "only gid appears inside it\n",
                 versionFilePath.GetCStr( ) );
        return false;
    }
    versionFile.getline( buffer, BUFSIZ );
    temporaryLogicalClock = atol( buffer );

    dprintf( D_FULLDEBUG, "Version::load version = %d\n",
             temporaryLogicalClock );
    REPLICATION_ASSERT(temporaryGid >= 0 && temporaryLogicalClock >= 0);

	return true;
}
/* Function   : save
 * Description: saves the Version object components to the underlying OS file
 */
void
Version::save( )
{
    dprintf( D_ALWAYS, "Version::save started\n" );

    ofstream versionFile( versionFilePath.GetCStr( ) );

    versionFile << gid << endl << logicalClock;
    //versionFile.close( );
    // finding the new last modification time
//    StatWrapper statWrapper( stateFilePath.GetCStr( ) );
//
//    if ( statWrapper.GetStatus( ) ) {
//        EXCEPT("Version::synchronize cannot get %s status "
//               "due to errno = %d", 
//               versionFilePath.GetCStr( ), 
//				 statWrapper.GetErrno());
//    }
//    lastModifiedTime = statWrapper.GetStatBuf( )->st_mtime;
}
