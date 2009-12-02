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

#ifndef RSM_BASE_H

#define RSM_BASE_H

#include "condor_common.h"
#include <list>
#include "dc_service.h"
#include "ReplicatorDownloader.h"
#include "ReplicatorUploader.h"
#include "ReplicatorFileSet.h"
#include "ReplicatorLocalVersion.h"
#include "ReplicatorPeerList.h"
#include "ReplicationBase.h"
#include "SafeFileOps.h"
#include "Utils.h"

/* Class      : BaseRsm
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
 *
 *              1) replicaSelectionHandler
 *              2) gidSelectionHandler
 */
class BaseRsm: public ReplicationBase
{
  public:

    /* Function: BaseRsm constructor
     */
    BaseRsm( void );

	/* Function: BaseRsm destructor
	 */
    virtual ~BaseRsm(void ) = 0;

	/* Function   : reinitialize
	 * Description: rereads all the configuration parameters and resets all the
	 *              data members
	 */
    virtual bool preInit( void );
    virtual bool postInit( void );
    bool reconfig( void );

	virtual bool updateAd( ClassAd &ad );

	// ** Notification handlers

    /* Function   : beforePassiveStateHandler
	 * Description: generic handler before the event, when HAD entered PASSIVE
	 *              state
	 */
    virtual bool handleBeforePassiveState( const ReplicatorPeer &,
										   const ClassAd & ) = 0;

	/* Function   : afterElectionStateHandler
	 * Description: generic handler after the event, when HAD is in transition
	 *              from ELECTION to LEADER state
	 */
    virtual bool handleAfterElectionState( const ReplicatorPeer &,
										   const ClassAd & ) = 0;

	/* Function   : afterLeaderStateHandler 
     * Description: generic handler after the event, when HAD is in transition
	 *              from LEADER to PASSIVE state 
	 */
    virtual bool handleAfterLeaderState( const ReplicatorPeer &,
										 const ClassAd & ) = 0;

	/* Function   : inLeaderStateHandler 
     * Description: generic handler after the event, when HAD is in inner loop
     *              of LEADER state
     */
    virtual bool handleInLeaderState( const ReplicatorPeer &,
									  const ClassAd & ) = 0;

	// ** Selection handlers

	/* Function    : replicaSelectionHandler
	 * Description : generic handler for selection of the best version out of
     *               versions list
     */
    virtual bool selectReplica(
		ReplicatorRemoteVersionMutable &newVersion) const = 0;

	/* Function   : gidSelectionHandler
     * Description: generic handler for selection of gid for the pool
     */
    virtual bool handleGidSelection( void ) = 0;

  protected:
	// version sending commands between replication daemons
    typedef bool (BaseRsm::*CommandFunction)(ReliSock& );

    /* Function    : downloadReplicaTransfererReaper
	 * Arguments   : service    - the daemon, for which the transfer has ended
	 *				 pid        - id of the downloading 'condor_transferer'
	 *							  process
	 *				 exitStatus - return value of the downloading
	 *							 'condor_transferer' process
     * Return value: int - success/failure value
	 * Note        : returns 0 upon success, 1 - upon failure
	 * Description : reaper of downloading 'condor_transferer' process 
     */
	static bool dcDownloadReaper(Service* service, int pid, int exitStatus);
	virtual bool downloadReaper( int pid, int exitStatus );

	/* Function    : uploadReplicaTransfererReaper
     * Arguments   : service    - the daemon, for which the transfer has ended
     *               pid        - id of the uploading 'condor_transferer' 
     *                            process
     *               exitStatus - return value of the uploading 
     *                           'condor_transferer' process
	 * Return value: int - success/failure value 
     * Note        : returns 0 upon success, 1 - upon failure
     * Description : reaper of uploading 'condor_transferer' process
     */
    static bool dcUploadReaper(Service* service, int pid, int exitStatus);
	virtual bool uploadReaper( int pid, int exitStatus );

	/* Function   : sendMessage
	 * Arguments  : command - id that is sent to other replication daemons
	 *						  along with the local version
	 * Description: broadcasting different commands to other replication
	 *				daemons along with the local version
	 */
    bool sendMessage( int command, bool sendClassAd ) {
		return sendMessage( command, sendClassAd, 0, NULL );
	};
	bool sendMessage( int command, bool sendClassAd,
					  int debugflags, const char *cmdstr );

	/* Function   : requestVersions 
     * Description: sending command to other replication daemons, asking
	 *				them to send their replica versions to this replication
	 *				daemon 
     */
    void requestVersions( void );

	/* Function    : download
	 * Arguments   : peer - peer to download from
	 * Return value: bool - success/failure value
	 * Description : starts downloading 'condor_transferer' process to download
	 *				 the version of remote replication daemon
     */
    bool startDownload( const ReplicatorPeer &peer,
						int time_limit );

	/* Function    : startUpload
     * Arguments   : peer - peer to upload to
	 * Return value: bool - success/failure value
     * Description : starts uploading 'condor_transferer' process to upload
     *               the version to remote replication daemon
     */
    bool startUpload( const ReplicatorPeer &peer,
					  int time_limit );

	/* Function    : startTransfer
     * Arguments   : peer - peer to upload to
     *				 reaper_id
     *				 download
     *				 transferer
	 * Return value: bool - success/failure value
     * Description : starts uploading 'condor_transferer' process to upload
     *               the version to remote replication daemon
     */
	bool startTransfer( const ReplicatorPeer &peer,
						int reaper_id,
						bool download,
						ReplicatorTransferer &transferer,
					  int time_limit );

	/* Function   : cleanup
	 * Description: clears and resets all inner structures and data members
	 */
    void cleanup( void );

    /* Function   : initializeReplicationList
	 * Arguments  : buffer - the string to initialize the replication daemons'
	 *						 list from
     * Description: initializes replication daemons list from the given string 
     */ 
    void initializeReplicationList(char* buffer);

	/* Function   : updateVersionsList
	 * Arguments  : newVersion - the version to update the versions' list
	 * Description: updates list of versions with new version
	 */
	bool updateVersions( ReplicatorRemoteVersion &newVersion );

	/* Function   : cancelVersionsListLeader 
     * Description: sets the state of all versions in the list to BACKUP
     */
    void cancelVersionsListLeader();

	/* Function    : killTransferers
     * Description : kills all the uploading and downloading transferers
     */
    void killTransferers( void );
    void killStuckDownload( time_t now );
    void killStuckUploads( time_t now );

	void printDataMembers( void ) const;

  protected:

	// Our public ad
	ClassAd					 m_classAd;

    // list of versions sent to the daemon during JOINING state
	//list<ReplicatorVersion>	 m_versionList;

	// configuration variables
	MyString				 m_downloaderPath;
	MyString				 m_uploaderPath;

	// All of our peers
	ReplicatorPeerList		 m_peers;

	// list of all replicator transfer processes & reapers
    int						 m_uploadReaperId;
	ReplicatorUploaderList	 m_uploaders;
    int						 m_downloadReaperId;
	ReplicatorDownloader	 m_downloader;

	// socket connection timeout
    int						 m_connectionTimeout;
};

#endif // RMS_BASE_H
