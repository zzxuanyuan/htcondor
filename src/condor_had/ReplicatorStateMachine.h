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

#ifndef REPLICATOR_STATE_MACHINE_H
#define REPLICATOR_STATE_MACHINE_H

// for 'ReplicatorState'
#include "Utils.h"
#include "ReplicatorTransferer.h"
#include "ReplicatorFileReplica.h"
#include "ReplicatorFile.h"
#include "ReplicatorFileList.h"
#include "AbstractReplicatorStateMachine.h"
#include "reli_sock.h"
#include "dc_service.h"
#include "list.h"
#include <list>

/* Class      : ReplicatorStateMachine
 * Description: concrete class for replication service state machine,
 *              Contains implementation of handlers for notifications from
 *              HAD about its transitions in its state machine:
 *              1) beforePassiveStateHandler - for HAD_BEFORE_PASSIVE_STATE
 *              2) afterElectionStateHandler - for HAD_AFTER_ELECTION_STATE
 *              3) afterLeaderStateHandler   - for HAD_AFTER_LEADER_STATE
 *              4) inLeaderStateHandler      - for HAD_IN_LEADER_STATE
 *
 *              Besides, it contains implementation of handlers for selection
 *              of the best version out of versions list and selection of the
 *              gid according to those versions:
 *              1) replicaSelectionHandler
 *              2) gidSelectionHandler
 */
class ReplicatorStateMachine : public AbstractReplicatorStateMachine
{
  public:
	/* Function: ReplicatorStateMachine constructor
     */
    ReplicatorStateMachine( void );
	/* Function: ReplicatorStateMachine destructor
     */
    virtual ~ReplicatorStateMachine( void );


	// Notification handlers

    /* Function   : beforePassiveStateHandler
     * Description: concrete handler before the event, when HAD entered PASSIVE
     * 				state; broadcasts old local version, solicits versions from
	 *				other replication daemons in the pool
     */
    bool beforePassiveStateHandler( const ClassAd &,
									const ReplicatorPeer &peer );

	/* Function   : afterElectionStateHandler
     * Description: concrete handler after the event, when HAD is in transition
     *              from ELECTION to LEADER state; sets the last time, when HAD
	 *              sent a HAD_IN_STATE_STATE (which is kind of "I am alive"
	 *              message for replication daemon) and selects the new gid for
	 *              the pool
     */
    bool afterElectionStateHandler( const ClassAd &,
									const ReplicatorPeer &peer );

	/* Function   : afterLeaderStateHandler
	 * Description: concrete handler after the event, when HAD is in transition
     *              from ELECTION to PASSIVE state
	 * Remarks    : void by now
	 */
    bool afterLeaderStateHandler( const ClassAd &,
								  const ReplicatorPeer &peer );

	/* Function   : inLeaderStateHandler
     * Description: concrete handler after the event, when HAD is in inner loop
     *              of LEADER state; sets the last time, when HAD sent a
	 * 				HAD_IN_STATE_STATE
     */
    bool inLeaderStateHandler( const ClassAd &,
							   const ReplicatorPeer &peer );


	// Selection handlers

	/* Function    : replicaSelectionHandler
     * Arguments   : newReplica -
     *                  in JOINING state: the version selected for downloading
     *                  in BACKUP state : the version compared against the local
     *                                    one in order to check, whether to
     *                                    download the leader's version or not
     * Return value: bool - whether it is worth to download the remote version
	 *						or not
	 * Description : concrete handler for selection of the best version out of
     *               versions list
     */
    virtual bool replicaSelectionHandler(ReplicatorFileReplica& newReplica);

	/* Function   : gidSelectionHandler
     * Description: concrete handler for selection of gid for the pool
	 * Remarks    : void by now
     */
    virtual void gidSelectionHandler( void );

	/* Function   : initialize
     * Description: initializes all inner structures, such as
	 *				commands, timers, reapers and data members
     */
    bool initialize( void );

	/* Function   : reinitialize
     * Description: reinitializes all inner structures, such as
     *              commands, timers, reapers and data members
     */
    bool reinitialize( void );

  protected:
	/* Function   : downloadReplicaTransfererReaper
     * Arguments  : service    - the daemon, for which the transfer has ended
     *              pid        - id of the downloading 'condor_transferer'
     *                           process
     *              exitStatus - return value of the downloading
     *                           'condor_transferer' process
     * Description: reaper of downloading 'condor_transferer' process
     */
    static int
	downloadReplicaTransfererReaper(Service *service, int pid, int exitStatus);

  private:
	enum CommandClass { CCLASS_HAD, CCLASS_PEER };
	// Managing stuck transferers
	void killStuckDownloadingTransferer(time_t currentTime);
	void killStuckUploadingTransferers (time_t currentTime);

    void registerCommand(int command, CommandClass cc );
    int commandHandlerHad(int command, Stream *stream);
    int commandHandlerPeer(int command, Stream *stream);
	int commandHandlerCommon( int command, Stream *stream,
							  const char *name,
							  ClassAd &ad, ReplicatorPeer &peer );

    bool reset( bool all );

	// Timers handlers
    void replicationTimer( void );
    void replicaRequestTimer( void );
    void replicaDownloadTimer( void );

	// Command handlers
    bool onLeaderVersion( const ClassAd &ad, const ReplicatorPeer &peer );
    bool onTransferFile( const ClassAd &ad, const ReplicatorPeer &peer );
    bool onSolicitVersion( const ClassAd &ad, const ReplicatorPeer &peer );
    bool onSolicitVersionReply( const ClassAd &ad, const ReplicatorPeer &peer );
    bool onNewlyJoinedVersion( const ClassAd &ad, const ReplicatorPeer &peer );
    bool onGivingUpVersion( const ClassAd &ad, const ReplicatorPeer &peer );

    static ReplicatorFileReplica *decodeVersionAndState( Stream *stream );
	bool becomeLeader( void );

	// My "sinful" string
	const char	*m_mySinfulString;

	// Configuration parameters
    int          m_replicationInterval;
    int          m_hadAliveTolerance;
    int          m_maxTransfererLifeTime;
    int          m_newlyJoinedWaitingVersionInterval;

	// Timers
    int          m_replicaRequestTimerId;
    int          m_replicaDownloadTimerId;
    int          m_replicationTimerId;
			    
	// last time HAD sent HAD_IN_LEADER_STATE
    time_t       m_lastHadAliveTime;

	// Debugging utilities
	void printDataMembers(void) const;

	void checkVersionSynchronization( void );

};

#endif // REPLICATOR_STATE_MACHINE_H
