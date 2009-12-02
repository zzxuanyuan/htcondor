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

#ifndef REPLICATOR_PEER_LIST_H
#define REPLICATOR_PEER_LIST_H

#include <list>
#include "string_list.h"
#include "ReplicatorPeer.h"

using namespace std;

// List of peers
class ReplicatorPeerList
{
  public:
	ReplicatorPeerList( void );
	virtual ~ReplicatorPeerList( void );

	bool init( const char &replication_list, bool &modified );
	int numPeers( void ) const {
		return m_peers.size();
	};
	int numActivePeers( void ) const;

	// Find a peer
	ReplicatorPeer *findPeer( const ReplicatorPeer &other );

	// Update the classad
	bool updateAd( ClassAd &ad ) const;

	// Update a peer
	bool updatePeerVersion( const ReplicatorRemoteVersion & );
	bool setAllState( ReplicatorVersion::State );
	bool resetAllState( void );

	// Select best peer / version
	virtual ReplicatorPeer *createPeer( void ) const {
		return new ReplicatorPeer;
	};
	virtual const ReplicatorPeer *findBestPeerVersion(
		ReplicatorVersion & ) const;
	virtual bool allSameGid( const ReplicatorVersion &version ) const;

	// IPC methods
	bool setConnectionTimeout( int timeout );
    bool sendMessage( int command,
					  const ClassAd *ad,
					  int &success_count ) const;
	
  private:
	StringList				*m_stringList;
    list<ReplicatorPeer *>	 m_peers;
};

#endif // REPLICATOR_PEER_LIST_H

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// c-basic-offset:4 ***
// End: ***
