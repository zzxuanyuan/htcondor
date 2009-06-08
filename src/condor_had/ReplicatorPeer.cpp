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
#include "internet.h"

#include "ReplicatorPeer.h"

ReplicatorPeer::ReplicatorPeer( void )
		: m_sinfulString( NULL ),
		  m_hostName( NULL ),
		  m_timeout( 0 ),
		  m_isPrimary( false )
{
}

bool
ReplicatorPeer::init( const char *sinful )
{
	if ( NULL == sinful ) {
		return false;
	}

	m_sinfulString = strdup( sinful );
	if ( NULL == m_sinfulString ) {
		return false;
	}

	m_hostName = getHostFromAddr( m_sinfulString );
	if ( NULL == m_hostName ) {
		dprintf( D_ALWAYS,
				 "Can't find hostname for sinful %s", m_sinfulString );
		return false;
	}

	return true;
}

ReplicatorPeer::~ReplicatorPeer( void )
{
	if ( m_sinfulString ) {
		free( m_sinfulString );
		m_sinfulString = NULL;
	}
	if ( m_hostName ) {
		free( m_hostName );
		m_hostName = NULL;
	}
}

bool
ReplicatorPeer::sendMessage( int command, const ClassAd *ad ) const
{
    Daemon		daemon( DT_ANY, m_sinfulString );

    // no retries after 'm_connectionTimeout' seconds of unsuccessful
    // connection
	if ( m_timeout ) {
		sock.timeout( m_timeout );
		sock.doNotEnforceMinimalCONNECT_TIMEOUT( );
	}

	ReliSock	sock;
    if( ! sock.connect( m_sinfulString, 0, false ) ) {
        dprintf( D_ALWAYS,
				 "::startMessage(): unable to connect to %s\n",
				 m_sinfulString );
		sock.close( );
        return false;
    }

    if( ! daemon.startCommand( command, &sock, m_timeout ) ) {
        dprintf( D_ALWAYS,
				 "::startMessage() cannot start command %s to %s\n",
				 utilToString(command), m_sinfulString );
		sock.close( );
        return false;
    }

	if ( NULL != ad ) {
		ClassAd	*adptr = const_cast<ClassAd *>( ad );
		if ( !adptr->put(sock) ) {
			dprintf( D_ALWAYS,
					 "::startMessage(): failed to send classad to %s\n",
					 m_sinfulString );
			sock.close( );
			return false;
		}
	}

	if( ! sock.eom( ) ) {
		sock.close( );
       	dprintf( D_ALWAYS,
				 "::sendCommand(): failed to send EOM\n" );
       	return false;
   	}
	return true;
}

bool
ReplicatorPeer::operator == ( const char *hostname ) const
{
	return ( 0 == strcasecmp(hostname.Value(), m_hostname) );
}

bool
ReplicatorPeer::operator == ( const ReplicatorPeer &other ) const
{
	return *this == other.getHostName( );
}


// Peer list manager
ReplicatorPeerList::ReplicatorPeerList( void )
{
}

ReplicatorPeerList::~ReplicatorPeerList( void )
{
	if ( m_rawList ) {
		delete m_rawList;
		m_rawList = NULL;
		free( m_rawString );
		m_rawString = NULL;
	}
}

bool
ReplicatorPeerList::init( const char *replication_list, bool &updated )
{
	StringList	*new_list = new StringList( tmp );

	if ( NULL == new_list ) {
		return false;
	}
	if ( NULL == m_rawList ) {
		updated = true;
	}
	else if ( m_rawList->similar(*new_list) == false ) {
		updated = true;
		delete m_rawList;
		free( m_rawString );
	}

	if ( !updated ) {
		return true;
	}

	// Empty the existing list of peers
	list <ReplicatorPeer *>::iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		delete peer;
	}
	m_peers.clear();

	m_rawList   = new_list;
	m_rawString = m_raw_list->print_to_string();

	char		*raw;
	const char	*my_sinful = daemonCore->InfoCommandSinfulString( );
	bool	 	 my_sinful_found = false;

    m_rawList->rewind( );
    while( ( raw = m_rawList->next()) != NULL ) {
        char* sinful = utilToSinful( replicationAddress );

        if( sinful == NULL ) {
            char tmp[256];
			snprintf( tmp, sizeof(tmp),
					  "Peers::init(): invalid address '%s'\n", raw );
            utilCrucialError( tmp );
            continue;
        }
        if( 0 == strcmp( sinful, my_sinful ) ) {
            my_sinful_found = true;
			free( sinful );
        }
        else {
			ReplicationPeer	*peer = new ReplicationPeer( );
			if ( !peer.init(sinful) ) {
				dprintf( D_ALWAYS, "Failed to initialize peer '%s'\n",sinful );
				delete peer;
				free( sinful );
				return false;
			}
            m_sinfulList.push_back( sinful );
        }
        // pay attention to release memory allocated by malloc with free and by
        // new with delete here utilToSinful returns memory allocated by malloc
    }

    if( ! my_sinful_found ) {
        utilCrucialError( "My address is not present in REPLICATION_LIST" );
		return false;
    }
	return true;
}

bool
ReplicatorPeerList::setConnectionTimeout( int timeout )
{
	list <ReplicatorPeer *>::iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		peer->setConnectionTimeout( timeout );
	}
	return true;
}
