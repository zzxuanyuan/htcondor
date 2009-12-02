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
#include "condor_attributes.h"
#include "internet.h"

#include "Utils.h"
#include "ReplicatorPeer.h"
#include "ReplicatorPeerList.h"


// Peer list manager
ReplicatorPeerList::ReplicatorPeerList( void )
		: m_stringList( NULL )
{
}

ReplicatorPeerList::~ReplicatorPeerList( void )
{
	if ( m_stringList ) {
		delete m_stringList;
		m_stringList = NULL;
	}
}

bool
ReplicatorPeerList::init( const char &replication_list, bool &updated )
{
	StringList	*new_list = new StringList( &replication_list );

	if ( NULL == new_list ) {
		return false;
	}
	if ( NULL == m_stringList ) {
		updated = true;
	}
	else if ( m_stringList->similar(*new_list, false) == false ) {
		updated = true;
		delete m_stringList;
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

	m_stringList = new_list;

	char	*raw;
	Sinful	 my_sinful( daemonCore->InfoCommandSinfulString( ) );
	bool	 my_sinful_found = false;

    m_stringList->rewind( );
    while( ( raw = m_stringList->next()) != NULL ) {
		char	*t = utilToSinful( raw );
        Sinful	sinful( t );

        if( ! sinful.valid() ) {
			dprintf( D_ALWAYS,
					 "Peers::init(): invalid address '%s'/'%s'\n", raw, t );
			free( t );
			return false;
        }
		dprintf( D_FULLDEBUG,
				 "Looking at '%s': sinful='%s', mysinful='%s'\n",
				 t, sinful.getSinful(), my_sinful.getSinful() );
		free( t );
        if( sinful.addressPointsToMe(my_sinful) ) {
			dprintf( D_FULLDEBUG, "Matched\n" );
            my_sinful_found = true;
        }
        else {
			ReplicatorPeer	*peer = createPeer( );
			if ( !peer->init(sinful) ) {
				dprintf( D_ALWAYS, "Failed to initialize peer '%s'\n",
						 sinful.getSinful() );
				delete peer;
				return false;
			}
            m_peers.push_back( peer );
        }
        // pay attention to release memory allocated by malloc with free and by
        // new with delete here utilToSinful returns memory allocated by malloc
    }

    if( ! my_sinful_found ) {
		char	*t;
		t = m_stringList->print_to_string( );
		dprintf( D_ALWAYS,
				 "My address '%s' is not present in REPLICATION_LIST '%s'\n",
				 my_sinful.getSinful(), t );
		free( t );
		return false;
    }
	return true;
}

int
ReplicatorPeerList::numActivePeers( void ) const
{
	int		count = 0;
	list <ReplicatorPeer *>::const_iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		if ( peer->isValidState( ) ) {
			count++;
		}
	}
	return count;
}

ReplicatorPeer *
ReplicatorPeerList::findPeer( const ReplicatorPeer &other )
{
	list <ReplicatorPeer *>::iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		if ( *peer == other ) {
			return peer;
		}
	}
	return NULL;
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

bool
ReplicatorPeerList::sendMessage( int command,
								 const ClassAd *ad,
								 int &success_count ) const
{
	success_count = 0;
	list <ReplicatorPeer *>::const_iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		const ReplicatorPeer	*peer = *iter;
		if ( !peer->sendMessage( command, ad ) ) {
			dprintf( D_ALWAYS,
					 "Failed to send message %d to peer\n", command );
		}
		else {
			success_count++;
		}
	}
	return true;
}

bool
ReplicatorPeerList::updateAd( ClassAd &ad ) const
{
	char	*tmp;

	if ( !m_stringList ) {
		dprintf( D_ALWAYS, "PeerList::updateAd: I'm not initialized\n" );
		return false;
	}
    tmp = m_stringList->print_to_string();
    ad.Assign( ATTR_HAD_REPLICATION_LIST, tmp );
	free( tmp );

	return true;
}

bool
ReplicatorPeerList::resetAllState( void )
{
	bool	rval = true;
	list <ReplicatorPeer *>::iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		if ( !peer->resetState( ) ) {
			rval = false;
		}
	}
	return rval;
}

bool
ReplicatorPeerList::setAllState( ReplicatorVersion::State state )
{
	bool	rval = true;
	list <ReplicatorPeer *>::iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		if ( !peer->setState( state ) ) {
			rval = false;
		}
	}
	return rval;
}

bool
ReplicatorPeerList::updatePeerVersion( const ReplicatorRemoteVersion &remote )
{
	list <ReplicatorPeer *>::iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		if ( *peer == remote.getPeer() ) {
			return peer->update( remote );
		}
	}
	dprintf( D_ALWAYS,
			 "Couldn't find matching peer <%s>\n",
			 remote.getPeer().getSinfulStr() );
	return false;
	
}

const ReplicatorPeer *
ReplicatorPeerList::findBestPeerVersion( ReplicatorVersion &best ) const
{
	ReplicatorPeer	*best_peer = NULL;
	best.reset( );
	list <ReplicatorPeer *>::const_iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		if (  best.sameGid(peer->getVersionInfo())  &&
			  ( peer->getVersionInfo() > best )  )    {
			best.setState( peer->getVersionInfo() );
			best_peer = peer;
		}
	}

	return best_peer;
}

bool
ReplicatorPeerList::allSameGid( const ReplicatorVersion &version ) const
{
	list <ReplicatorPeer *>::const_iterator iter;
	for( iter = m_peers.begin(); iter != m_peers.end(); iter++ ) {
		ReplicatorPeer	*peer = *iter;
		if ( !peer->sameGid(version) ) {
			return false;
		}
	}
	return true;
}
