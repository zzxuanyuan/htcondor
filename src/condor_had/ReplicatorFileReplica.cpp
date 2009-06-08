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

#include "ReplicatorFileReplica.h"
#include "FilesOperations.h"
#include "condor_fix_fstream.h"

time_t ReplicatorFileReplica::m_lastModifiedTime = -1;


// ==== ReplicatorFileReplica methods ====
ReplicatorFileReplica::ReplicatorFileReplica( const ReplicatorFile &file,
											  const ReplicatorPeer &peer )
		: ReplicatorFileVersion( file ),
		  m_peerInfo( peer ),
		  m_uploadProcessData( *this ),
		  m_state( VERSION_REQUESTING ),
		  m_isPrimary( FALSE )
{
}

bool
ReplicatorFileReplica::initialize( const ReplicatorFile &file )
{
	if ( !ReplicatorFileVersion::initialize( ) ) {
		return false;
	}
    m_mySinfulString = daemonCore->InfoCommandSinfulString( );
	return true;
}

bool
ReplicatorFileReplica::registerUploaders(
	ReplicatorTransferList &transferers ) const
{
	return transferers.Register( m_uploader );
}

bool
ReplicatorFileReplica::code( ReliSock& socket )
{
    dprintf( D_ALWAYS, "ReplicatorFileReplica::code started\n" );
    socket.encode( );

    char* temporarySinfulString = const_cast<char*>( m_sinfulString.Value() );
   	int isPrimaryAsInteger      = int( m_isPrimary );
   
    if( !socket.code( m_gid )          /*|| ! socket.eom( )*/ ||
        !socket.code( m_logicalClock ) /*|| ! socket.eom( )*/ ||
        !socket.code( temporarySinfulString ) /*|| ! socket.eom( )*/ || 
		!socket.code( isPrimaryAsInteger ) ) {
        dprintf( D_NETWORK, "ReplicatorFileReplica::code "
                            "unable to code the version\n");
        return false;
    }
    return true;
}

bool
ReplicatorFileReplica::decode( Stream* stream )
{
    dprintf( D_ALWAYS, "ReplicatorFileReplica::decode started\n" );
    
    int   gid           = -1;
    int   logical_clock = -1;
    char *sinful_string = 0;
	int   is_primary    = 0;

    stream->decode( );

    if( ! stream->code( gid ) ) {
        dprintf( D_NETWORK, "ReplicatorFileReplica::decode "
                            "unable to decode the gid\n" );
        return false;
    }

    if( ! stream->code( logicalClock ) ) {
        dprintf( D_NETWORK, "ReplicatorFileReplica::decode "
                            "unable to decode the logical clock\n" );
        return false;
    }

    if( ! stream->code( sinfulString ) ) {
        dprintf( D_NETWORK, "ReplicatorFileReplica::decode "
                            "unable to decode the sinful string\n" );
        return false;
    }

	if( ! stream->code( isPrimary ) ) {
        dprintf( D_NETWORK, "ReplicatorFileReplica::decode "
                            "unable to decode the 'isPrimary' field\n" );
        return false;
    }

	setGid( gid, false );
	setLogicalClock( logical_clock, false );
    m_sinfulString = sinful_string;
	m_isPrimary    = is_primary;
    dprintf( D_FULLDEBUG,
			 "::decode(): remote version %s\n", toString( ).Value( ) );
    free( sinful_string );

    return true;
}

MyString
ReplicatorFileReplica::toString( void ) const
{
    MyString versionAsString = "logicalClock = ";

    versionAsString += m_logicalClock;
    versionAsString += ", gid = ";
    versionAsString += m_gid;
    versionAsString += ", belongs to ";
    versionAsString += m_sinfulString;
    
    return versionAsString;
}
