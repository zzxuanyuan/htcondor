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
#include "daemon.h"
#include "daemon_types.h"
#include "subsystem_info.h"

#include "RsmBase.h"
#include "SafeFileOps.h"

using namespace std;

/*
 * Base Replicator State Machine class
 */
BaseRsm::BaseRsm( void )
		: ReplicationBase( new ReplicatorFileSet(),
						   new SafeFileRotator(true) ),
		  m_uploadReaperId( -1 ),
		  m_uploaders( ),
		  m_downloadReaperId( -1 ),
		  m_downloader( *m_fileset )
{
	dprintf( D_FULLDEBUG, "BaseRsm ctor\n" );
}

BaseRsm::~BaseRsm( void )
{
	dprintf( D_FULLDEBUG, "BaseRsm dtor\n" );
	cleanup( );
}

bool
BaseRsm::preInit( void )
{
	dprintf( D_FULLDEBUG, "BaseRsm::preInit\n" );

	if ( !ReplicationBase::preInit() ) {
		dprintf( D_ALWAYS, "ReplicationBase initialization failed\n" );
		return false;
	}

    // register the download/upload reaper for the transferer process
	m_downloadReaperId = daemonCore->Register_Reaper(
		"downloadReaper",
		(ReaperHandler)&BaseRsm::dcDownloadReaper,
		"downloadReaper", this );

	m_uploadReaperId = daemonCore->Register_Reaper(
		"uploadReaper",
		(ReaperHandler) &BaseRsm::dcUploadReaper,
		"uploadReaper",
		this );
	return true;
}

bool
BaseRsm::postInit( void )
{
	dprintf( D_FULLDEBUG, "BaseRsm::postInit\n" );
	return ReplicationBase::postInit( );
}

// releasing the dynamic memory and assigning initial values to all the data
// members of the base class
void
BaseRsm::cleanup( void )
{
	dprintf( D_FULLDEBUG, "BaseRsm::cleanup started\n" );
    m_uploaders.killAll( SIGKILL );
    m_downloader.kill( SIGKILL );
}

bool
BaseRsm::reconfig( void )
{
	dprintf( D_FULLDEBUG, "BaseRsm::reconfig\n" );

	if ( ! ReplicationBase::reconfig() ) {
		return false;
	}

    char* tmp = NULL;

    tmp = param( "REPLICATION_LIST" );
	bool	peers_modified = false;
    if ( tmp ) {
        if ( !m_peers.init( *tmp, peers_modified ) ) {
			dprintf( D_ALWAYS,
					 "RsmBase::reconfig():"
					 " Failed to initialize from list '%s'\n",
					 tmp );
			return false;
		}
        free( tmp );
    } else {
		dprintf( D_ALWAYS, "REPLICATION_LIST not defined\n" );
		return false;
    }

	tmp = param( "HAD_DOWNLOADER" );
	if ( NULL != tmp ) {
		m_downloaderPath = tmp;
		free( tmp );
	}
	else {
		tmp = param( "SBIN" );
		if( !tmp ) {
			dprintf( D_ALWAYS, "SBIN not defined\n" );
			return false;
		}
		else {
			m_downloaderPath.sprintf( "%s/condor_had_downloader", tmp );
			free( tmp );
		}
	}

	tmp = param( "HAD_UPLOADER" );
	if ( NULL != tmp ) {
		m_uploaderPath = tmp;
		free( tmp );
	}
	else {
		tmp = param( "SBIN" );
		if( !tmp ) {
			dprintf( D_ALWAYS, "SBIN not defined\n" );
			return false;
		}
		else {
			m_uploaderPath.sprintf( "%s/condor_had_uploader", tmp );
			free( tmp );
		}
	}

	printDataMembers( );

	return true;
}

bool
BaseRsm::updateAd( ClassAd &ad )
{
    // publish list of replication nodes
	if ( !m_peers.updateAd( ad ) ) {
		return false;
	};

	return ReplicationBase::updateAd( ad, false );
}

// Canceling all the data regarding the downloading process that has
// just finished, i.e. its pid, the last time of creation. Then
// replacing the state file and the version file with the temporary
// files, that have been uploaded.  After all this is finished,
// loading the received version file's data into 'm_myVersion'
bool
BaseRsm::dcDownloadReaper( Service* service, int pid, int exitStatus)
{
    dprintf( D_FULLDEBUG,
			 "BaseRsm::downloadReaper called for process no. %d\n", pid );
    BaseRsm* self = static_cast<BaseRsm*>( service );
	return self->downloadReaper( pid, exitStatus );
}

bool
BaseRsm::downloadReaper( int pid, int exitStatus )
{

    // setting the downloading reaper process id to initialization value to
    // know whether the downloading has finished or not
	// NOTE: upon stalling the downloader, the transferer is being killed
	//		 before the reaper is called, so that the application fails in
	//		 the assert, this is the reason for commenting it out
	//REPLICATION_ASSERT(replicatorStateMachine->
	//					m_downloadTransfererMetadata.isValid());
	m_downloader.clear( );

    // the function ended due to the operating system signal, the numeric
    // value of which is stored in exitStatus
    if( WIFSIGNALED( exitStatus ) ) {
        dprintf( D_PROC,
				 "BaseRsm::downloadReaper "
				 "process %d failed by signal number %d\n",
				 pid, WTERMSIG( exitStatus ) );
		m_fileset->cleanupTempDownloadFiles( );
        return false;
    }
    // exit function real return value can be retrieved by dividing the
    // exit status by 256
    else if( WEXITSTATUS( exitStatus ) != 0 ) {
        dprintf( D_PROC,
				 "BaseRsm::downloadReaper "
				 "process %d ended unsuccessfully with exit status %d\n",
				 pid, WEXITSTATUS( exitStatus ) );
		m_fileset->cleanupTempDownloadFiles( );
        return false;
    }

	if ( !m_fileset->installNewFiles() ) {
		dprintf( D_ALWAYS,
				 "downloadReaper(): failed to install new files\n" );
		m_fileset->cleanupTempDownloadFiles( );
		return false;
	}
	if ( !m_versionRotator->Rotate() ) {
		dprintf( D_ALWAYS,
				 "downloadReaper(): failed to install new version file\n" );
		m_fileset->cleanupTempDownloadFiles( );
		return false;
	}
	return m_myVersion.synchronize( false );
}

// Scanning the list of uploading transferers and deleting all the data
// regarding the process that has just finished and called the reaper, i.e.
// its pid and time of creation
bool
BaseRsm::dcUploadReaper(
    Service* service, int pid, int exitStatus)
{
    dprintf( D_ALWAYS,
        "BaseRsm::uploadReaper called for process no. %d\n", pid );
    BaseRsm *self =
        static_cast<BaseRsm*>( service );
	return self->uploadReaper( pid, exitStatus );
}

bool
BaseRsm::uploadReaper( int pid, int exitStatus )
{
	ReplicatorTransferer	*uploader = m_uploaders.Find( pid );
	uploader->clear( );
	delete uploader;


    // the function ended due to the operating system signal, the numeric
    // value of which is stored in exitStatus
    if( WIFSIGNALED( exitStatus ) ) {
        dprintf( D_PROC,
				 "BaseRsm::uploadReaper "
				 "process %d failed by signal number %d\n",
				 pid, WTERMSIG( exitStatus ) );
        return false;
    }
    // exit function real return value can be retrieved by dividing the
    // exit status by 256
    else if( WEXITSTATUS( exitStatus ) != 0 ) {
        dprintf( D_PROC,
				 "BaseRsm::uploadReaper "
				 "process %d ended unsuccessfully "
				 "with exit status %d\n",
				 pid, WEXITSTATUS( exitStatus ) );
        return false;
    }
    return true;
}

// creating downloading transferer process and remembering its pid and creation
// time
bool
BaseRsm::startDownload( const ReplicatorPeer &peer, int time_limit )
{
	return startTransfer( peer, m_downloadReaperId, true,
						  m_downloader, time_limit );
}

// creating uploading transferer process and remembering its pid and
// creation time
bool
BaseRsm::startUpload( const ReplicatorPeer &peer,
					  int time_limit )
{
	ReplicatorUploader *uploader = new ReplicatorUploader( );
	return startTransfer( peer, m_uploadReaperId, false,
						  *uploader, time_limit  );
}

bool
BaseRsm::startTransfer( const ReplicatorPeer &peer,
						int reaper_id,
						bool download,
						ReplicatorTransferer &transferer,
						int time_limit )
{
	const char	*direction = NULL;
	const char	*path = NULL;
	ArgList 	 processArguments;

	if ( download ) {
		direction = "downloader";
		path = m_downloaderPath.Value();
		processArguments.AppendArg( path );
		processArguments.AppendArg( "-replicator" );
		processArguments.AppendArg( peer.getSinfulStr() );
	}
	else {
		direction = "uploader";
		path = m_uploaderPath.Value();
		processArguments.AppendArg( path );
		processArguments.AppendArg( "-downloader" );
		processArguments.AppendArg( peer.getSinfulStr() );
	}

	if ( m_localName ) {
		processArguments.AppendArg( "-local-name" );
		processArguments.AppendArg( m_localName );
	}

	// Get arguments from this ArgList object for descriptional purposes.
	MyString	s;
	processArguments.GetArgsStringForDisplay( &s );
    dprintf( D_FULLDEBUG,
			 "BaseRsm::startTransfer creating "
			 "transferer %s process:\n '%s'\n",
			 direction, s.Value( ) );

	// PRIV_ROOT privilege is necessary here to create the process
	// so we can read GSI certs <sigh>
	priv_state privilege = PRIV_ROOT;

	int pid = daemonCore->Create_Process(
		path,						// name
        processArguments,			// args
        privilege,					// priv
        reaper_id,					// reaper id
        FALSE,						// command port needed?
        NULL,						// env
        NULL,						// cwd
        NULL						// process family info
        );
    if( 0 == pid ) {
        dprintf( D_ALWAYS,
				 "BaseRsm::startTransfer unable to create "
				 "transferer %s process\n", direction );
        return false;
    } else {
        dprintf( D_FULLDEBUG,
				 "BaseRsm::startTransfer transferer %s "
				 "process created with pid = %d\n",
				 direction, pid );

       /* Remembering the last time, when the downloading 'condor_transferer'
        * was created: the monitoring might be useful in possible prevention
        * of stuck 'condor_transferer' processes. Remembering the pid of the
        * downloading process as well: to terminate it when the downloading
        * process is stuck
        */
		transferer.activate( pid, time_limit );
		if ( download ) {
			m_fileset->setDownloaderPid( pid );
		}
    }

    return true;
}

// sending message, along with the local replication daemon's version and state
bool
BaseRsm::sendMessage( int command, bool sendClassAd,
					  int debugflags, const char *cmdstr )
{
	bool	status;
	int		success_count = 0;
	if ( sendClassAd ) {
		status = m_peers.sendMessage( command, NULL, success_count );
	}
	else {
		status = m_peers.sendMessage( command, &m_classAd, success_count );
	}
	if ( !status && debugflags && cmdstr ) {
		dprintf( debugflags, "Failed to broadcast %s message\n", cmdstr ); 
	}
	else if ( debugflags && cmdstr ) {
		dprintf( debugflags,
				 "Send %s message to %d peers\n", cmdstr, success_count ); 
	}
	return status;
}

// inserting/replacing version from specific remote replication daemon
// into/in 'm_versionsList'
bool
BaseRsm::updateVersions( ReplicatorRemoteVersion &version )
{
	return m_peers.updatePeerVersion( version );
}

void
BaseRsm::cancelVersionsListLeader( void )
{
	m_peers.setAllState( ReplicatorVersion::STATE_BACKUP );
}

// cancels all the data, considering transferers, both uploading and
// downloading such as pids and last times of creation, and sends
// SIGKILL signals to the stuck processes
void
BaseRsm::killTransferers()
{
	m_downloader.kill( SIGKILL );
	m_downloader.cleanupTempFiles( );

	m_uploaders.killAll( SIGKILL );
	m_uploaders.cleanupTempFiles( );
}

/* Function   : killStuckDownload
 * Description: kills downloading transferer process, if its working time
 *				exceeds MAX_TRANSFER_LIFETIME seconds, and clears all the data
 *				regarding it, i.e. pid and last time of creation 
 */
void 
BaseRsm::killStuckDownload( time_t now )
{
	m_downloader.killIfStuck( SIGKILL, now );
}

/* Function   : killStuckDownload
 * Description: kills downloading transferer process, if its working time
 *				exceeds MAX_TRANSFER_LIFETIME seconds, and clears all the data
 *				regarding it, i.e. pid and last time of creation 
 */
void 
BaseRsm::killStuckUploads( time_t now )
{
	m_uploaders.killAllStuck( SIGKILL, now );
}

void
BaseRsm::printDataMembers( void ) const
{
	dprintf( D_ALWAYS,
			 "\n"
			 "Base path              - %s\n"
			 "Files                  - %s\n"
			 "Log files              - %s\n"
			 "Version file path      - %s\n"
			 "State                  - %d\n"
			 "Downloader executeable - %s\n"
			 "Uploader executeable   - %s\n"
			 "Connection timeout     - %d\n"
			 "Downloading reaper id  - %d\n"
			 "Uploading reaper id    - %d\n",
			 m_fileset->getBaseDir(),
			 m_fileset->getFileListStr(),
			 m_fileset->getLogfileListStr(),
			 m_versionRotator->getFilePath(),
			 m_myVersion.getState(),
			 m_downloaderPath.Value(),
			 m_uploaderPath.Value(),
			 m_connectionTimeout,
			 m_downloadReaperId,
			 m_uploadReaperId );
}
