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

#ifndef ABSTRACT_REPLICATOR_STATE_MACHINE_H
#define ABSTRACT_REPLICATOR_STATE_MACHINE_H

#include "Utils.h"
#include "ReplicatorTransferer.h"
#include "ReplicatorFileReplica.h"
#include "ReplicatorFile.h"
#include "ReplicatorFileSet.h"
#include "reli_sock.h"
#include "dc_service.h"
#include "list.h"
#include <list>

// using namespace std;

/* Class      : AbstractReplicatorStateMachine
 * Description: base abstract class for replication service state machine,
 *              contains useful functions for implementation of replication,
 *              such as broadcasting version, downloading/uploading etc.
 *              Contains handlers for notifications from HAD about its
 *              transitions in its state machine:
 *              1) beforePassiveStateHandler - for HAD_BEFORE_PASSIVE_STATE
 *              2) afterElectionStateHandler - for HAD_AFTER_ELECTION_STATE
 *              3) afterLeaderStateHandler   - for HAD_AFTER_LEADER_STATE
 *              4) inLeaderStateHandler      - for HAD_IN_LEADER_STATE
 *
 *              Besides, it contains handlers for selection of the
 *              best version out of versions list and selection of the
 *              gid according to those versions:
 *              1) replicaSelectionHandler
 *              2) gidSelectionHandler
 */
class AbstractReplicatorStateMachine: public Service
{
  public:

    /* Function: AbstractReplicatorStateMachine constructor
     */
    AbstractReplicatorStateMachine( void );

	/* Function: AbstractReplicatorStateMachine destructor
	 */
    virtual ~AbstractReplicatorStateMachine( void ) = 0;

	/* Function   : reinitialize
	 * Description: rereads all the configuration parameters and resets all
	 *              the data members
	 */
	bool reinitialize( void );


	// ==== Selection handlers ====

	/* Function    : replicaSelectionHandler
	 * Description : generic handler for selection of the best version out of
     *               versions list
     */
    virtual bool
		replicaSelectionHandler( ReplicatorFileReplica& newVersion ) = 0;

	/* Function   : gidSelectionHandler
     * Description: generic handler for selection of gid for the pool
     */
    virtual void gidSelectionHandler( void ) = 0;

	// ==== End of selection handlers ====

  protected:
	// version sending commands between replication daemons
    typedef bool (AbstractReplicatorStateMachine::*CommandFunction)
		( ReliSock& );

    /* Function    : downloadReaper
	 * Arguments   : service    - the daemon, for which the transfer has ended
	 *				 pid        - id of the downloading 'condor_transferer'
	 *							  process
	 *				 exitStatus - return value of the downloading
	 *							 'condor_transferer' process
     * Return value: int - success/failure value
	 * Note        : returns 0 upon success, 1 - upon failure
	 * Description : reaper of downloading 'condor_transferer' process
     */
	static int
		downloadReaper( Service* service, int pid, int exitStatus );

	/* Function    : uploadReaper
     * Arguments   : service    - the daemon, for which the transfer has ended
     *               pid        - id of the uploading 'condor_transferer'
     *                            process
     *               exitStatus - return value of the uploading
     *                           'condor_transferer' process
	 * Return value: int - success/failure value
     * Note        : returns 0 upon success, 1 - upon failure
     * Description : reaper of uploading 'condor_transferer' process
     */
    static int
		uploadReaper( Service* service, int pid, int exitStatus );

	/* Function   : broadcastMessage
	 * Arguments  : command - id that is sent to other replication daemons
	 *				along with the local version
	 * Description: broadcasting different commands to other
	 *				replication daemons along with the local version
	*/
    bool broadcastMessage( int command );

	/* Function   : requestVersions
     * Description: sending command to other replication daemons, asking them
	 * 				to send their replica versions to this replication daemon
     */
    bool requestVersions( void );

	/* Function    : download
	 * Arguments   : ReplicatorFileReplica - Replica to download from
	 * Return value: bool - success/failure value
	 * Description : starts downloading 'condor_transferer' process to
	 *				 download the version of remote replication daemon
     */
    bool download( ReplicatorFileReplica &version );

	/* Function    : upload
	 * Arguments   : ReplicatorFileReplica - Replica to upload from
	 * Return value: bool - success/failure value
	 * Description : starts downloading 'condor_transferer' process to upload
	 *				 the version of remote replication daemon
     */
    bool upload( ReplicatorFileReplica &version );

	/* Function   : reset
	 * Description: clears and resets all inner structures and data members
	 */
    bool reset( void );

    /* Function   : initializeReplicationList
     * Description: initializes replication daemons list from the given string
     */
    void initializeReplicationList( void );

	/* Function   : updateVersionsList
	 * Arguments  : newVersion - the version to update the versions' list
	 * Description: updates list of versions with new version
	 */
    void updateVersionsList( ReplicatorFileReplica &newVersion );

	/* Function   : cancelVersionsListLeader
     * Description: sets the state of all versions in the list to BACKUP
     */
    void cancelVersionsListLeader( void );

	/* Function   : sendCommand
	 * Arguments  : command            - id
	 *				daemonSinfulString - remote replication daemon address
	 *				function           - function that adds specific data that
	 *									 is to be sent
     * Description: generic function to send any command to the remote
	 *				replication daemon; the given function adds specific
	 *				data structures to send according to the kind of message
	 *				needed. For example, for sending version through this
	 *				function the 'function' must be able to encode the local
	 *				version to the socket
     */
    void sendCommand(int command, char* daemonSinfulString,
                     CommandFunction function);

	/* Function   : sendVersionAndStateCommand
	 * Arguments  : command            - id
	 *				daemonSinfulString - remote replication daemon address
     * Description: this function demonstrates a usage of more general
	 *				'sendCommand'; it sends to the remote daemon the local
	 *				version and the state of this replication daemon
     */
    void sendVersionAndStateCommand(int command, char* daemonSinfulString) {
        sendCommand( command, daemonSinfulString,
					 &AbstractReplicatorStateMachine::versionAndStateCommand );
    };

	// ==== Command functions ====

	/* Function    : versionAndStateCommand
     * Arguments   : socket - socket through which the data is send to the
	 *						  remote replication daemon
	 * Return value: success/failure value
     * Description : specific command function, adding to the socket the local
	 *				 version and the state of this replication daemon
     */
    bool versionAndStateCommand(ReliSock& socket);

	/* Function    : versionCommand
     * Arguments   : socket - socket through which the data is send to the
     *                        remote replication daemon
     * Return value: success/failure value
     * Description : specific command function, adding to the socket the local
     *               version
     */
    bool versionCommand(ReliSock& );

	/* Function    : noCommand
     * Arguments   : socket - socket through which the data is send to the
     *                        remote replication daemon
     * Return value: success/failure value
     * Description : specific command function, adding nothing to the socket
     */
    bool noCommand(ReliSock& ) { return true;  };

	// ==== End of command functions ====

	/* Function    : findUploadTransferer
	   Arguments   : pid - PID of the process to find
     * Description : Find the transferer info related to the PID
     */
	ReplicatorTransferer *findUploader( int pid ) {
		return m_uploaders.Find( pid );
	};

	/* Function    : findDownloadTransferer
	   Arguments   : pid - PID of the process to find
     * Description : Find the transferer info related to the PID
     */
	ReplicatorTransferer *findDownloader( int pid ) {
		return m_downloaders.Find( pid );
	};

	/* Function   : numActiveDownloads
	 * Description: returns number of running downloading 'condor_transferers'
	 */
    int numActiveDownloads( void ) const {
		return m_downloaders.numActive( );
	};

	/* Function    : killTransferers
     * Description : kills all the uploading and downloading transferers
     */
    void killTransferers( void );

  protected:

    // list of all of the files we replicate
	ReplicatorFileSet       *m_fileSet;

	// configuration variables
	MyString                 m_transfererPath;

	// the replication daemon state
    ReplicatorState			 m_state;

	// All of our peers
	ReplicatorPeerList		 m_peerList;

	// list of all replicator transfer processes
	ReplicatorUploaderList	 m_uploaders;
	ReplicatorDownloaderList m_downloaders;

	// socket connection timeout
    int                      m_connectionTimeout;

	// uploading/downloading 'condor_transferer' reapers' ids
	int                      m_downloadReaperId;
    int                      m_uploadReaperId;

	void printDataMembers( void ) const;
};

#endif // ABSTRACT_REPLICATOR_STATE_MACHINE_H
