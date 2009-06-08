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

#include "condor_daemon_core.h"
// for 'param' function
#include "condor_config.h"
// for 'Daemon' class
#include "daemon.h"
// for 'DT_ANY' definition
#include "daemon_types.h"

#include "AbstractReplicatorStateMachine.h"
#include "ReplicatorFile.h"
#include "FilesOperations.h"

// gcc compilation pecularities demand explicit declaration of template classes
// and functions instantiation
#if 0
template void utilClearList<AbstractReplicatorStateMachine::ProcessMetadata>
				( List<AbstractReplicatorStateMachine::ProcessMetadata>& );
template void utilClearList<ReplicatorVersion>( List<ReplicatorVersion>& );
#endif

AbstractReplicatorStateMachine::AbstractReplicatorStateMachine( void )
		: m_state( VERSION_REQUESTING ),
		  m_connectionTimeout( DEFAULT_SEND_COMMAND_TIMEOUT ),
		  m_downloadReaperId( -1 ),
		  m_uploadReaperId( -1 )
{
	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine ctor started\n" );
}

AbstractReplicatorStateMachine::~AbstractReplicatorStateMachine( void )
{
	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine dtor started\n" );
    shutdown();
}

// releasing the dynamic memory and assigning initial values to all the data
// members of the base class
void
AbstractReplicatorStateMachine::shutdown( void )
{
	dprintf( D_ALWAYS, "AbstractReplicatorStateMachine::shutdown started\n" );
   	m_state             = VERSION_REQUESTING;
   	m_connectionTimeout = DEFAULT_SEND_COMMAND_TIMEOUT;

    m_transfererPath = "";

    //utilCancelReaper(m_downloadReaperId);
    //utilCancelReaper(m_uploadReaperId);
	// upon finalizing and/or reinitialiing the existing transferer processes
	// must be killed, otherwise we will temporarily deny creation of new
	// downloading transferers till the older ones are over
    killTransferers( );
}

bool
AbstractReplicatorStateMachine::reinitialize( void )
{
	dprintf( D_FULLDEBUG, "::reinitialize()\n" );

    char	*tmp = NULL;

    tmp = param( "REPLICATION_LIST" );
	bool	 list_updated = false;
    if ( tmp ) {
		if ( !m_peerList.init(tmp, list_updated) ) {
			dprintf( D_ALWAYS,
					 "::reinitialize(): Failed to initilize from list '%s'\n",
					 tmp );
			return false;
		}
        free( tmp );
    } else {
        utilCrucialError( utilNoParameterError("REPLICATION_LIST",
		  								       "REPLICATION").Value( ) );
    }

	// Log what we've found
	if ( list_updated ) {
		dprintf( D_FULLDEBUG,
				 "REPLICATION LIST changed: now %s\n",
				 m_peerList.getRawString() );
	}
	else {
		dprintf( D_FULLDEBUG, "REPLICATION LIST unchanged\n" );
	}

	// Get the connection timeout
    m_connectionTimeout = param_integer( "HAD_CONNECTION_TIMEOUT", 0, 1 );
	m_peerList.setConnectionTimeout( m_connectionTimeout );

	// Path to transferer
	tmp = param( "TRANSFERER" );
	if ( NULL != tmp ) {
		m_transfererPath = tmp;
		free( tmp );
	}
	else {
		tmp = param( "SBIN" );
		if( !tmp ) {
			utilCrucialError(
				utilConfigurationError("SBIN","REPLICATION").Value());
		}
		else {
			m_transfererPath.sprintf( "%s/condor_transferer", tmp );
			free( tmp );
		}
	}

	char *spool = param( "SPOOL" );
    if( NULL == spool ) {
        utilCrucialError( utilNoParameterError("SPOOL", "REPLICATION").
                          Value( ) );
	}

	// Clear out the old file list
	m_fileList.clear( );

	// Handle file set separately
	tmp = param( "REPLICATION_FILE_SET" );
	if ( NULL != tmp ) {
		StringList	*files = new StringList( tmp );
		ReplicatorFileSet	*file_set = new ReplicatorFileSet( spool, files );
		file_set->setPeers( m_peerList );
		m_fileList.registerFile( file_set );
		return true;
	}

	tmp = param( "REPLICATION_FILE_LIST" );
	if ( NULL != tmp ) {
		tmp = param( "STATE_FILE" );
	}
	if ( NULL == tmp ) {
		tmp = param( "NEGOTIATOR_STATE_FILE" );
	}
	if ( NULL == tmp ) {
		MyString	 state_file;
		state_file  = spool;
		state_file += "/";
		state_file += "Accountantnew.log";
		tmp = strdup( state_file.Value() );
	}
	ASSERT( tmp != NULL );

	// Build a list of the files and "versions" of each
	StringList	 file_list( tmp );
	char		*file_path;
	file_list.rewind();
	while(  (file_path = file_list.next()) != NULL ) {
		ReplicatorFile	*file_info = new ReplicatorFile( spool, file_path );
		file_info->setPeers( m_peerList );
		file_info->registerUploaders( m_uploaders );
		file_info->registerDownloaders( m_downloaders );
		m_fileList.registerFile( file_info );
	}

	printDataMembers( );

}

// Canceling all the data regarding the downloading process that has just
// finished, i.e. its pid, the last time of creation. Then replacing the state
// file and the version file with the temporary files, that have been uploaded.
// After all this is finished, loading the received version file's data into
// 'm_myVersion'
int
AbstractReplicatorStateMachine::downloadReaper(
    Service*	service,
	int			pid,
	int			exitStatus )
{
    dprintf( D_FULLDEBUG,
            "downloadReaper() : called for process no. %d\n", pid );
    AbstractReplicatorStateMachine* stateMachine =
         static_cast<AbstractReplicatorStateMachine*>( service );

    // setting the downloading reaper process id to initialization value to
    // know whether the downloading has finished or not
	// NOTE: upon stalling the downloader, the transferer is being killed
	// 		 before the reaper is called, so that the application fails in
	//		 the assert, this is the reason for commenting it out
	ReplicatorTransferer	*proc = stateMachine->findDownloader( pid );
	if ( NULL == proc ) {
		dprintf( D_ALWAYS,
				 "downloadReaper(): can't find downloader for PID %d\n", pid );
        return FALSE;
	}
	proc->clear( );

	ReplicatorDownloader	*downloader =
		dynamic_cast<ReplicatorDownloader *>( proc );
	if ( NULL == downloader ) {
		dprintf( D_ALWAYS,
				 "downloadReaper(): Process data for PID %d isn't download\n",
				 pid );
        return FALSE;
	}

    // the function ended due to the operating system signal, the numeric
    // value of which is stored in exitStatus
    if( WIFSIGNALED( exitStatus ) ) {
        dprintf( D_PROC,
				 "downloadReaper(): process %d failed by signal number %d\n",
				 pid, WTERMSIG( exitStatus ) );
        return FALSE;
    }

    // exit function real return value can be retrieved by dividing the
    // exit status by 256
    else if( WEXITSTATUS( exitStatus ) != 0 ) {
        dprintf( D_PROC,
				 "downloadReaper(): "
				 "process %d ended unsuccessfully "
				 "with exit status %d\n",
				 pid, WEXITSTATUS( exitStatus ) );
        return FALSE;
    }

	ReplicatorFileBase	&file_info = downloader->getFileInfo( );
	if ( !file_info.rotate( pid ) ) {
		dprintf( D_ALWAYS,
				 "downloadReaper(): failed to rotate in new files\n" );
		return FALSE;
	}

    file_info.synchronize( false );

    return TRUE;
}

// Scanning the list of uploading transferers and deleting all the data
// regarding the process that has just finished and called the reaper, i.e.
// its pid and time of creation
int
AbstractReplicatorStateMachine::uploadReaper(
    Service* service, int pid, int exitStatus)
{
    dprintf( D_ALWAYS,
			 "AbstractReplicatorStateMachine::uploadReaper "
			 "called for process no. %d\n", pid );
    AbstractReplicatorStateMachine* stateMachine =
        static_cast<AbstractReplicatorStateMachine*>( service );

	ReplicatorTransferer	*proc = stateMachine->findUploader( pid );
	if ( NULL == proc ) {
		dprintf( D_ALWAYS,
				 "uploadReaper(): can't find PID %d\n", pid );
        return FALSE;
	}
	proc->clear( );

	ReplicatorUploader	*uploader =
		dynamic_cast<ReplicatorUploader *>( proc );
	if ( NULL == uploader ) {
		dprintf( D_ALWAYS,
				 "uploadReaper(): Process data for PID %d isn't upload\n",
				 pid );
        return FALSE;
	}

    // the function ended due to the operating system signal, the numeric
    // value of which is stored in exitStatus
    if( WIFSIGNALED( exitStatus ) ) {
        dprintf( D_PROC,
				 "uploadReaper(): process %d failed by signal number %d\n",
				 pid, WTERMSIG( exitStatus ) );
        return FALSE;
    }
    // exit function real return value can be retrieved by dividing the
    // exit status by 256
    else if( WEXITSTATUS( exitStatus ) != 0 ) {
        dprintf( D_PROC,
				 "uploadReaper(): "
				 "process %d ended unsuccessfully with exit status %d\n",
				 pid, WEXITSTATUS( exitStatus ) );
        return FALSE;
    }
    return TRUE;
}

/* creating downloading transferer process and remembering its pid and 
 * creation time
 */
bool
AbstractReplicatorStateMachine::download( ReplicatorFileReplica &version )
{
	const ReplicatorFile	&file       = version.getFileInfo( );
	const ReplicatorPeer	&peer       = version.getPeerInfo( );
	ReplicatorDownloader	&downloader = file.getDownloader( );

	ArgList  processArguments;
	processArguments.AppendArg( m_transfererPath.Value() );
	processArguments.AppendArg( "-f" );
	processArguments.AppendArg( "down" );
	processArguments.AppendArg( peer.getSinful() );
	processArguments.AppendArg( file.getVersionFilePath() );
	processArguments.AppendArg( "1" );
	processArguments.AppendArg( file.getFilePath() );

	// Get arguments from this ArgList object for descriptional purposes.
	MyString	s;
	processArguments.GetArgsStringForDisplay( &s );
    dprintf( D_FULLDEBUG,
			 "::download(): "
			 "creating condor_transferer process: \n \"%s\"\n",
			 s.Value( ) );

	ASSERT( downloader.isActive() == false );

	// PRIV_ROOT privilege is necessary here to create the process
	// so we can read GSI certs <sigh>
	priv_state privilege = PRIV_ROOT;

	int transfererPid = daemonCore->Create_Process(
        m_transfererPath.Value( ),    // name
        processArguments,             // args
        privilege,                    // priv
        m_downloadReaperId,           // reaper id
        FALSE,                        // command port needed?
        NULL,                         // env
        NULL,                         // cwd
        NULL                          // process family info
        );
    if( transfererPid == FALSE ) {
        dprintf( D_ALWAYS,
				 "::download(): "
				 "unable to create condor_transferer process\n" );
        return false;
    }

	dprintf( D_FULLDEBUG,
			 "::download(): "
			 "condor_transferer process created with pid = %d\n",
			 transfererPid );

	/* Remembering the last time, when the downloading 'condor_transferer'
	 * was created: the monitoring might be useful in possible prevention
	 * of stuck 'condor_transferer' processes. Remembering the pid of the
	 * downloading process as well: to terminate it when the downloading
	 * process is stuck
	 */
	downloader.activate( transfererPid );

    return true;
}

// creating uploading transferer process and register it
bool
AbstractReplicatorStateMachine::upload( ReplicatorFileReplica &replica )
{
	const ReplicatorFile	&file     = replica.getFileInfo( );
	const ReplicatorPeer	&peer     = replica.getPeerInfo( );
	ReplicatorUploader		&uploader = replica.getUploader( );

	ArgList  processArguments;
	processArguments.AppendArg( m_transfererPath.Value() );
	processArguments.AppendArg( "-f" );
	processArguments.AppendArg( "up" );
	processArguments.AppendArg( peer.getSinful() );
	processArguments.AppendArg( file.getVersionFilePath() );
	processArguments.AppendArg( "1" );
	processArguments.AppendArg( file.getFilePath() );

	// Get arguments from this ArgList object for descriptional purposes.
	MyString	s;
	processArguments.GetArgsStringForDisplay( &s );
    dprintf( D_FULLDEBUG,
			 "::upload(): "
			 "creating condor_transferer process: \n \"%s\"\n",
			 s.Value( ) );

	ASSERT( uploader.isActive() == false );

	// PRIV_ROOT privilege is necessary here to create the process
	// so we can read GSI certs <sigh>
	priv_state privilege = PRIV_ROOT;

    int transfererPid = daemonCore->Create_Process(
        m_transfererPath.Value( ),    // name
        processArguments,             // args
        privilege,                    // priv
        m_uploadReaperId,             // reaper id
        FALSE,                        // command port needed?
        NULL,                         // envs
        NULL,                         // cwd
        NULL                          // process family info
        );
    if ( transfererPid == FALSE ) {
		dprintf( D_ALWAYS,
				 "::upload(): "
				 "unable to create condor_transferer process\n");
        return false;
    }

	dprintf( D_FULLDEBUG,
			 "::upload() "
			 "condor_transferer process created with pid = %d\n",
			 transfererPid );

	/* Remembering the last time, when the uploading 'condor_transferer'
	 * was created: the monitoring might be useful in possible prevention
	 * of stuck 'condor_transferer' processes. Remembering the pid of the
	 * uploading process as well: to terminate it when the uploading
	 * process is stuck
	 */
	uploader.activate( transfererPid );

    return true;
}

// send command, along with the local replication daemon's version and state
bool
AbstractReplicatorStateMachine::broadcastVersion( int command )
{
	int		errors = 0;
	m_fileList.sendCommand( command, true, errors );
	if ( errors ) {
		dprintf( D_FULLDEBUG,
				 "Failed to send command %d to %d peers\n", command, errors );
		return false;
	}
	return true;
}

bool
AbstractReplicatorStateMachine::requestVersions( void )
{
	int		errors = 0;
	m_fileList.sendCommand( REPLICATION_SOLICIT_VERSION, true, errors );
	if ( errors ) {
		dprintf( D_FULLDEBUG,
				 "Failed to send command %d to %d peers\n",
				 REPLICATION_SOLICIT_VERSION, errors );
		return false;
	}
	return true;
}

// inserting/replacing version from specific remote replication daemon
// into/in 'm_versionsList'
void
AbstractReplicatorStateMachine::updateVersionsList(
	ReplicatorVersion	&newVersion )
{
    ReplicatorVersion* oldVersion;
// TODO: Atomic operation
    m_versionsList.Rewind( );

    while( m_versionsList.Next( oldVersion ) ) {
        // deleting all occurences of replica belonging to the same host name
        if( oldVersion->getHostName( ) == newVersion.getHostName( ) ) {
            delete oldVersion;
            m_versionsList.DeleteCurrent( );
        }
    }
    dprintf( D_FULLDEBUG,
        "AbstractReplicatorStateMachine::updateVersionsList appending %s\n",
         newVersion.toString( ).Value( ) );
    m_versionsList.Append( newVersion );
    m_versionsList.Rewind( );
// End of TODO: Atomic operation
}

void
AbstractReplicatorStateMachine::cancelVersionsListLeader( void )
{
    ReplicatorVersion* version;

    m_versionsList.Rewind( );

    while( m_versionsList.Next( version ) ) {
        version->setState( BACKUP );
    }

    m_versionsList.Rewind( );
}

// sending command to remote replication daemon; specified command function
// allows to specify which data is to be sent to the remote daemon
void
AbstractReplicatorStateMachine::sendCommand(
    int command, char* daemonSinfulString, CommandFunction function )
{
    dprintf( D_FULLDEBUG,
			 "::sendCommand(): %s to %s\n",
			 utilToString(command), daemonSinfulString );
    Daemon  daemon( DT_ANY, daemonSinfulString );
    ReliSock socket;

    // no retries after 'm_connectionTimeout' seconds of unsuccessful
    // connection
    socket.timeout( m_connectionTimeout );
    socket.doNotEnforceMinimalCONNECT_TIMEOUT( );

    if( ! socket.connect( daemonSinfulString, 0, false ) ) {
        dprintf( D_ALWAYS,
				 "::sendCommand(): unable to connect to %s\n",
				 daemonSinfulString );
		socket.close( );

        return;
    }

	// General actions for any command sending
    if( ! daemon.startCommand( command, &socket, m_connectionTimeout ) ) {
        dprintf( D_ALWAYS,
				 "::sendCommand() cannot start command %s to %s\n",
				 utilToString(command), daemonSinfulString );
		socket.close( );

        return;
    }

    char const* sinfulString = daemonCore->InfoCommandSinfulString();
    if(! socket.put( sinfulString )/* || ! socket.eom( )*/) {
        dprintf( D_ALWAYS,
				 "::sendCommand():"
				 " unable to send the local sinful string or eom%s\n",
				 sinfulString );
		socket.close( );

        return;
    }
    else {
        dprintf( D_FULLDEBUG,
				 "::sendCommand(): local sinful string coded successfully\n" );
    }

	// Command-specific actions
	if( ! ((*this).*(function))( socket ) ) {
    	socket.close( );

		return;
	}

	if( ! socket.eom( ) ) {
		socket.close( );
       	dprintf( D_ALWAYS,
				 "::sendCommand(): unable to code the end of message\n" );
       	return;
   	}

	socket.close( );
   	dprintf( D_ALWAYS,
			 "::sendCommand(): %s command sent to %s successfully\n",
             utilToString(command), daemonSinfulString );
}

// specific command function - sends local daemon's version over the socket
bool
AbstractReplicatorStateMachine::versionCommand( ReliSock& socket )
{
    dprintf( D_ALWAYS,
			 "AbstractReplicatorStateMachine::versionCommand started\n" );

    if( ! m_myVersion.code( socket ) ) {
        dprintf( D_NETWORK,
				 "AbstractReplicatorStateMachine::versionCommand "
				 "unable to code the replica\n");
        return false;
    }
    dprintf( D_ALWAYS,
			 "AbstractReplicatorStateMachine::versionCommand "
			 "sent command successfully\n" );
    return true;
}

// specific command function - sends local daemon's version and state over
// the socket
bool
AbstractReplicatorStateMachine::versionAndStateCommand(ReliSock& socket)
{
    if( ! versionCommand( socket ) ) {
            return false;
    }
	int stateAsInteger = int( m_state );

    if( ! socket.code( stateAsInteger ) /*|| ! socket.eom( )*/ ) {
        dprintf( D_NETWORK,
				 "AbstractReplicatorStateMachine::versionAndStateCommand "
				 "unable to code the state or eom%d\n", m_state );
        return false;
    }
    dprintf( D_ALWAYS,
			 "AbstractReplicatorStateMachine::versionAndStateCommand "
			 "sent command successfully\n" );
    return true;
}

// cancels all the data, considering transferers, both uploading and
// downloading such as pids and last times of creation, and sends
// SIGKILL signals to the stuck processes

void
AbstractReplicatorStateMachine::killTransferers()
{
    if( m_downloadTransfererMetadata.isValid() ) {

       /* Beware of sending SIGKILL with download transferer's pid =
		* -1, because * according to POSIX it will be sent to every
		* that the * current process is able to sent signals to
       */

        dprintf( D_FULLDEBUG,
            "AbstractReplicatorStateMachine::killTransferers "
            "killing downloading condor_transferer pid = %d\n",
                   m_downloadTransfererMetadata.m_pid );
        //kill( m_downloadTransfererMetadata.m_pid, SIGKILL );
        daemonCore->Send_Signal( m_downloadTransfererMetadata.m_pid, SIGKILL );
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
		m_downloadTransfererMetadata.set();
    }

	m_uploadTransfererMetadataList.Rewind( );

	ProcessMetadata* uploadTransfererMetadata = NULL;

    while( m_uploadTransfererMetadataList.Next( uploadTransfererMetadata ) ) {
        if( uploadTransfererMetadata->isValid( ) ) {
            dprintf( D_FULLDEBUG,
                "AbstractReplicatorStateMachine::killTransferers "
                "killing uploading condor_transferer pid = %d\n",
                uploadTransfererMetadata->m_pid );
            //kill( uploadTransfererMetadata->m_pid, SIGKILL );
			daemonCore->Send_Signal( uploadTransfererMetadata->m_pid,
									 SIGKILL );

			// when the process is killed, it could have not yet
			// erased its temporary files, this is why we ensure it
			// by erasing it in killer function

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
			// after deletion the iterator is moved to the previous member
			// so advancing the iterator twice and missing one entry does not
			// happen
        	m_uploadTransfererMetadataList.DeleteCurrent( );
		}
    }
	m_uploadTransfererMetadataList.Rewind( );
}

void
AbstractReplicatorStateMachine::printDataMembers( void ) const
{
	dprintf( D_ALWAYS,
			 "\n"
			 "State file path        - %s\n"
			 "Version file path      - %s\n"
			 "State                  - %d\n"
			 "Transferer executeable - %s\n"
			 "Connection timeout     - %d\n"
			 "Downloading reaper id  - %d\n"
			 "Uploading reaper id    - %d\n",
			 m_stateFilePath.Value(),
			 m_versionFilePath.Value(),
			 m_state,
			 m_transfererPath.Value(),
			 m_connectionTimeout,
			 m_downloadReaperId,
			 m_uploadReaperId );
};
