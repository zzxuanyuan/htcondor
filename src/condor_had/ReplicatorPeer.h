/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#ifndef REPLICATOR_PEER_H
#define REPLICATOR_PEER_H

#include "MyString.h"
#include "Utils.h"

/* Class      : ReplicatorPeer
 * Description: class, representing a peer replicator
 */
class ReplicatorPeer
{
public:

    /* Function: ReplicatorPeer constructor
     */
	ReplicatorPeer( void );

    /* Function: ReplicatorPeer destructor
     */
	~ReplicatorPeer( void );

	/* Function:     init()
	 * Return value: bool - true:OK, false:failed
	 * Description:  Use this to do the real initialization
	 */
	bool init( const char *sinful );

    /* Function    : getSinfulString
     * Return value: char*: the sinful string
     * Description : returns the sinful string
     */
    const char *getSinful(void) const { return m_sinfulString; };

    /* Function    : getSinfulString
     * Return value: MyString: the sinful string
     * Description : returns the sinful string
     */
    bool getSinful( MyString &s) const { s = m_sinfulString; return true; };

	/* Function    : getHostName
	 * Return value: MyString - this replication daemon host name
	 * Description : returns this replication daemon host name
	 */
    const char *getHostName(void) const { return m_hostName; };

	/* Function    : setConnectionTimeout
	 * Arguments   : int - timeout value to use for connections
	 * Return value: bool - true/false value
     * Description : Sets the connection timeout
     */
    bool setConnectionTimeout( int timeout ) {
		m_timeout = timeout;
		return true;
	};

	/* Function    : setPrimary
	 * Arguments   : bool - value to set to
	 * Return value: bool - true/false value
     * Description : Sets the state of primary for this peer
     */
    bool setPrimary( bool primary ) { return m_isPrimary = primary; };

	/* Function    : isPrimary
	 * Return value: bool - true/false value
     * Description : Returns true if this peer is primary
     */
    bool isPrimary( void ) const { return m_isPrimary; };

	/* Function    : operator ==
	 * Arguments   : hostname - the hostname to compare to
	 * Return value: bool - true/false value
     * Description : Returns true if the hostnames are the same
     */
    bool operator == (const char *hostname) const;

	/* Function    : operator ==
	 * Arguments   : hostname - the hostname to compare to
	 * Return value: bool - true/false value
     * Description : Returns true if the hostnames are the same
     */
    bool operator == ( const ReplicatorPeer &other ) const;

	/* Function    : startMessage
	 * Arguments   : int command - command to send
	 * Arguments   : ClassAd - classad to send with command or NULL
	 * Return value: bool
	 * Description : returns true on success, false otherise
	 */
    bool sendMessage( int command, const ClassAd *ad ) const;

  private:
	char		*m_sinfulString;
	char		*m_hostName;
	int			 m_timeout;
	bool		 m_isPrimary;
};

// List of peers
class ReplicatorPeerList
{
  public:
	ReplicatorPeerList( void );
	~ReplicatorPeerList( void );

	bool init( const char *replication_list, bool &updated );
	bool setConnectionTimeout( int timeout );
	const char *getRawString( void ) const { return m_rawString; };
	int numPeers( void ) const { return m_peers.size(); };

	list<ReplicatorPeer *> &getPeers( void ) {
		return m_peers;
	};
	list<ReplicatorPeer *> const &getPeersConst( void ) const {
		return m_peers;
	};

  private:
	StringList				*m_rawList;
	char					*m_rawString;

    list<ReplicatorPeer *>	 m_peers;
};

#endif // REPLICATOR_PEER_H
