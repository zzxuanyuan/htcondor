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
#include "condor_config.h"
#include "my_username.h"

#include "RsmStandard.h"
#include "FilesOperations.h"

// multiplicative factor, determining how long the active HAD, that does not
// send the messages to the replication daemon, is considered alive
const int ALIVE_TOLERANCE_FACTOR		= 2;
// multiplicative factor, determining how long the newly joining machine is
// allowed to download the version and state files of other pool machines
const int NEWLY_JOINED_TOLERANCE_FACTOR = 2;

StandardRsm::StandardRsm( void )
		: BaseRsm( ),
		  m_updateInterval( -1 ),
		  m_replicationInterval( -1 ),
		  m_aliveTolerance( -1 ),
		  m_maxTransfererLifeTime( -1 ),
		  m_newlyJoinedWaitingVersionInterval( -1 ),

		  m_updateCollectorTimerId( -1 ),
		  m_versionRequestingTimerId( -1 ),
		  m_downloadTimerId( -1 ),
		  m_replicationTimerId( -1 ),

		  m_lastHadAliveTime( -1 )
{
	dprintf( D_ALWAYS, "StandardReplicatorSM ctor\n" );
   	srand( time( NULL ) );
}

// finalizing the delta, belonging to this class only, since the data,
// belonging to the base class is finalized implicitly
StandardRsm::~StandardRsm( void )
{
	dprintf( D_FULLDEBUG, "StandardRsm dtor\n" );
    cleanup( false );
}

/* Function   : cleanup
 * Description: clears and resets all inner structures and data members
 */
bool
StandardRsm::cleanup( bool cleanup_base )
{
	dprintf( D_FULLDEBUG, "StandardRsm::clean\n" );

    utilCancelTimer(m_replicationTimerId);
    utilCancelTimer(m_versionRequestingTimerId);
    utilCancelTimer(m_downloadTimerId);
    utilCancelTimer(m_updateCollectorTimerId);
    m_replicationInterval               = -1;
    m_aliveTolerance                    = -1;
    m_maxTransfererLifeTime             = -1;
    m_newlyJoinedWaitingVersionInterval = -1;
    m_lastHadAliveTime                  = -1;
    m_updateInterval                    = -1;

	if ( cleanup_base ) {
		BaseRsm::cleanup( );
	}
	return true;
}

bool
StandardRsm::shutdown( void )
{
    ClassAd invalidate_ad;
    MyString line;

    invalidate_ad.SetMyTypeName( QUERY_ADTYPE );
    invalidate_ad.SetTargetTypeName( GENERIC_ADTYPE );
    line.sprintf( "%s == \"%s\"", ATTR_NAME, m_name.Value( ) );
    invalidate_ad.AssignExpr( ATTR_REQUIREMENTS, line.Value( ) );
    daemonCore->sendUpdates( INVALIDATE_ADS_GENERIC, &invalidate_ad,
							 NULL, false );
	return true;
}

bool
StandardRsm::preInit( void )
{
    dprintf( D_FULLDEBUG, "StandardRsm::preInit\n" );
	return BaseRsm::preInit( );
}

bool
StandardRsm::postInit( void )
{
    dprintf( D_FULLDEBUG, "StandardRsm::postInit\n" );

    // register commands that the service responds to
    registerCommand( HAD_BEFORE_PASSIVE_STATE, CCLASS_HAD );
    registerCommand( HAD_AFTER_ELECTION_STATE, CCLASS_HAD );
    registerCommand( HAD_AFTER_LEADER_STATE, CCLASS_HAD );
    registerCommand( HAD_IN_LEADER_STATE, CCLASS_HAD );
    registerCommand( REPLICATION_LEADER_VERSION, CCLASS_PEER );
    registerCommand( REPLICATION_TRANSFER_FILE, CCLASS_TRANSFERER );
    registerCommand( REPLICATION_NEWLY_JOINED_VERSION, CCLASS_PEER );
    registerCommand( REPLICATION_GIVING_UP_VERSION, CCLASS_PEER );
    registerCommand( REPLICATION_SOLICIT_VERSION, CCLASS_PEER );
    registerCommand( REPLICATION_SOLICIT_VERSION_REPLY, CCLASS_PEER );

	return BaseRsm::postInit( );
}

// clears all the inner structures and loads the configuration parameters'
// values again
bool
StandardRsm::reconfig( void )
{
    dprintf( D_FULLDEBUG, "StandardRsm::reconfig\n" );
    if ( !BaseRsm::reconfig( ) ) {
		return false;
	}

    // delete all configurations and start everything over from the scratch
    cleanup( false );

    m_replicationInterval =
		param_integer( "REPLICATION_INTERVAL", 5 * MINUTE, 1 );
    m_newlyJoinedWaitingVersionInterval =
        param_integer( "NEWLY_JOINED_WAITING_VERSION_INTERVAL",
					   ( NEWLY_JOINED_TOLERANCE_FACTOR *
						 (m_connectionTimeout + 1) ),
					   1 );

	m_aliveTolerance =
		(  ALIVE_TOLERANCE_FACTOR *
		   ( 2 * m_connectionTimeout * m_peers.numPeers() + 1 )  );

	dprintf( D_FULLDEBUG,
			 "StandardRsm::reconfig %s=%d\n",
			 "HAD_LIST", m_aliveTolerance );

    int updateInterval = param_integer( "REPLICATION_UPDATE_INTERVAL", 300 );
    if ( m_updateInterval != updateInterval ) {
        m_updateInterval = updateInterval;

        utilCancelTimer(m_updateCollectorTimerId);

        m_updateCollectorTimerId = daemonCore->Register_Timer ( 0,
               m_updateInterval,
               (TimerHandlercpp) &StandardRsm::updateCollectors,
               "StandardRsm::updateCollectors", this );
    }

    // set a timer to replication routine
    dprintf( D_ALWAYS,
			 "StandardRsm::reconfig setting replication timer\n" );
    m_replicationTimerId = daemonCore->Register_Timer(
		m_replicationInterval,
		(TimerHandlercpp) &StandardRsm::handleReplicationTimer,
		"Time to replicate file", this );

	// for debugging purposes only
	printDataMembers( );

	if ( !initializeClassAd() ) {
		dprintf( D_ALWAYS, "Failed to initialize my ClassAd\n" );
	}

	ReplicatorPeer	peer;
	ClassAd			ad;
	return handleBeforePassiveState( peer, ad );
}

bool
StandardRsm::initializeClassAd( void )
{
	initAd( m_classAd, "Replicator" );
	updateAd( m_classAd );

	// publish DC attributes
    daemonCore->publish( &m_classAd );
	return true;
}

// sends the version of the last execution time to all the replication daemons,
// then asks the pool replication daemons to send their own versions to it,
// sets a timer to wait till the versions are received
bool
StandardRsm::handleBeforePassiveState( const ReplicatorPeer & /*peer*/,
									   const ClassAd & /*ad*/ )
{
    ASSERT( isState(ReplicatorVersion::STATE_REQUESTING) );

    dprintf( D_FULLDEBUG,
			"StandardRsm::beforePassiveStateHandler\n" );
    sendMessage( REPLICATION_NEWLY_JOINED_VERSION, true,
				 D_FULLDEBUG, "NEWLY_JOINED_VERSION" );
    sendMessage( REPLICATION_SOLICIT_VERSION, false,
				 D_FULLDEBUG, "SOLICIT_VERSION" );

    dprintf( D_FULLDEBUG,
			 "StandardRsm::beforePassiveStateHandler "
			 "registering version requesting timer\n" );
    m_versionRequestingTimerId = daemonCore->Register_Timer(
		m_newlyJoinedWaitingVersionInterval,
		(TimerHandlercpp) &StandardRsm::handleVersionReqTimer,
		"Time to pass to STATE_DOWNLOADING state", this );
	return true;
}

bool
StandardRsm::handleAfterElectionState( const ReplicatorPeer & /*peer*/,
									   const ClassAd & /*ad*/ )
{
    dprintf( D_FULLDEBUG,
			 "StandardRsm::afterElectionStateHandler\n" );
    ASSERT( !isState(ReplicatorVersion::STATE_LEADER) );

	// we stay in STATE_REQUESTING or STATE_DOWNLOADING state
    // of newly joining node, we will go to LEADER_STATE later
    // upon receiving of IN_LEADER message from HAD
    if(  ( isState(ReplicatorVersion::STATE_REQUESTING) )  ||
		 ( isState(ReplicatorVersion::STATE_DOWNLOADING) )  ) {
        return true;
    }

	return becomeLeader( );
}

bool
StandardRsm::handleAfterLeaderState( const ReplicatorPeer & /*peer*/,
									 const ClassAd & /*ad*/ )
{
    if(  isState(ReplicatorVersion::STATE_REQUESTING)   ||
		 isState(ReplicatorVersion::STATE_DOWNLOADING)  ) {
        return true;
    }

	// receiving this notification message in STATE_BACKUP state
	// means, that the // pool version downloading took more time than
	// it took for the HAD to // become active and to give up the
	// leadership, in this case we ignore this notification message
	// from HAD as well, since we do not want to broadcast our newly
	// downloaded version to others, because it is too new
	if( isState(ReplicatorVersion::STATE_BACKUP) ) {
		return true;
	}
	dprintf( D_ALWAYS,
			 "StandardRsm::afterLeaderStateHandler\n" );
    sendMessage( REPLICATION_GIVING_UP_VERSION, true,
				 D_FULLDEBUG, "GIVING_UP_VERSION" );
	setState( ReplicatorVersion::STATE_BACKUP );
	return true;
}

bool
StandardRsm::handleInLeaderState( const ReplicatorPeer & /*peer*/,
								  const ClassAd & /*ad*/ )
{
    dprintf( D_FULLDEBUG,
			 "StandardRsm::inLeaderStateHandler started with state = %d\n",
			 int( getState( ) ) );
    // ASSERT(getState( ) != ReplicatorVersion::STATE_BACKUP)

    if(  isState(ReplicatorVersion::STATE_REQUESTING)  ||
		 isState(ReplicatorVersion::STATE_DOWNLOADING)  ) {
        return true;
    }

	// receiving this notification message in STATE_BACKUP state
    // means, that the pool version downloading took more time than it
    // took for the HAD to become active, in this case we act as if we
    // received AFTER_ELECTION message
    if( isState(ReplicatorVersion::STATE_BACKUP) ) {
		return becomeLeader( );
	}
	m_lastHadAliveTime = time( NULL );

    dprintf( D_FULLDEBUG,
            "StandardRsm::inLeaderStateHandler last HAD alive time "
            "is set to %s", ctime( &m_lastHadAliveTime ) );

    //if( downloadTransferersNumber( ) == 0 &&
	// 	  replicaSelectionHandler( newVersion ) ) {
    //    download( newVersion.getSinfulString( ).Value( ) );
    //}
	return true;
}

bool
StandardRsm::selectReplica( ReplicatorRemoteVersionMutable &new_version ) const
{
    ASSERT(  isState(ReplicatorVersion::STATE_DOWNLOADING) ||
			 isState(ReplicatorVersion::STATE_BACKUP)       );
    dprintf( D_ALWAYS, "StandardRsm::replicaSelectionHandler "
			"started with my version = %s, #active peers = %d\n",
             m_myVersion.toString(), m_peers.numActivePeers() );

    ReplicatorVersion my_version( m_myVersion );

	// in STATE_BACKUP state compares the received version with the local one
    if( isState(ReplicatorVersion::STATE_BACKUP) ) {
		// compares the versions, taking only 'gid' and 'logicalClock' into
		// account - this is the reason for making the states equal
        my_version.setState( new_version );
        return ( new_version.diffGid(my_version) ||
				 ( new_version > my_version )    );
    }

	/* in STATE_DOWNLOADING state selecting the best version from the list of
	 * received versions according to the policy defined by
	 * 'replicaSelectionHandler', i.e. selecting the version with greatest
	 * 'logicalClock' value amongst a group of versions with the same gid
	 */
	ReplicatorSimpleFileSet	 fileset( *m_fileset );
	ReplicatorVersion		 best( fileset, ReplicatorVersion::STATE_INVALID );
	const ReplicatorPeer	*peer;
	peer = m_peers.findBestPeerVersion( best );
	if ( ! peer ) {
		return false;
	}

	// compares the versions, taking only 'gid' and 'logicalClock' into
    // account - this is the reason for making the states equal
    my_version.setState( best );

	// either when the versions are incomparable or when the local version
	// is worse, the remote version must be downloaded
    if( my_version.isComparable(best) && (my_version >= best) ) {
        return false;
    }
    new_version.setState( best );
	new_version.setPeer( *peer );
    dprintf( D_ALWAYS,
			 "StandardRsm: best version selected: %s\n",
			 new_version.toString() );
    return true;
}

// until the state files merging utility is ready, the function is not really
// interesting, it selects 0 as the next gid each time
bool
StandardRsm::handleGidSelection( void )
{
    ASSERT( isState(ReplicatorVersion::STATE_BACKUP) ||
			isState(ReplicatorVersion::STATE_LEADER)  );
    dprintf( D_FULLDEBUG, "StandardRsm::handleGridSelection\n");

	if ( m_peers.allSameGid(m_myVersion) ) {
        dprintf( D_FULLDEBUG, "No need to select new gid\n" );
        return true;
    }

	m_myVersion.setRandomGid( );
    dprintf( D_ALWAYS,
			 "StandardRsm::gidSelectionHandler new gid selected: %d\n",
			 m_myVersion.getGid() );
	return true;
}

/* Function   : becomeLeader
 * Description: passes to leader state, sets the last time, that HAD sent its
 * 				message and chooses a new gid
 */
bool
StandardRsm::becomeLeader( void )
{
	// sets the last time, when HAD sent a HAD_IN_STATE_STATE
	m_lastHadAliveTime = time( NULL );
    dprintf( D_FULLDEBUG,
			 "StandardRsm::becomeLeader last HAD alive time is set to %s",
			 ctime( &m_lastHadAliveTime ) );

	// selects new gid for the pool
    handleGidSelection( );
    setState( ReplicatorVersion::STATE_LEADER );
	return true;
}

/* Function   : handleLeaderVersion
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_LEADER_VERSION command; comparing the
 *				received version to the local one and downloading the replica
 *				from the remote replication daemon when the received version is
 *				better than the local one and there is no downloading
 *				'condor_transferer' running at the same time
 */
bool
StandardRsm::handleLeaderVersion( const ReplicatorPeer &peer,
								  const ClassAd &ad )
{
    dprintf( D_FULLDEBUG, "StandardRsm::LeaderVersionHandler\n" );

    if( isState(ReplicatorVersion::STATE_BACKUP) ) {
        return true;
    }
	checkVersionSynchronization( );

	ReplicatorRemoteVersionMutable	version( peer );
	if ( !version.initialize( ad ) ) {
		dprintf( D_ALWAYS,
				 "handleLeaderVersion: Failed to init version from ad\n" );
		return false;
	}

	// comparing the received version to the local one
    bool downloadNeeded = selectReplica( version );

    // downloading the replica from the remote replication daemon, when the
	// received version is better and there is no running downloading
	// 'condor_transferers'
    if( !m_downloader.isActive() && downloadNeeded ) {
        dprintf( D_FULLDEBUG,
				 "StandardRsm::onLeaderVersion downloading from %s\n",
				 version.getPeer().getSinfulStr() );
        startDownload( version.getPeer(), m_maxTransfererLifeTime );
    }

    // replication leader must not send a version which hasn't been updated
	return true;
}

/* Function   : handleTransferFile
 * Arguments  : daemonSinfulString - the address of remote replication daemon,
 *				which sent the REPLICATION_TRANSFER_FILE command
 * Description: handler of REPLICATION_TRANSFER_FILE command; starting
 *				uploading the replica from specified replication daemon
 */
bool
StandardRsm::handleTransferFile( const ReplicatorPeer &peer,
								 const ClassAd & /*ad*/ )
{
    dprintf( D_FULLDEBUG,
			 "StandardRsm::handleTransferFile <%s> started\n",
             peer.getSinfulStr() );
    if( !isState(ReplicatorVersion::STATE_LEADER) ) {
		return true;
    }

	return startUpload( peer, m_maxTransfererLifeTime );
}

/* Function   : handleSolicitVersion
 * Arguments  : daemonSinfulString - the address of remote replication daemon,
 *              which sent the REPLICATION_SOLICIT_VERSION command
 * Description: handler of REPLICATION_SOLICIT_VERSION command; sending local
 *				version along with current replication daemon state
 */
bool
StandardRsm::handleSolicitVersion( const ReplicatorPeer &peer,
								   const ClassAd & /*ad*/ )
{
    dprintf( D_FULLDEBUG, "StandardRsm::onSolicitVersion %s\n",
             peer.getSinfulStr() );
    if(  isState(ReplicatorVersion::STATE_BACKUP)  ||
		 isState(ReplicatorVersion::STATE_LEADER)  ) {
		return sendMessage( REPLICATION_SOLICIT_VERSION_REPLY, true );
	}
	return true;
}

/* Function   : handleSolicitVersionReply
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_SOLICIT_VERSION_REPLY command; updating
 * 				versions list with newly received remote version
 */
bool
StandardRsm::handleSolicitVersionReply( const ReplicatorPeer &peer,
										const ClassAd &ad )
{
    dprintf( D_ALWAYS, "StandardRsm::handleSolicitVersionReply started\n" );
    if( !isState(ReplicatorVersion::STATE_REQUESTING) ) {
		return true;
	}

    ReplicatorRemoteVersion version( peer );
	if ( !version.initialize(ad) ) {
		dprintf( D_ALWAYS, "Failed to initialize version from ad\n" );
		return false;
	}
	return m_peers.updatePeerVersion( version );
}

/* Function   : onNewlyJoinedVersion
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_NEWLY_JOINED_VERSION command;
 *				void by now
 */
bool
StandardRsm::handleNewlyJoinedVersion( const ReplicatorPeer & /*peer*/,
									   const ClassAd & /*ad*/ )
{
    dprintf(D_ALWAYS, "StandardRsm::onNewlyJoinedVersion started\n");

    if( isState(ReplicatorVersion::STATE_LEADER) ) {
        // eventually merging files
        //decodeAndAddVersion( stream );
    }
	return true;
}

/* Function   : onGivingUpVersion
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_GIVING_UP_VERSION command; void by now
 * 				initiating merging two reconciled replication leaders' state
 *				files and new gid selection (for replication daemon in leader
 *				state)
 */
bool
StandardRsm::handleGivingUpVersion( const ReplicatorPeer & /*peer*/,
									const ClassAd & /*ad*/ )
{
    dprintf( D_ALWAYS, "StandardRsm::onGivingUpVersion started\n" );

    if( isState(ReplicatorVersion::STATE_BACKUP) ) {
        // eventually merging files
    }
    if( isState(ReplicatorVersion::STATE_LEADER) ){
        // eventually merging files
        handleGidSelection( );
    }
	return true;
}

bool
StandardRsm::downloadReaper( int pid, int exitStatus )
{
    bool rval = BaseRsm::downloadReaper( pid, exitStatus );
    if(  ( rval == true ) &&
		 isState(ReplicatorVersion::STATE_DOWNLOADING)  ) {
        handleDownloadTimer( );
    }
    return rval;
}

/* Function   : commandHandlerHad
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: handles various commands sent to this replication daemon
 *				from the HAD
 */
int
StandardRsm::commandHandlerHad( int command, Stream* stream )
{
	ClassAd			ad;
	ReplicatorPeer	peer;

	if ( !commonCommandHandler( command, stream, "HAD", ad, peer )  ) {
		return FALSE;
	}

	bool	status = true;
    switch( command ) {
	case HAD_BEFORE_PASSIVE_STATE:
		status = handleBeforePassiveState( peer, ad );
		break;
	case HAD_AFTER_ELECTION_STATE:
		status = handleAfterElectionState( peer, ad );
		break;
	case HAD_AFTER_LEADER_STATE:
		status = handleAfterLeaderState( peer, ad );
		break;
	case HAD_IN_LEADER_STATE:
		status = handleInLeaderState( peer, ad );
		break;
	default:
		dprintf( D_ALWAYS,
				 "commandHandlerHad(): Unhandled command %d!!!\n",
				 command );
		return FALSE;
    }
	return status ? TRUE : FALSE;
}

/* Function   : commandHandlerPeer
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: handles various commands sent to this replication daemon
 *				from a peer replicator
 */
int
StandardRsm::commandHandlerPeer( int command, Stream* stream )
{
	ClassAd			ad;
	ReplicatorPeer	host;
	if ( !commonCommandHandler( command, stream, "peer", ad, host )  ) {
		return FALSE;
	}

	// Valid peer?
	ReplicatorPeer	*peer;
	peer = m_peers.findPeer( host );
	if ( !peer ) {
		dprintf( D_ALWAYS,
				 "Peer message %d from unknown host %s\n",
				 command, host.getSinfulStr( ) );
		return FALSE;
	}

	ReplicatorSimpleFileSet	fileset;
	if ( !fileset.init(ad) ) {
		dprintf( D_ALWAYS,
				 "Peer message %d from %s missing fileset info\n",
				 command, host.getSinfulStr( ) );
		return FALSE;
	}

	if ( !sameFiles(fileset, *peer) ) {
		return FALSE;
	}

	bool	status = true;
    switch( command ) {
	case REPLICATION_LEADER_VERSION:
		status = handleLeaderVersion( *peer, ad );
		break;
	case REPLICATION_SOLICIT_VERSION:
		status = handleSolicitVersion( *peer, ad );
		break;
	case REPLICATION_SOLICIT_VERSION_REPLY:
		status = handleSolicitVersionReply( *peer, ad );
		break;
	case REPLICATION_NEWLY_JOINED_VERSION:
		status = handleNewlyJoinedVersion( *peer, ad );
		break;
	case REPLICATION_GIVING_UP_VERSION:
		status = handleGivingUpVersion( *peer, ad );
		break;
	default:
		dprintf( D_ALWAYS,
				 "CommandHandlerPeer(): Unhandled command %d!!!\n", command );
		status = FALSE;
    }
	return status ? TRUE : FALSE;
}

/* Function   : commandHandlerTransferer
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: handles various commands sent to this replication daemon
 *				from a peer transferer
 */
int
StandardRsm::commandHandlerTransferer( int command, Stream* stream )
{
	ClassAd			ad;
	ReplicatorPeer	host;
	if ( !commonCommandHandler( command, stream, "Transferer", ad, host )  ) {
		return FALSE;
	}

	// Valid peer?
	ReplicatorPeer	*peer;
	peer = m_peers.findPeer( host );
	if ( !peer ) {
		dprintf( D_ALWAYS,
				 "Transferer message %d from unknown host %s\n",
				 command, host.getSinfulStr( ) );
		return FALSE;
	}

	ReplicatorSimpleFileSet	fileset;
	if ( !fileset.init(ad) ) {
		dprintf( D_ALWAYS,
				 "Transferer message %d from %s missing fileset info\n",
				 command, host.getSinfulStr( ) );
		return FALSE;
	}

	if ( !sameFiles(fileset, *peer) ) {
		return FALSE;
	}

	bool status = true;
    switch( command ) {
	case REPLICATION_TRANSFER_FILE:
		status = handleTransferFile( *peer, ad );
		break;
	default:
		dprintf( D_ALWAYS,
				 "CommandHandlerTransferer(): Unhandled command %d!!!\n",
				 command );
		return FALSE;
    }

	return status ? TRUE : FALSE;
}

/* Function   : registerCommand
 * Arguments  : command - id to register
 *			  : cc - Replicator command class
 * Description: register command with given id in daemon core
 */
bool
StandardRsm::registerCommand( int command, CommandClass cc )
{
	CommandHandlercpp	handler = NULL;
	const char			*handler_name;

	switch( cc ) {
	case CCLASS_HAD:
		handler =
			(CommandHandlercpp) &StandardRsm::commandHandlerHad;
		handler_name = "commandHandlerHad";
		break;
	case CCLASS_PEER:
		handler =
			(CommandHandlercpp) &StandardRsm::commandHandlerPeer;
		handler_name = "commandHandlerPeer";
		break;
	case CCLASS_TRANSFERER:
		handler =
			(CommandHandlercpp) &StandardRsm::commandHandlerTransferer;
		handler_name = "commandHandlerTransferer";
		break;
	}

	ASSERT( handler );
    daemonCore->Register_Command(
        command, const_cast<char*>( utilToString(command) ),
        handler, handler_name, this, DAEMON );
	return true;
}

/* Function   : replicationTimer
 * Description: replication daemon life cycle handler
 * Remarks    : fired upon expiration of timer, designated by
 *              'm_replicationTimerId' each 'REPLICATION_INTERVAL' seconds
 */
void
StandardRsm::handleReplicationTimer( void )
{
    dprintf( D_FULLDEBUG, "StandardRsm::handleReplicationTimer\n" );
    utilCancelTimer(m_replicationTimerId);

    m_replicationTimerId = daemonCore->Register_Timer ( m_replicationInterval,
                    (TimerHandlercpp) &StandardRsm::handleReplicationTimer,
                    "Time to replicate file", this );
    if( isState(ReplicatorVersion::STATE_REQUESTING) ) {
        return;
    }
	int currentTime = time( NULL );
    /* Killing stuck uploading/downloading processes: allowing downloading/
     * uploading for about several replication intervals only
     */
	killStuckDownload( currentTime );
	if( isState(ReplicatorVersion::STATE_DOWNLOADING) ) {
        return;
    }

	killStuckUploads( currentTime );
    dprintf( D_FULLDEBUG,
			 "StandardRsm::replicationTimer: "
			 "# transferers: downloading=%d, uploading=%d\n",
             m_downloader.isActive() ? 1 : 0,
			 m_uploaders.numActive() );
    if( isState(ReplicatorVersion::STATE_BACKUP) ) {
		checkVersionSynchronization( );

		return;
    }
    dprintf( D_ALWAYS,
			 "StandardRsm::replicationTimer: "
			 "synchronizing the local version with actual state file\n" );
	// if after the version synchronization, the file update was tracked, the
	// local version is broadcasted to the entire pool
    if( m_myVersion.synchronize( true ) ) {
        sendMessage( REPLICATION_LEADER_VERSION, true );
    }
    dprintf( D_FULLDEBUG,
			 "StandardRsm::replicationTimer %d seconds "
			 "without HAD_IN_LEADER_STATE\n",
             int( currentTime - m_lastHadAliveTime ) );
	// allowing to remain replication leader without HAD_IN_LEADER_STATE
	// messages for about 'ALIVE_TOLERANCE' seconds only
    if( currentTime - m_lastHadAliveTime > m_aliveTolerance) {
        sendMessage( REPLICATION_GIVING_UP_VERSION, true );
        setState( ReplicatorVersion::STATE_BACKUP );
    }
}
/* Function   : handleVersionReqTimer
 * Description: timer, expiration of which means stopping collecting the pool
 *				versions in STATE_REQUESTING state, passing to
 *				STATE_DOWNLOADING state and starting downloading from the
 *				machine with the best version
 */
void
StandardRsm::handleVersionReqTimer( void )
{
    dprintf( D_FULLDEBUG, "StandardRsm::versionReqTimer\n" );
    utilCancelTimer(m_versionRequestingTimerId);
    setState( ReplicatorVersion::STATE_DOWNLOADING );

    // selecting the best version amongst all the versions that have been sent
    // by other replication daemons
	ReplicatorPeer					peer;	// Place holder
    ReplicatorRemoteVersionMutable	version( peer );

    if( selectReplica(version) &&  version.isPeerValid() ) {
        startDownload( version.getPeer(), m_maxTransfererLifeTime );
        dprintf( D_FULLDEBUG,
				 "StandardRsm::versionRequestingTimer "
				 "registering version downloading timer\n" );
        m_downloadTimerId = daemonCore->Register_Timer(
			m_maxTransfererLifeTime,
            (TimerHandlercpp) &StandardRsm::handleDownloadTimer,
            "Time to pass to BACKUP state", this );
    } else {
        handleDownloadTimer( );
    }
}

/* Function   : handleDownloadTimer
 * Description: timer, expiration of which means stopping downloading the best
 *              pool version in STATE_DOWNLOADING state and passing to
 *              STATE_BACKUP state
 */
void
StandardRsm::handleDownloadTimer( void )
{
    dprintf( D_FULLDEBUG, "StandardRsm::handleDownloadTimer\n" );
    utilCancelTimer(m_downloadTimerId);

	m_peers.resetAllState( );
	checkVersionSynchronization( );
	setState( ReplicatorVersion::STATE_BACKUP );
}

/* Function    : updateCollectors
 * Description : sends the classad update to collectors
 */
void
StandardRsm::updateCollectors( void )
{
	updateAd( m_classAd );
	daemonCore->sendUpdates( UPDATE_AD_GENERIC, &m_classAd, NULL, false );
}

void
StandardRsm::checkVersionSynchronization( void )
{
	int temporaryGid = -1, temporaryLogicalClock = -1;

	m_myVersion.readVersionFile( temporaryGid, temporaryLogicalClock );
	ASSERT( temporaryGid == m_myVersion.getGid( ) &&
			temporaryLogicalClock == m_myVersion.getLogicalClock( ) );
}

void
StandardRsm::printDataMembers( void )
{
	dprintf( D_ALWAYS, "\n"
			 "Replication interval                  - %d\n"
			 "HAD alive tolerance                   - %d\n"
			 "Max transferer life time              - %d\n"
			 "Newly joined waiting version interval - %d\n"
			 "Version requesting timer id           - %d\n"
			 "Version downloading timer id          - %d\n"
			 "Replication timer id                  - %d\n"
			 "Last HAD alive time                   - %ld\n",
			 m_replicationInterval,
			 m_aliveTolerance,
			 m_maxTransfererLifeTime,
			 m_newlyJoinedWaitingVersionInterval,
			 m_versionRequestingTimerId,
			 m_downloadTimerId,
			 m_replicationTimerId,
			 m_lastHadAliveTime );
}
