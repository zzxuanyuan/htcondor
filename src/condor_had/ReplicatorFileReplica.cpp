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

#include "ReplicatorFile.h"
#include "ReplicatorPeer.h"
#include "ReplicatorFileReplica.h"
#include "FilesOperations.h"
#include "condor_fix_fstream.h"


// ==== ReplicatorFileReplica methods ====
ReplicatorFileReplica::ReplicatorFileReplica( const ReplicatorFileBase &file,
											  const ReplicatorPeer &peer )
		: ReplicatorFileVersion( file ),
		  m_fileInfo( file ),
		  m_peerInfo( peer ),
		  m_uploader( *this ),
		  m_state( STATE_REQUESTING ),
		  m_isPrimary( FALSE )
{
}

ReplicatorFileReplica *
ReplicatorFileReplica::generate( const ClassAd &ad,
								 const ReplicatorPeer &peer,
								 MyString &s )
{
	int					 gid = 0;
	int					 clock;
	bool				 is_primary;
	ReplicatorState		 state;
	ReplicatorFileSet	*file_set = NULL;
	MyString			 tmp;

	if ( !ad.LookupInteger( ATTR_REPLICATOR_GID, gid ) ) {
		s.sprintf( "GID missing in peer ad\n" );
		return NULL;
	};
	

	if ( !ad.LookupInteger( ATTR_REPLICATOR_LOGICAL_CLOCK, clock ) ) {
		s.sprintf( "'Logical clock' missing in peer ad\n" );
		return NULL;
	};

	if ( !ad.LookupBool( ATTR_REPLICATOR_IS_PRIMARY, is_primary ) ) {
		s.sprintf( "'Is Primary' missing in peer ad\n" );
		return NULL;
	};

	if ( !ad.LookupString( ATTR_REPLICATOR_STATE, tmp ) ) {
		s.sprintf( "'State' missing in peer ad\n" );
		return NULL;
	};

	state = ReplicatorFileReplica::lookupState( tmp.Value() );
	if ( STATE_INVALID == state ) {
		s.sprintf( "Invalid state '%s' in peer ad\n", tmp.Value() );
		return NULL;
	}

	// Extract the file set
	if ( ad.LookupString( ATTR_REPLICATOR_FILE_SET, tmp ) ) {
		if ( NULL == m_fileSet ) {
			s.sprintf( "No file set in peer ad" );
			return NULL;
		}
		StringList	files(tmp.Value());
		file_set = new ReplicatorFileSet( files );
	}

	ReplicatorFileReplica *replica =
		new ReplicatorFileReplica( file_set, peer );
	replica->setGid( gid, false );
	replica->setLogicalClock( clock, false );
	replica->setState( state );

	return replica;
}

#if 0
bool
ReplicatorFileReplica::initialize( const ReplicatorFileBase &file )
{
	if ( !ReplicatorFileVersion::initialize( ) ) {
		return false;
	}
    m_mySinfulString = daemonCore->InfoCommandSinfulString( );
	return true;
}
#endif

bool
ReplicatorFileReplica::registerUploaders(
	ReplicatorTransfererList &transferers ) const
{
	return transferers.Register( m_uploader );
}

struct StateLookup
{
	ReplicatorState	 state;
	const char		*str;
};
static StateLookup	states[] =
{
	{ STATE_REQUESTING, "REQUESTING" },
	{ STATE_DOWNLOADING, "DOWNLOADING" },
	{ STATE_BACKUP, "BACKUP" },
	{ STATE_LEADER, "LEADER" },
	{ STATE_INVALID, NULL }
};

/* Function    : lookupState
 * Argument    : char * - string representation of the state
 * Return value: ReplicatorState - the state value / invalid
 * Description : returns the state
 */   
ReplicatorState
ReplicatorFileReplica::lookupState( const char *str )
{
	for( int i = 0;  states[i].state != STATE_INVALID;  i++ ) {
		if ( !strcasecmp( states[i].str, str ) ) {
			return states[i].state;
		}
	}
	return STATE_INVALID;
}

/* Function    : lookupState
 * Argument    : ReplicatorState - the state value
 * Return value: char * - string representation of the state
 * Description : returns the string representing the state
 */   
const char *
ReplicatorFileReplica::lookupState( ReplicatorState state )
{
	for( int i = 0;  states[i].state != STATE_INVALID;  i++ ) {
		if ( states[i].state == state ) {
			return states[i].str;
		}
	}
	return NULL;
}

bool
ReplicatorFileReplica::operator > ( const ReplicatorFileReplica &other ) const
{
	MyString	myStr, otherStr;
    dprintf( D_FULLDEBUG,
			 "ReplicatorFileReplica::operator > comparing %s vs. %s\n",
			 toString(myStr), other.toString(otherStr) );
    
    if( getState( ) == STATE_LEADER &&
        other.getState( ) != STATE_LEADER ) {
        return true;
    }
    if( getState( ) != STATE_LEADER &&
        other.getState( ) == STATE_LEADER ) {
        return false;
    }
    return ( getLogicalClock() > other.getLogicalClock() );
}

bool
ReplicatorFileReplica::operator >= ( const ReplicatorFileReplica &other ) const
{
    dprintf( D_FULLDEBUG, "ReplicatorFileReplica::operator >= started\n" );
    return ! ( other > *this);
}

#if 0
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
#endif

const char *
ReplicatorFileReplica::toString( MyString &str ) const
{
    str = "logicalClock = ";
    str += m_logicalClock;
    str += ", gid = ";
    str += m_gid;
    str += ", belongs versionAsString ";
    str += m_peerInfo.getSinful();
    
    return str.Value();
}
