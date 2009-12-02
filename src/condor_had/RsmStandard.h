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

#ifndef RSM_STANDARD_H
#define RSM_STANDARD_H

#include "RsmBase.h"

/* Class      : StandardRsm
 * Description: concrete class for replication service state machine,
 *              Contains implementation of handlers for notifications from 
 *              HAD about its transitions in its state machine:
 *              1) handleBeforePassiveState - for HAD_BEFORE_PASSIVE_STATE
 *              2) handleAfterElectionState - for HAD_AFTER_ELECTION_STATE
 *              3) handleAfterLeaderState   - for HAD_AFTER_LEADER_STATE
 *              4) handleInLeaderState      - for HAD_IN_LEADER_STATE
 *
 *              Besides, it contains implementation of handlers for selection 
 *              of the best version out of versions list and selection of the 
 *              gid according to those versions:
 *              1) replicaSelectionHandler
 *              2) gidSelectionHandler
 */
class StandardRsm: public BaseRsm
{
  public:
	/* C-tor, D-Tor
     */
    StandardRsm( void );
    virtual ~StandardRsm( void );

	
	// Init / shutdown methods
    virtual bool preInit( void );
    virtual bool postInit( void );
    bool shutdown( void );

    bool reconfig( void );

	/*
	 * Notification handlers
	 */

    /* Function   : handleBeforePassiveState
     * Description: concrete handler before the event, when HAD entered PASSIVE
     * 				state; broadcasts old local version, solicits versions from
	 *				other replication daemons in the pool
     */
    virtual bool handleBeforePassiveState( const ReplicatorPeer &,
										   const ClassAd & );

	/* Function   : handleAfterElectionState
     * Description: concrete handler after the event, when HAD is in transition
     *              from ELECTION to LEADER state; sets the last time, when HAD
	 *              sent a HAD_IN_STATE_STATE (which is kind of "I am alive" 
	 *              message for replication daemon) and selects the new gid for
	 *              the pool 
     */
    virtual bool handleAfterElectionState( const ReplicatorPeer &,
										   const ClassAd & );

	/* Function   : handleAfterLeaderState
	 * Description: concrete handler after the event, when HAD is in transition
     *              from ELECTION to PASSIVE state
	 * Remarks    : void by now
	 */
    virtual bool handleAfterLeaderState( const ReplicatorPeer &,
										 const ClassAd & );

	/* Function   : handleInLeaderState
     * Description: concrete handler after the event, when HAD is in inner loop
     *              of LEADER state; sets the last time, when HAD sent a 
	 * 				HAD_IN_STATE_STATE
     */
    virtual bool handleInLeaderState( const ReplicatorPeer &,
									  const ClassAd & );

	/*
	 * Selection handlers
	 */

	/* Function    : replicaSelectionHandler
     * Arguments   : newVersion -
     *                  in JOINING state: the version selected for downloading
     *                  in BACKUP state : the version compared against the
	 *									  local
     *                                    one in order to check, whether to
     *                                    download the leader's version or not
     * Return value: bool - whether it is worth to download the remote version
	 *						or not
	 * Description : concrete handler for selection of the best version out of
     *               versions list
     */
    virtual bool selectReplica(
		ReplicatorRemoteVersionMutable &newVersion ) const;

	/* Function   : gidSelectionHandler
     * Description: concrete handler for selection of gid for the pool
	 * Remarks    : void by now
     */
    virtual bool handleGidSelection( void );

  protected:

	/* Function   : downloadReaper
     * Arguments  : service    - the daemon, for which the transfer has ended
     *              pid        - id of the downloading 'condor_transferer'
     *                           process
     *              exitStatus - return value of the downloading
     *                           'condor_transferer' process
     * Description: reaper of downloading 'condor_transferer' process
     */
    bool downloadReaper( int pid, int exitStatus );

	/* Function   : uploadReaper
     * Arguments  : service    - the daemon, for which the transfer has ended
     *              pid        - id of the downloading 'condor_transferer'
     *                           process
     *              exitStatus - return value of the downloading
     *                           'condor_transferer' process
     * Description: reaper of uploading 'condor_transferer' process
     */
    static bool
	uploadReaper(Service* service, int pid, int exitStatus);

  private:
	enum CommandClass { CCLASS_HAD, CCLASS_PEER, CCLASS_TRANSFERER };

    bool initializeClassAd( void );
    void updateCollectors( void );

    int commandHandlerHad(int command, Stream* stream);
    int commandHandlerPeer(int command, Stream* stream);
    int commandHandlerTransferer(int command, Stream* stream);
    bool registerCommand( int command, CommandClass cc );

    bool cleanup( bool cleanup_base );

	// Timers handlers
    void handleReplicationTimer( void );
    void handleVersionReqTimer( void );
    void handleDownloadTimer( void );

	// Peer Replicator Command handlers
    bool handleLeaderVersion( const ReplicatorPeer &, const ClassAd & );
    bool handleTransferFile( const ReplicatorPeer &, const ClassAd & );
    bool handleSolicitVersion( const ReplicatorPeer &, const ClassAd & );
    bool handleSolicitVersionReply( const ReplicatorPeer &, const ClassAd & );
    bool handleNewlyJoinedVersion( const ReplicatorPeer &, const ClassAd & );
    bool handleGivingUpVersion( const ReplicatorPeer &, const ClassAd & );

	bool becomeLeader( void );

	/* Function   : downloadTransferersNumber
	 * Description: returns number of running downloading 'condor_transferers'
	 */
    MyString	 m_name;

	// Configuration parameters
    int			 m_updateInterval;
    int			 m_replicationInterval;
    int			 m_aliveTolerance;
    int			 m_maxTransfererLifeTime;
    int			 m_newlyJoinedWaitingVersionInterval;

	// Timers
    int			 m_updateCollectorTimerId;
    int			 m_versionRequestingTimerId;
    int			 m_downloadTimerId;
    int			 m_replicationTimerId;

	// last time HAD sent HAD_IN_LEADER_STATE
    time_t		 m_lastHadAliveTime;

	// Debugging utilities
	void printDataMembers( void );
	void checkVersionSynchronization( void );

};

#endif // RSM_STANDARD_H
