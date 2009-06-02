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

    /* Function: ReplicatorFileVersion constructor
     */
	ReplicatorPeer( const char *sinful );

    /* Function    : getSinfulString
     * Return value: MyString: the sinful string
     * Description : returns the sinful string
     */
    const char *getSinful(void) const { return m_sinfulString.Value(); };

	/* Function    : getHostName
	 * Return value: MyString - this replication daemon host name
	 * Description : returns this replication daemon host name
	 */
    const char *getHostName(void) const { return m_hostName.Value(); };

	/* Function    : setPrimary
	 * Arguments   : bool - value to set to
	 * Return value: bool - true/false value
     * Description : Sets the state of primary for this peer
     */
    bool isPrimary( bool primary ) { return m_isPrimary = primary; };

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


private:
    MyString	 m_sinfulString;
	MyString	 m_hostName;
	bool		 m_isPrimary;
};

#endif // REPLICATOR_PEER_H
