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

#ifndef REPLICATOR_PEER_H
#define REPLICATOR_PEER_H

#include "condor_common.h"
#include "condor_sinful.h"
#include "MyString.h"
#include "ReplicatorRemoteVersion.h"

class ReliSock;

/* Class      : ReplicatorPeer
 * Description: class, representing a peer replicator
 */
class ReplicatorPeer
{
 public:

	/* Function: ReplicatorPeer constructor
	 */
	ReplicatorPeer( void );
	ReplicatorPeer( const ReplicatorPeer &other );

    /* Function: ReplicatorPeer destructor
     */
	~ReplicatorPeer( void );

	/* Function:     init()
	 * Return value: bool - true:OK, false:failed
	 * Description:  Use this to do the real initialization
	 */
	bool reset( void );
	bool init( const char *sinful, const char *hostname = NULL );
	bool init( const Sinful &sinful, const char *hostname = NULL );
	bool init( const ReplicatorPeer &other );
	bool update( const ReplicatorRemoteVersion &remote ) {
		return m_version.update( remote );
	};

    /* Function    : getSinfulString
     * Return value: char*: the sinful string
     * Description : returns the sinful string
     */
    const Sinful &getSinful(void) const {
		return *m_sinful;
	};
	const char *getSinfulStr(void) const {
		return m_sinful ? m_sinful->getSinful() : NULL;
	};

	/* Function    : getHostName
	 * Return value: MyString - this replication daemon host name
	 * Description : returns this replication daemon host name
	 */
    const char *getHostName(void) const {
		return m_hostName;
	};

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
    bool setPrimary( bool primary ) {
		return m_version.setIsPrimary( primary );
	};

	/* Function    : isPrimary
	 * Return value: bool - true/false value
     * Description : Returns true if this peer is primary
     */
    bool isPrimary( void ) const {
		return m_version.getIsPrimary( );
	};

	// Get / set state
	bool getState( void ) const {
		return m_version.getState( );
	};
	bool setState( ReplicatorVersion::State state ) {
		return m_version.setState( state );
	};
	bool isState( ReplicatorVersion::State state ) const {
		return m_version.isState( state );
	};
	bool isValidState( void ) const {
		return m_version.isValidState( );
	};
	bool resetState( void ) {
		return m_version.reset( );
	};
	bool isValid( void ) const {
		return m_sinful != NULL;
	};

	// Accessors to version info
	const ReplicatorRemoteVersion &getVersionInfo( void ) const {
		return m_version;
	}
	bool setState( const ReplicatorVersion &version ) {
		return m_version.setState( version );
	};
	bool sameGid( const ReplicatorVersion &version ) const {
		return m_version.sameGid( version );
	};

	// Other accessors
	int getTimeout( void ) const { return m_timeout; };

	/* Function    : operator ==
	 * Arguments   : hostname - the hostname to compare to
	 * Return value: bool - true/false value
     * Description : Returns true if the hostnames are the same
     */
    bool operator == ( const Sinful &sinful ) const;
    bool operator != ( const Sinful &sinful ) const {
		return ( !(*this == sinful) );
	};

	/* Function    : operator ==
	 * Arguments   : hostname - the hostname to compare to
	 * Return value: bool - true/false value
     * Description : Returns true if the hostnames are the same
     */
    bool operator == ( const ReplicatorPeer &other ) const;
    bool operator != ( const ReplicatorPeer &other ) const {
		return ( !(*this == other) );
	};

	/* Function    : sendMessage
	 * Arguments   : int command - command to send
	 * Arguments   : ClassAd - classad to send with command or NULL
	 * Return value: bool
	 * Description : returns true on success, false otherise
	 */
    bool sendMessage( int command,
					  const ClassAd *ad = NULL,
					  ReliSock *sock = NULL ) const;


  private:
	Sinful					*m_sinful;
	const char				*m_sinfulStr;
	char					*m_hostName;
	int			 			 m_timeout;
	ReplicatorRemoteVersion	 m_version;
};

#endif // REPLICATOR_PEER_H
