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

// for 'daemonCore'
#include "condor_daemon_core.h"
// for 'param' function
#include "condor_config.h"

#include "ReplicatorStateMachine.h"
#include "ReplicatorFileSet.h"
#include "FilesOperations.h"

// multiplicative factor, determining how long the active HAD, that does not
// send the messages to the replication daemon, is considered alive
#define HAD_ALIVE_TOLERANCE_FACTOR      (2)
// multiplicative factor, determining how long the newly joining machine is
// allowed to download the version and state files of other pool machines
#define NEWLY_JOINED_TOLERANCE_FACTOR   (2)

// gcc compilation pecularities demand explicit declaration of template classes
// and functions instantiation
#if 0
template void utilCopyList<ReplicatorVersion>( List<ReplicatorVersion>&,
											   List<ReplicatorVersion>& );
#endif

/* forward declaration of the function to resolve the recursion between it
 * and 'getConfigurationDefaultPositiveIntegerParameter'
 */
static int
getConfigurationPositiveIntegerParameter( const char* parameter );

#if 0
/* Function    : getConfigurationDefaultPositiveIntegerParameter
 * Arguments   : parameter - the parameter name
 * Return value: int - default value for the specified parameter
 * Description : returns default value of the specified configuration parameter
 * Note        : the function may halt the program execution, in case when the
 *				 calculation of the default value of a parameter depends on a
 *               value of another parameter, like with
 *               'NEWLY_JOINED_WAITING_VERSION_INTERVAL'
 */
static int
getConfigurationDefaultPositiveIntegerParameter( const char* parameter )
{
	if(      ! strcmp( parameter, "REPLICATION_INTERVAL" ) ) {
		return 5 * MINUTE;
	}
	else if( ! strcmp( parameter, "HAD_CONNECTION_TIMEOUT" ) ) {
		return DEFAULT_SEND_COMMAND_TIMEOUT;
	}
	else if( ! strcmp( parameter, "MAX_TRANSFERER_LIFETIME" ) ) {
    	return 5 * MINUTE;
	}
	else if( ! strcmp( parameter, "NEWLY_JOINED_WAITING_VERSION_INTERVAL" ) ) {
    	int hadConnectionTimeout =
			getConfigurationPositiveIntegerParameter("HAD_CONNECTION_TIMEOUT");
		return NEWLY_JOINED_TOLERANCE_FACTOR * (hadConnectionTimeout + 1);
	}
	return -1;
}

/* Function    : getConfigurationPositiveIntegerParameter
 * Arguments   : parameter  - the parameter name
 * Return value: int - value of the specified parameter, either from the
 *					   configuration file or, when not specified explicitly in
 *					   the configuration file, the default one
 * Description : returns a value of the specified configuration parameter from
 *				 the configuration file; if the value is not specified, takes
 *				 the default value
 * Note        : the function may halt the program execution, in case when the
 *               value of a parameter is not properly specified in the
 *		 		 configuration file - this is the difference between the
 *		 		 function and 'param_integer' in
 *				 condor_c++_util/condor_config.C
 */
static int
getConfigurationPositiveIntegerParameter( const char* parameter )
{
	char* buffer         = param( parameter );
	int   parameterValue = -1;

    if( buffer ) {
        bool result = false;

        parameterValue = utilAtoi( buffer, &result );
        free( buffer );

        if( ! result || parameterValue <= 0 ) {
        	utilCrucialError( utilConfigurationError(parameter,
                                             "REPLICATION").Value( ) );
		}
    } else {
        dprintf( D_ALWAYS, "getConfigurationPositiveIntegerParameter "
                 "finding default value for %s\n", parameter );
        parameterValue = getConfigurationDefaultPositiveIntegerParameter(
			parameter );
    }
    dprintf( D_FULLDEBUG,
			 "getConfigurationPositiveIntegerParameter %s=%d\n",
			 parameter, parameterValue );
	return parameterValue;
}
#endif

ReplicatorStateMachine::ReplicatorStateMachine( void )
{
	dprintf( D_ALWAYS, "ReplicatorStateMachine ctor started\n" );
	m_mySinfulString			  = daemonCore->InfoCommandSinfulString( );
   	m_state                       = STATE_REQUESTING;
   	m_replicationTimerId          = -1;
   	m_replicaRequestTimerId       = -1;
   	m_replicaDownloadTimerId      = -1;
   	m_replicationInterval         = -1;
   	m_hadAliveTolerance           = -1;
   	m_maxTransfererLifeTime       = -1;
   	m_newlyJoinedWaitingVersionInterval = -1;
   	m_lastHadAliveTime            = -1;
   	srand( time(NULL) );
}

// finalizing the delta, belonging to this class only, since the data,
// belonging to the base class is finalized implicitly
ReplicatorStateMachine::~ReplicatorStateMachine( void )
{
	dprintf( D_ALWAYS, "ReplicatorStateMachine dtor started\n" );
    reset( false );
}

/* Function   : reset
 * Description: clears and resets all inner structures and data members
 */
bool
ReplicatorStateMachine::reset( bool all )
{
	dprintf( D_ALWAYS, "RSM::reset() started\n" );

    utilCancelTimer(m_replicationTimerId);
    utilCancelTimer(m_replicaRequestTimerId);
    utilCancelTimer(m_replicaDownloadTimerId);
    m_replicationInterval               = -1;
    m_hadAliveTolerance                 = -1;
    m_maxTransfererLifeTime             = -1;
    m_newlyJoinedWaitingVersionInterval = -1;
    m_lastHadAliveTime                  = -1;

	if ( all ) {
		AbstractReplicatorStateMachine::reset( );
	}
	return true;
}

bool
ReplicatorStateMachine::initialize( void )
{
    dprintf( D_ALWAYS, "RSM::initialize started\n" );

    reinitialize( );

    // register commands that the service responds to
    registerCommand( HAD_BEFORE_PASSIVE_STATE, CCLASS_HAD );
    registerCommand( HAD_AFTER_ELECTION_STATE, CCLASS_HAD );
    registerCommand( HAD_AFTER_LEADER_STATE, CCLASS_HAD );
    registerCommand( HAD_IN_LEADER_STATE, CCLASS_HAD );
    registerCommand( REPLICATION_LEADER_VERSION, CCLASS_PEER );
    registerCommand( REPLICATION_TRANSFER_FILE, CCLASS_PEER );
    registerCommand( REPLICATION_NEWLY_JOINED_VERSION, CCLASS_PEER );
    registerCommand( REPLICATION_GIVING_UP_VERSION, CCLASS_PEER );
    registerCommand( REPLICATION_SOLICIT_VERSION, CCLASS_PEER );
    registerCommand( REPLICATION_SOLICIT_VERSION_REPLY, CCLASS_PEER );

	return true;
}

/* clears all the inner structures and loads the configuration parameters'
 * values again
 */
bool
ReplicatorStateMachine::reinitialize( void )
{
    // delete all configurations and start everything over from the scratch
    reset( false );
	AbstractReplicatorStateMachine::reinitialize( );

    m_replicationInterval =
		param_integer( "REPLICATION_INTERVAL", 5 * MINUTE, 1 );
    m_maxTransfererLifeTime =
        param_integer( "MAX_TRANSFER_LIFETIME", 5 * MINUTE, 1 );
    m_newlyJoinedWaitingVersionInterval =
        param_integer( "NEWLY_JOINED_WAITING_VERSION_INTERVAL", 5 * MINUTE, 1);

    // deduce HAD alive tolerance
    int timeout = param_integer( "HAD_CONNECTION_TIMEOUT",
								 DEFAULT_SEND_COMMAND_TIMEOUT,
								 1);
    char* buffer = param( "HAD_LIST" );
    if ( buffer ) {
        StringList hadList;

        hadList.initializeFromString( buffer );
        free( buffer );
        m_hadAliveTolerance =
			HAD_ALIVE_TOLERANCE_FACTOR * (2 * timeout * hadList.number() + 1);

        dprintf( D_FULLDEBUG,
				 "RSM::reinitialize() %s=%d\n",
				 "HAD_LIST", m_hadAliveTolerance );
    } else {
        utilCrucialError( utilNoParameterError( "HAD_LIST", "HAD" ).Value( ));
    }

    // set a timer to replication routine
    dprintf( D_ALWAYS, "RSM::reinitialize() setting replication timer\n" );
    m_replicationTimerId = daemonCore->Register_Timer( m_replicationInterval,
            (TimerHandlercpp) &ReplicatorStateMachine::replicationTimer,
            "Time to replicate file", this );

    // register the download/upload reaper for the transferer process
    if( m_downloadReaperId == -1 ) {
		m_downloadReaperId = daemonCore->Register_Reaper(
        	"downloadReaper",
			(ReaperHandler)&ReplicatorStateMachine::downloadReaper,
        	"downloadReaper",
			this );
	}
    if( m_uploadReaperId == -1 ) {
		m_uploadReaperId = daemonCore->Register_Reaper(
        	"uploadReplicaTransfererReaper",
        (ReaperHandler) &ReplicatorStateMachine::uploadReaper,
        	"uploadReplicaTransfererReaper",
			this );
    }
	// for debugging purposes only
	printDataMembers( );

	// 
	ClassAd			ad;
	ReplicatorPeer	peer;
	beforePassiveStateHandler( ad, peer );
}

/* Function   : registerCommand
 * Arguments  : command - id to register
 * Description: register command with given id in daemon core
 */
void
ReplicatorStateMachine::registerCommand(int command, CommandClass cc )
{
	CommandHandlercpp	handler = NULL;
	const char			*handler_name;

	switch( cc ) {
	case CCLASS_HAD:
		handler =
			(CommandHandlercpp) &ReplicatorStateMachine::commandHandlerHad;
		handler_name = "commandHandlerHad";
		break;
	case CCLASS_PEER:
		handler =
			(CommandHandlercpp) &ReplicatorStateMachine::commandHandlerPeer;
		handler_name = "commandHandlerPeer";
		break;
	}

	ASSERT( handler );
    daemonCore->Register_Command(
        command, const_cast<char*>( utilToString(command) ),
        handler, handler_name, this, DAEMON );
}

/* Function   : commandHandlerHad
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: handles various commands sent to this replication daemon
 *				from the HAD
 */
int
ReplicatorStateMachine::commandHandlerHad( int command, Stream* stream )
{
	ClassAd			ad;
	ReplicatorPeer	peer;

	if ( !commandHandlerCommon( command, stream, "HAD", ad, peer )  ) {
		return FALSE;
	}
	ASSERT( peer == m_mySinfulString );

    switch( command ) {
	case HAD_BEFORE_PASSIVE_STATE:
		beforePassiveStateHandler( ad, peer );
		break;
	case HAD_AFTER_ELECTION_STATE:
		afterElectionStateHandler( ad, peer );
		break;
	case HAD_AFTER_LEADER_STATE:
		afterLeaderStateHandler( ad, peer );
		break;
	case HAD_IN_LEADER_STATE:
		inLeaderStateHandler( ad, peer );
		break;
	default:
		dprintf( D_ALWAYS,
				 "commandHandlerHad(): Unhandled command %d!!!\n", command );
		return FALSE;
    }
	return TRUE;
}

/* Function   : commandHandlerPeer
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: handles various commands sent to this replication daemon
 *				from a peer replicator
 */
int
ReplicatorStateMachine::commandHandlerPeer( int command, Stream* stream )
{
	ClassAd			ad;
	ReplicatorPeer	peer;
	if ( !commandHandlerCommon( command, stream, "HAD", ad, peer )  ) {
		return FALSE;
	}

	MyString				 error;
	ReplicatorFileReplica	*replica =
		ReplicatorFileReplica::generate( ad, peer, error );
	if ( NULL == replica ) {
		dprintf( D_ALWAYS, "ERROR: %s from %s\n",
				 error.Value(), peer.getSinful() );
		return FALSE;
	}

	if ( ! m_fileSet->equivilent( replica->getFileInfo() ) ) {
		dprintf( D_ALWAYS,
				 "ERROR: File set from peer %s mismatch:"
				 " got:'%s' expected:'%s'",
				 peer.getSinful(),
				 replica->getFileSet.getNames(),
				 m_fileSet->getNames() );
		return FALSE;
	}

	int	status = TRUE;
    switch( command ) {
	case REPLICATION_LEADER_VERSION:
		status = onLeaderVersion( ad, sinful );
		break;
	case REPLICATION_TRANSFER_FILE:
		status = onTransferFile( ad, sinful );
		break;
	case REPLICATION_SOLICIT_VERSION:
		status = onSolicitVersion( ad, sinful );
		break;
	case REPLICATION_SOLICIT_VERSION_REPLY:
		status = onSolicitVersionReply( ad, sinful );
		break;
	case REPLICATION_NEWLY_JOINED_VERSION:
		status = onNewlyJoinedVersion( ad, sinful );
		break;
	case REPLICATION_GIVING_UP_VERSION:
		status = onGivingUpVersion( ad, sinful );
		break;
	default:
		dprintf( D_ALWAYS,
				 "CommandHandlerPeer(): Unhandled command %d!!!\n", command );
		status = FALSE;
    }
	return status;
}

/* Function   : commandHandlerCommon
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: Common command handler decoding
 *				from a peer replicator
 */
int
ReplicatorStateMachine::commandHandlerCommon(
	int command, Stream *stream, const char *name,
	ClassAd &ad, ReplicatorPeer &peer )
{
	// Read the ClassAd off the wire
    stream->decode( );
	if ( !ad.initFromStream(*stream) ) {
        dprintf( D_ALWAYS,
				 "::commandHandler(%s): Failed to read ClassAd [%s]\n",
                 name, utilToString(command) );
	}

	// And, read the EOM
    if( ! stream->end_of_message() ) {
        dprintf( D_ALWAYS, "::commandHandler(%s): read EOM failed from %s\n",
				 name, sinful.Value() );
    }

	// Pull out the sinful from the address
	if ( !ad.LookupString( ATTR_MY_ADDRESS, sinful ) ) {
		dprintf( D_ALWAYS,
				 "commandHandler(%s) ERROR: %s not in ad received from %s\n",
				 name, ATTR_MY_ADDRESS, sinful.Value() );
		return FALSE;
	}
	peer.init( sinful.Value() );

    dprintf( D_FULLDEBUG,
			 "::commandHandler(%s) received command %s from %s\n",
			 name, utilToString(command), sinful.Value() );

	return TRUE;
}


/* sends the version of the last execution time to all the replication
 * daemons, then asks the pool replication daemons to send their own
 * versions to it, sets a timer to wait till the versions are received
 */
bool
ReplicatorStateMachine::beforePassiveStateHandler(
	const ClassAd & /*ad*/, const MyString & /*sinful*/ )
{
    ASSERT(m_state == STATE_REQUESTING);

    dprintf( D_ALWAYS,
			"::beforePassiveStateHandler() started\n" );
    broadcastMessage( REPLICATION_NEWLY_JOINED_VERSION );
    requestVersions( );

    dprintf( D_FULLDEBUG,
			 "::beforePassiveStateHandler() "
			 "registering replica requesting timer\n" );
    m_replicaRequestTimerId = daemonCore->Register_Timer(
		m_newlyJoinedWaitingVersionInterval,
       (TimerHandlercpp) &ReplicatorStateMachine::replicaRequestTimer,
       "Time to pass to STATE_DOWNLOADING state", this );
}

bool
ReplicatorStateMachine::afterElectionStateHandler(
	const ClassAd & /*ad*/, const MyString & /*sinful*/ )
{
    dprintf( D_ALWAYS, "::afterElectionStateHandler() started\n" );
    ASSERT(m_state != STATE_LEADER);

	// we stay in STATE_REQUESTING or STATE_DOWNLOADING state
    // of newly joining node, we will go to LEADER_STATE later
    // upon receiving of IN_LEADER message from HAD
    if( m_state == STATE_REQUESTING || m_state == STATE_DOWNLOADING ) {
        return true;
    }

	return becomeLeader( );
}

bool
ReplicatorStateMachine::afterLeaderStateHandler(
	const ClassAd & /*ad*/, const MyString & /*sinful*/ )
{
   // REPLICATION_ASSERT(state != STATE_BACKUP)

    if( m_state == STATE_REQUESTING || m_state == STATE_DOWNLOADING ) {
        return true;
    }

	/* receiving this notification message in STATE_BACKUP state
	   means that the pool version downloading took more time than it
	   took for the HAD to become active and to give up the
	   leadership, in this case we ignore this notification message
	   from HAD as well, since we do not want to broadcast our newly
	   downloaded version to others, because it is too new */
	if( m_state == STATE_BACKUP ) {
		return true;
	}
    dprintf( D_ALWAYS,
			"ReplicatorStateMachine::afterLeaderStateHandler started\n" );
    broadcastMessage( REPLICATION_GIVING_UP_VERSION );
    m_state = STATE_BACKUP;
	return true;
}

bool
ReplicatorStateMachine::inLeaderStateHandler(
	const ClassAd & /*ad*/, const MyString & /*sinful*/ )
{
    dprintf( D_ALWAYS,
			 "::inLeaderStateHandler() started "
			 "with state = %d\n", int( m_state ) );
    // ASSERT(m_state != STATE_BACKUP)

    if( m_state == STATE_REQUESTING || m_state == STATE_DOWNLOADING) {
        return true;
    }

	/* receiving this notification message in STATE_BACKUP state
	   means that the pool version downloading took more time than it
	   took for the HAD to become active, in this case we act as if we
	   received AFTER_ELECTION message */
    if( m_state == STATE_BACKUP ) {
		return becomeLeader( );
	}
	m_lastHadAliveTime = time( NULL );

    dprintf( D_FULLDEBUG,
            "ReplicatorStateMachine::inLeaderStateHandler last HAD alive time "
            "is set to %s", ctime( &m_lastHadAliveTime ) );

# if 0
    if( downloadTransferersNumber( ) == 0 &&
	 	  replicaSelectionHandler( newVersion ) ) {
        download( newVersion.getSinfulString( ).Value( ) );
    }
# endif

	return true;
}

bool
ReplicatorStateMachine::replicaSelectionHandler(
	ReplicatorFileReplica	&newReplica )
{
    ASSERT( m_state == STATE_DOWNLOADING || m_state == STATE_BACKUP );
    dprintf( D_ALWAYS,
			 "::replicaSelectionHandler() "
			 "started with my version = %s, #versions = %d\n",
             m_myVersion.toString( ).Value( ), m_versionsList.Number( ) );
    List<ReplicatorVersion> actualVersionsList;
    ReplicatorVersion myVersionCopy = m_myVersion;

    utilCopyList( actualVersionsList, m_versionsList );

	// in STATE_BACKUP state compares the received version with the local one
    if( m_state == STATE_BACKUP ) {
		// compares the versions, taking only 'gid' and 'logicalClock' into
		// account - this is the reason for making the states equal
        myVersionCopy.setState( newVersion );

        return ! newVersion.isComparable( myVersionCopy ) ||
				 newVersion > myVersionCopy;
    }

	/* in STATE_DOWNLOADING state selecting the best version from the list of
	 * received versions according to the policy defined by
	 * 'replicaSelectionHandler', i.e. selecting the version with greatest
	 * 'logicalClock' value amongst a group of versions with the same gid
	 */
    actualVersionsList.Rewind( );

    if( actualVersionsList.IsEmpty( ) ) {
        return false;
    }
    ReplicatorVersion version;
    ReplicatorVersion bestVersion;

    // taking the first actual version as the best version in the meantime
    actualVersionsList.Next( bestVersion );
    dprintf( D_ALWAYS, "::replicaSelectionHandler() best version = %s\n",
			 bestVersion.toString( ).Value( ) );

    while( actualVersionsList.Next( version ) ) {
        dprintf( D_ALWAYS,
				 "::replicaSelectionHandler(): actual version = %s\n",
				 version.toString( ).Value( ) );
        if( version.isComparable( bestVersion ) && version > bestVersion ) {
            bestVersion = version;
        }
    }
    actualVersionsList.Rewind( );

	// compares the versions, taking only 'gid' and 'logicalClock' into
    // account - this is the reason for making the states equal
    myVersionCopy.setState( bestVersion );

	// either when the versions are incomparable or when the local version
	// is worse, the remote version must be downloaded
    if( myVersionCopy.isComparable( bestVersion ) &&
		myVersionCopy >= bestVersion ) {
        return false;
    }
    newVersion = bestVersion;
    dprintf( D_ALWAYS,
			 "::replicaSelectionHandler() best version selected: %s\n",
			 newVersion.toString().Value() );
    return true;
}

/* until the state files merging utility is ready, the function is not really
 * interesting, it selects 0 as the next gid each time
 */
void
ReplicatorStateMachine::gidSelectionHandler( void )
{
    REPLICATION_ASSERT( m_state == STATE_BACKUP || m_state == STATE_LEADER );
    dprintf( D_ALWAYS,
			 "::gidSelectionHandler() started\n");

    bool      			    areVersionsComparable = true;
    List<ReplicatorVersion> actualVersionsList;
    ReplicatorVersion       actualVersion;

    utilCopyList( actualVersionsList, m_versionsList );

    while( actualVersionsList.Next( actualVersion ) ) {
        if( ! m_myVersion.isComparable( actualVersion ) ) {
            areVersionsComparable = false;

            break;
        }
    }
    actualVersionsList.Rewind( );

    if( areVersionsComparable ) {
        dprintf( D_FULLDEBUG, "No need to select new gid\n" );
        return;
    }
    int temporaryGid = 0;

    while( ( temporaryGid = rand( ) ) == m_myVersion.getGid( ) ) {
		// Do nothing
	}
    m_myVersion.setGid( temporaryGid );

    dprintf( D_FULLDEBUG, "New gid selected: %d\n", temporaryGid );
}

/* Function   : decodeVersionAndState
 * Arguments  : stream - socket, through which the data is received
 * Description: receives remote replication daemon version and state from the
 *				given socket
 */
ReplicatorVersion*
ReplicatorStateMachine::decodeVersionAndState( Stream* stream )
{
	ReplicatorVersion	*newVersion = new ReplicatorVersion;
	// decode remote replication daemon version
   	if( ! newVersion->decode( stream ) ) {
    	dprintf( D_ALWAYS, "ReplicatorStateMachine::decodeVersionAndState "
                           "cannot read remote daemon version\n" );
       	delete newVersion;

       	return 0;
   	}
   	int remoteReplicatorState;

   	stream->decode( );
	// decode remore replication daemon state
   	if( ! stream->code( remoteReplicatorState ) ) {
    	dprintf( D_ALWAYS, "ReplicatorStateMachine::decodeVersionAndState "
                           "unable to decode the state\n" );
       	delete newVersion;

       	return 0;
   	}
   	newVersion->setState( ReplicatorState( remoteReplicatorState ) );

   	return newVersion;
   	//updateVersionsList( *newVersion );
}

/* Function   : becomeLeader
 * Description: passes to leader state, sets the last time, that HAD sent its
 * 				message and chooses a new gid
 */
bool
ReplicatorStateMachine::becomeLeader( void )
{
	// sets the last time, when HAD sent a HAD_IN_STATE_STATE
	m_lastHadAliveTime = time( NULL );
    dprintf( D_FULLDEBUG,
			 "::becomeLeader(): last HAD alive time is set to %s",
			 ctime( &m_lastHadAliveTime ) );

	// selects new gid for the pool
    gidSelectionHandler( );
    m_state = STATE_LEADER;
	return true;
}

/* Function   : onLeaderVersion
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_LEADER_VERSION command; comparing the
 *				received version to the local one and downloading the replica
 *				from the remote replication daemon when the received version is
 *				better than the local one and there is no downloading
 *				'condor_transferer' running at the same time
 */
bool
ReplicatorStateMachine::onLeaderVersion( const ClassAd &ad,
										 const MyString &sinful )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::onLeaderVersion started\n" );

    if( m_state != STATE_BACKUP ) {
        return;
    }
	checkVersionSynchronization( );

    Version* newVersion = decodeVersionAndState( stream );

	// comparing the received version to the local one
    bool downloadNeeded = replicaSelectionHandler( *newVersion );

    // downloading the replica from the remote replication daemon, when the
	// received version is better and there is no running downloading
	// 'condor_transferers'
    if( downloadTransferersNumber( ) == 0 && newVersion && downloadNeeded ) {
        dprintf( D_FULLDEBUG, "ReplicatorStateMachine::onLeaderVersion "
				"downloading from %s\n",
				newVersion->getSinfulString( ).Value( ) );
        download( newVersion->getSinfulString( ).Value( ) );
    }

    // replication leader must not send a version which hasn't been updated
    //assert(downloadNeeded);
    //REPLICATION_ASSERT(downloadNeeded);
	delete newVersion;
}

/* Function   : onTransferFile
 * Arguments  : daemonSinfulString - the address of remote replication daemon,
 *				which sent the REPLICATION_TRANSFER_FILE command
 * Description: handler of REPLICATION_TRANSFER_FILE command; starting
 * 				uploading the replica from specified replication daemon
 */
bool
ReplicatorStateMachine::onTransferFile( const ClassAd &ad,
										const MyString &sinful )
{
    dprintf( D_ALWAYS, "::onTransferFile() %s started\n",
             daemonSinfulString );
    if( m_state == STATE_LEADER ) {
        upload( daemonSinfulString );
    }
}

/* Function   : onSolicitVersion
 * Arguments  : daemonSinfulString - the address of remote replication daemon,
 *              which sent the REPLICATION_SOLICIT_VERSION command
 * Description: handler of REPLICATION_SOLICIT_VERSION command; sending local
 *				version along with current replication daemon state
 */
bool
ReplicatorStateMachine::onSolicitVersion( const ClassAd &ad,
										  const MyString &sinful )
{
    dprintf( D_ALWAYS, "::onSolicitVersion() %s started\n",
             daemonSinfulString );
    if( m_state == STATE_BACKUP || m_state == STATE_LEADER ) {
        sendVersionAndStateCommand( REPLICATION_SOLICIT_VERSION_REPLY,
                                    daemonSinfulString );
   }
}

/* Function   : onSolicitVersionReply
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_SOLICIT_VERSION_REPLY command; updating
 * 				versions list with newly received remote version
 */
bool
ReplicatorStateMachine::onSolicitVersionReply( const ClassAd & /*ad*/,
											   const MyString & /*sinful*/ )
{
    dprintf( D_ALWAYS, "ReplicatorStateMachine::onSolicitVersionReply "
					   "started\n" );
    ReplicatorVersion* newVersion = 0;

    if( m_state == STATE_REQUESTING &&
	  ( newVersion = decodeVersionAndState( stream ) ) ) {
        updateVersionsList( *newVersion );
    }
}

/* Function   : onNewlyJoinedVersion
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_NEWLY_JOINED_VERSION command; void by
 *				now
 */
void
ReplicatorStateMachine::onNewlyJoinedVersion( Stream* /*stream*/ )
{
    dprintf(D_ALWAYS,
			"ReplicatorStateMachine::onNewlyJoinedVersion started\n");

    if( m_state == STATE_LEADER ) {
        // eventually merging files
        //decodeAndAddVersion( stream );
    }
}

/* Function   : onGivingUpVersion
 * Arguments  : stream - socket, through which the data is received and sent
 * Description: handler of REPLICATION_GIVING_UP_VERSION command; void by now
 * 				initiating merging two reconciled replication leaders' state
 *				files and new gid selection (for replication daemon in leader
 *				state)
 */
void
ReplicatorStateMachine::onGivingUpVersion( Stream* /*stream*/ )
{
    dprintf( D_ALWAYS,
			 "ReplicatorStateMachine::onGivingUpVersion started\n" );

    if( m_state == STATE_BACKUP) {
        // eventually merging files
    }
    if( m_state == STATE_LEADER ){
        // eventually merging files
        gidSelectionHandler( );
    }
}

int
ReplicatorStateMachine::downloadReplicaTransfererReaper(
	Service		*service,
	int			 pid,
	int			 exitStatus )
{
    ReplicatorStateMachine* replicatorStateMachine =
    	static_cast<ReplicatorStateMachine*>( service );
    int returnValue = AbstractReplicatorStateMachine::
						downloadReplicaTransfererReaper(service,
														pid,
														exitStatus);
    if( returnValue == TRANSFERER_TRUE &&
		replicatorStateMachine->m_state == STATE_DOWNLOADING ) {
        replicatorStateMachine->versionDownloadingTimer( );
    }
    return returnValue;
}

/* Function   : killStuckDownloadingTransferer
 * Description: kills downloading transferer process, if its working time
 *				exceeds MAX_TRANSFER_LIFETIME seconds, and clears all the data
 *				regarding it, i.e. pid and last time of creation
 */
void
ReplicatorStateMachine::killStuckDownloadingTransferer( time_t currentTime )
{
    // killing stuck downloading 'condor_transferer'
    if( m_downloadTransfererMetadata.isValid( ) &&
        currentTime - m_downloadTransfererMetadata.m_lastTimeCreated >
            m_maxTransfererLifeTime ) {
       /* Beware of sending signal with downloadTransfererPid = -1, because
        * according to POSIX it will be sent to every process that the
        * current process is able to sent signals to
        */
        dprintf( D_FULLDEBUG,
				"ReplicatorStateMachine::killStuckDownloadingTransferer "
                "killing downloading condor_transferer pid = %d\n",
                 m_downloadTransfererMetadata.m_pid );
		// sending SIGKILL signal, wrapped in daemon core function for
		// portability
    	if( !daemonCore->Send_Signal( m_downloadTransfererMetadata.m_pid,
									 SIGKILL ) ) {
        	dprintf( D_ALWAYS,
                     "ReplicatorStateMachine::killStuckDownloadingTransferer"
                     " kill signal failed, reason = %s\n", strerror(errno));
        }
		// when the process is killed, it could have not yet erased its
		// temporary files, this is why we ensure it by erasing it in killer
		// function
		MyString extension( m_downloadTransfererMetadata.m_pid );

        // the .down ending is needed in order not to confuse between
        // upload and download processes temporary files
        extension += ".";
        extension += DOWNLOADING_TEMPORARY_FILES_EXTENSION;

        FilesOperations::safeUnlinkFile( m_versionFilePath.Value( ),
                                         extension.Value( ) );
        FilesOperations::safeUnlinkFile( m_stateFilePath.Value( ),
                                         extension.Value( ) );
		m_downloadTransfererMetadata.set( );
	}
}

/* Function   : killStuckUploadingTransferers
 * Description: kills uploading transferer processes, the working time of which
 *              exceeds MAX_TRANSFER_LIFETIME seconds, and clears all the data,
 *				regarding them, i.e. pids and last times of creation
 */
void
ReplicatorStateMachine::killStuckUploadingTransferers( time_t currentTime )
{
	m_uploadTransfererMetadataList.Rewind( );

	ProcessMetadata* uploadTransfererMetadata = NULL;

	// killing stuck uploading 'condor_transferers'
    while( m_uploadTransfererMetadataList.Next( uploadTransfererMetadata ) ) {
        if( uploadTransfererMetadata->isValid( ) &&
			currentTime - uploadTransfererMetadata->m_lastTimeCreated >
              m_maxTransfererLifeTime ) {
            dprintf( D_FULLDEBUG,
					"ReplicatorStateMachine::killStuckUploadingTransferers "
                    "killing uploading condor_transferer pid = %d\n",
                    uploadTransfererMetadata->m_pid );
			// sending SIGKILL signal, wrapped in daemon core function for
        	// portability
			if( !daemonCore->Send_Signal(
				uploadTransfererMetadata->m_pid, SIGKILL ) ) {
				dprintf( D_ALWAYS,
						 "ReplicatorStateMachine::"
						 "killStuckUploadingTransferers"
						 " kill signal failed, reason = %s\n",
						 strerror(errno));
			}
			// when the process is killed, it could have not yet erased its
        	// temporary files, this is why we ensure it by erasing it in
        	// killer function
			MyString extension( uploadTransfererMetadata->m_pid );
            // the .up ending is needed in order not to confuse between
            // upload and download processes temporary files
            extension += ".";
            extension += UPLOADING_TEMPORARY_FILES_EXTENSION;

            FilesOperations::safeUnlinkFile( m_versionFilePath.Value( ),
                                             extension.Value( ) );
            FilesOperations::safeUnlinkFile( m_stateFilePath.Value( ),
                                             extension.Value( ) );
			delete uploadTransfererMetadata;
			m_uploadTransfererMetadataList.DeleteCurrent( );
		}
    }
	m_uploadTransfererMetadataList.Rewind( );
}

/* Function   : replicationTimer
 * Description: replication daemon life cycle handler
 * Remarks    : fired upon expiration of timer, designated by
 *              'm_replicationTimerId' each 'REPLICATION_INTERVAL' seconds
 */
void
ReplicatorStateMachine::replicationTimer( void )
{
    dprintf( D_ALWAYS,
			 "ReplicatorStateMachine::replicationTimer cancelling timer\n" );
    utilCancelTimer(m_replicationTimerId);

    dprintf( D_ALWAYS,
			 "ReplicatorStateMachine::replicationTimer "
			 "registering timer once again\n" );
    m_replicationTimerId = daemonCore->Register_Timer (
		m_replicationInterval,
		(TimerHandlercpp) &ReplicatorStateMachine::replicationTimer,
		"Time to replicate file",
		this );
    if( m_state == STATE_REQUESTING ) {
        return;
    }

    /* Killing stuck uploading/downloading processes: allowing
	 * downloading/uploading for about several replication intervals only
     */
	int currentTime = time( NULL );

	// TODO: Atomic operation
	killStuckDownloadingTransferer( currentTime );
	// End of TODO: Atomic operation

	if( m_state == STATE_DOWNLOADING ) {
        return;
    }

	// TODO: Atomic operation
	killStuckUploadingTransferers( currentTime );
	// End of TODO: Atomic operation

    dprintf( D_FULLDEBUG,
			 "ReplicatorStateMachine::replicationTimer "
			 "# downloading condor_transferer = %d, "
			 "# uploading condor_transferer = %d\n",
             downloadTransferersNumber( ),
			 m_uploadTransfererMetadataList.Number( ) );
    if( m_state == STATE_BACKUP ) {
		checkVersionSynchronization( );
		return;
    }

    dprintf( D_ALWAYS,
			 "ReplicatorStateMachine::replicationTimer "
			 "synchronizing the "
			 "local version with actual state file\n" );

	// if after the version synchronization, the file update was tracked, the
	// local version is broadcasted to the entire pool
    if( m_myVersion.synchronize( true ) ) {
        broadcastMessage( REPLICATION_LEADER_VERSION );
    }
    dprintf( D_FULLDEBUG,
			 "ReplicatorStateMachine::replicationTimer %d seconds "
			 "without HAD_IN_LEADER_STATE\n",
             int( currentTime - m_lastHadAliveTime ) );

	// allowing to remain replication leader without HAD_IN_LEADER_STATE
	// messages for about 'HAD_ALIVE_TOLERANCE' seconds only
    if( currentTime - m_lastHadAliveTime > m_hadAliveTolerance) {
        broadcastMessage( REPLICATION_GIVING_UP_VERSION );
        m_state = STATE_BACKUP;
    }
}

/* Function   : replicaRequestTimer
 * Description: timer, expiration of which means stopping collecting the pool
 *				versions in STATE_REQUESTING state, passing to
 *				STATE_DOWNLOADING state and starting downloading from the
 *				machine with the best version
 */
void
ReplicatorStateMachine::replicaRequestTimer( void )
{
    dprintf( D_ALWAYS,
			"::replicaRequestTimer(): started\n" );

    utilCancelTimer(m_replicaRequestTimerId);
    dprintf( D_FULLDEBUG,
			 "::replicaRequestTimer(): cancelling ver. req timer\n" );

    m_state = STATE_DOWNLOADING;

    // Select the best version amongst all the versions that have been
	// sent by other replication daemons
    Version updatedVersion;
    if( replicaSelectionHandler(updatedVersion) ) {
        download( updatedVersion.getSinfulString( ).Value( ) );
        dprintf( D_FULLDEBUG,
				 "::replicaRequestTimer() registering version dnl timer\n");
        m_replicaDownloadTimerId = daemonCore->Register_Timer(
			m_maxTransfererLifeTime,
            (TimerHandlercpp) &ReplicatorStateMachine::versionDownloadingTimer,
            "Time to pass to STATE_BACKUP state", this );
    } else {
        versionDownloadingTimer( );
    }
}

/* Function   : versionDownloadingTimer
 * Description: timer, expiration of which means stopping downloading the best
 *              pool version in STATE_DOWNLOADING state and passing to
 *              STATE_BACKUP state
 */
void
ReplicatorStateMachine::versionDownloadingTimer( void )
{
    dprintf( D_ALWAYS,
			"::versionDownloadingTimer() started\n" );
    utilCancelTimer(m_replicaDownloadTimerId);
    dprintf( D_FULLDEBUG,
			 "::versionDownloadingTimer() "
			 "cancelling version downloading timer\n" );
    utilClearList( m_versionsList );

	checkVersionSynchronization( );

	m_state = STATE_BACKUP;
}

void
ReplicatorStateMachine::printDataMembers( void ) const
{
	dprintf( D_ALWAYS,
			 "\n"
			 "Replication interval                  - %d\n"
			 "HAD alive tolerance                   - %d\n"
			 "Max transferer life time              - %d\n"
			 "Newly joined waiting version interval - %d\n"
			 "Version requesting timer id           - %d\n"
			 "Version downloading timer id          - %d\n"
			 "Replication timer id                  - %d\n"
			 "Last HAD alive time                   - %ld\n",
			 m_replicationInterval,
			 m_hadAliveTolerance,
			 m_maxTransfererLifeTime,
			 m_newlyJoinedWaitingVersionInterval,
			 m_replicaRequestTimerId,
			 m_replicaDownloadTimerId,
			 m_replicationTimerId,
			 m_lastHadAliveTime );
};

void
ReplicatorStateMachine::checkVersionSynchronization( void )
{
	int temporaryGid = -1, temporaryLogicalClock = -1;

	m_myVersion.load( temporaryGid, temporaryLogicalClock);
	REPLICATION_ASSERT(
		temporaryGid == m_myVersion.getGid( ) &&
		temporaryLogicalClock == m_myVersion.getLogicalClock( ));
}
