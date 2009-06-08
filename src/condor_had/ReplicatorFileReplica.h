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

#ifndef REPLICATOR_FILE_REPLICA_H
#define REPLICATOR_FILE_REPLICA_H

#include "ReplicatorTransferer.h"
#include "ReplicatorFileVersion.h"
#include "ReplicatorPeer.h"
#include "Utils.h"
#include "reli_sock.h"


// the state of replicator daemon
enum ReplicatorState {
	VERSION_REQUESTING = 0,
	VERSION_DOWNLOADING = 1,
	BACKUP,
	REPLICATION_LEADER
};

// Pre-declare a couple of classes
class ReplicatorFile;			// Pre-declare the file info
class ReplicatorFileReplica;	// Pre-declare the file version info

/* Class      : ReplicatorUploader
 * Description: class, representing a version of state file, including gid of
 *				the pool, logical clock and last modification time of the state
 *				file
 */
class ReplicatorUploader : public ReplicatorTransferer
{
  public:
	ReplicatorUploader( ReplicatorFileReplica &replica )
		: m_replica( replica ) {
	};
	~ReplicatorUploader( void ) { };
	ReplicatorFileBase &getFileInfo( void );

  private:
	ReplicatorFileReplica	&m_replica;
};

/* Class      : ReplicatorFileReplica
 * Description: class, representing a replica of state file, including gid of
 *				the pool, logical clock and last modification time of the state
 *				file
 */
class ReplicatorFileReplica : public ReplicatorFileVersion
{
public:

    /* Function: ReplicatorFileReplica constructor
     */
	ReplicatorFileReplica( const ReplicatorFile &, const ReplicatorPeer &peer );


	// ==== Operations ====

	/* Function   : initialize
	 * Arguments  : pStateFilePath - OS path to state file
	 *  			pVersionFilePath - OS path to version file
	 * Description: initializes all data members
	 */
    bool initialize( const ReplicatorFile & );


	// ==== End of operations ====


	// ==== Inspectors ====

# if 0
	/* Function    : getGid
     * Return value: int - gid
     * Description : returns gid
     */
    int getGid(void) const { return m_gid; };

    /* Function    : getLogicalClock
     * Return value: int - logical clock
     * Description : returns logical clock
     */
	int getLogicalClock(void) const { return m_logicalClock; };
# endif

	/* Function    : getFileInfo
	 * Return value: ReplicatorFile - Information on the replicated file
	 * Description : returns the related file info object
	 */
    const ReplicatorFile &getFileInfo(void) const {
		return m_fileInfo;
	};

	/* Function    : getPeerInfo
	 * Return value: ReplicatorPeer - Information on the peer
	 * Description : returns the related peer info object
	 */
    const ReplicatorPeer &getPeerInfo(void) const {
		return m_peerInfo;
	};

	/* Function    : getUploader
	 * Return value: ReplicatorUploader - Uploader object
	 * Description : returns the uploader object
	 */
	ReplicatorUploader &getUploader( void ) const {
		return m_uploader;
	};


	// ==== End of inspectors ====


	// ==== Comparison operators ====

	/* Function    : isSameHost
	 * Arguments   : hostname - the hostname to compare to
	 * Return value: bool - true/false value
     * Description : Returns true if the hostnames are the same
     */
    bool isSameHost(const char *hostname) const {
		return m_peerInfo == hostname;
	};

	/* Function    : isSameHost
	 * Arguments   : other - Replica to compare to
	 * Return value: bool - true/false value
     * Description : Returns true if the hostnames are the same
     */
    bool isSameHost(const ReplicatorFileReplica &other) const {
		return other.getPeerInfo() == m_peerInfo;
	};

	// ==== End of comparison operators ====


	// ==== Mutators ====

	/* Function   : setState
	 * Arguments  : newState - new state of the replication daemon to send to
	 *				the newly joined machine
     * Description: sets the state of the replication daemon to send to the
	 *				newly joined machine
     */
    void setState(const ReplicatorState& newState) { m_state = newState; };

	/* Function   : setState
     * Arguments  : replica - the replica, the state of which is assigned to
     *                        the current replica's one
     * Description: sets the state of the replication daemon to send to the
     *              newly joined machine as the specified replica's one
     */
    void setState(const ReplicatorFileReplica &replica) {
		m_state = replica.getState();
	};

	// ==== End of mutators ====


	// ==== Convertors ====

	/* Function    : toString
     * Return value: MyString - string representation of Version object
	 * Description : represents the Version object as string
     */
    MyString toString( void ) const;

	// ==== End of convertors ====


	// === Private methods ===
  private:

	/* Function    : getState
     * Return value: ReplicatorState - this replication daemon current state
     * Description : returns this replication daemon current state
     */   
    const ReplicatorState& getState( void ) const { return m_state; };

	
	//  === Private data ===
  private:

	// File info
	const ReplicatorFile		&m_fileInfo;

	// Peer info
	const ReplicatorPeer		&m_peerInfo;

	// My process data
	mutable ReplicatorUploader	 m_uploader;
 
	// components of the version
	ReplicatorState       		 m_state;

	// added support for conservative policy of accepting updates from
	// primary HAD machines only
	bool    					 m_isPrimary;
};

#endif // REPLICATOR_FILE_REPLICA_H
