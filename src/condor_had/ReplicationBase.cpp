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
#include "Utils.h"
#include "ReplicationBase.h"
#include "SafeFileOps.h"

using namespace std;

static ReplicationBase	*self = NULL;
const ReplicationBase	*getReplicator( void ) {
	return self;
};

/*
 * Base Replicator State Machine class
 */
ReplicationBase::ReplicationBase( ReplicatorFileSet *fileset,
								  SafeFileRotator *rotator)
		: m_fileset( fileset ),
		  m_versionFilePath( ),
		  m_localName( get_mySubSystem()->getLocalName() ),
		  m_myVersion( *m_fileset, m_versionFilePath ),
		  m_connectionTimeout( DEFAULT_SEND_COMMAND_TIMEOUT ),
		  m_maxTransferLifetime( DEFAULT_MAX_TRANSFER_LIFETIME ),
		  m_versionRotator( rotator )
{
	dprintf( D_FULLDEBUG, "ReplicationBase ctor\n" );
	self = this;
}

ReplicationBase::~ReplicationBase( void )
{
	dprintf( D_FULLDEBUG, "ReplicationBase dtor\n" );
	self = NULL;
	delete m_fileset;
	delete m_versionRotator;
}


bool
ReplicationBase::initializeAll( void )
{
	if ( !preInit() ) {
		dprintf( D_ALWAYS, "initializeAll: preInit failed\n" );
		return false;
	}
	if ( !reconfig() ) {
		dprintf( D_ALWAYS, "initializeAll: reconfig failed\n" );
		return false;
	}
	if ( !postInit() ) {
		dprintf( D_ALWAYS, "initializeAll: postInit failed\n" );
		return false;
	}

	return true;
}

bool
ReplicationBase::preInit( void )
{
    dprintf( D_FULLDEBUG, "ReplicationBase::preInit\n" );
	return true;
}

bool
ReplicationBase::postInit( void )
{
    dprintf( D_FULLDEBUG, "ReplicationBase::postInit\n" );
	return m_myVersion.init( );
}

bool
ReplicationBase::reconfig( void )
{
	bool	 status = true;
    char	*tmp = NULL;

	dprintf( D_FULLDEBUG, "ReplicationBase::reconfig\n" );

    m_connectionTimeout = param_integer(
		"HAD_CONNECTION_TIMEOUT", DEFAULT_SEND_COMMAND_TIMEOUT, 1 );
	char* spool = param( "SPOOL" );
	if ( NULL == spool ) {
		dprintf( D_ALWAYS, "SPOOL not defined!\n" );
		return false;
	}

    m_maxTransferLifetime =
		param_integer( "MAX_TRANSFER_LIFETIME", DEFAULT_MAX_TRANSFER_LIFETIME );

	// Handle file set separately
	StringList	 files;
	StringList	 logfiles;
	char		*basedir = NULL;
	tmp = param( "REPLICATION_FILE_SET" );
	if ( NULL != tmp ) {
		files.initializeFromString( tmp );
		free( tmp );

		tmp = param( "REPLICATION_LOGFILE_SET" );
		if ( NULL != tmp ) {
			logfiles.initializeFromString( tmp );
			free( tmp );
		}

		basedir = param( "REPLICATION_BASEDIR" );
		if ( NULL == basedir ) {
			basedir = strdup( "" );
		}
	}
	else {
		if ( NULL != tmp ) {
			tmp = param( "STATE_FILE" );
		}
		if ( NULL == tmp ) {
			tmp = param( "NEGOTIATOR_STATE_FILE" );
		}
		if ( NULL == tmp ) {
			MyString	 state_file;
			state_file  = spool;
			state_file += DIR_DELIM_STRING;
			state_file += "Accountantnew.log";
			tmp = strdup( state_file.Value() );
		}
		ASSERT( tmp != NULL );
		basedir = strdup( "" );
		ASSERT( basedir != NULL );
		files.initializeFromString( tmp );
	}

	// Initialize the fileset object
	bool modified = false;
	if ( !m_fileset->init( *basedir, files, logfiles, modified ) ) {
		dprintf( D_ALWAYS, "Failed to initialize fileset\n" );
		status = false;
		goto cleanup;
	}

	tmp = param( "REPLICATION_VERSION_FILE" );
	if ( NULL == tmp ) {
		m_versionFilePath  = spool;
		m_versionFilePath += DIR_DELIM_STRING;
		if ( NULL == m_localName ) {
			m_versionFilePath += "ReplicationVersion";
		}
		else {
			m_versionFilePath += m_localName;
			m_versionFilePath += ".ReplicationVersion";
		}
	}
	else if ( *tmp == *DIR_DELIM_STRING ) {
		m_versionFilePath = tmp;
		free( tmp );
	}
	else {
		m_versionFilePath  = spool;
		m_versionFilePath += DIR_DELIM_STRING;
		m_versionFilePath =+ tmp;
		free( tmp );
	}
	
	m_versionRotator->init( m_versionFilePath, ".tmp", getpid() );

  cleanup:
	free( basedir );
	free( spool );

	return status;
}

bool
ReplicationBase::initAd( ClassAd &ad, const char *label ) const
{
	MyString	s;

    ad.SetMyTypeName(GENERIC_ADTYPE);
    ad.SetTargetTypeName( label );

    s.sprintf( "%s@%s -p %d",
			   label,
			   my_full_hostname(),
			   daemonCore->InfoCommandPort() );
    ad.Assign( ATTR_NAME, s );
	ad.Assign( ATTR_MY_ADDRESS, daemonCore->InfoCommandSinfulString() );

	return true;
}

bool
ReplicationBase::updateAd( ClassAd &ad, bool file_details ) const
{
	// Add info from the file set
	if ( !m_fileset->updateAd( ad, file_details ) ) {
		return false;
	}

	// Add info from the file set
	if ( !m_myVersion.updateAd( ad ) ) {
		return false;
	}
	return true;
}

/* Function   : commandHandlerCommon
 * Arguments  : command - command to handle request
 * 				stream  - socket, through which the data for the command
 *						  arrived
 * Description: Common command handler decoding
 *				from a peer replicator
 */
bool
ReplicationBase::commonCommandHandler(
	int				 command,
	Stream			*stream,
	const char		*from,
	ClassAd			&ad,
	ReplicatorPeer	&peer,
	bool			 eom )
{
	// Read the ClassAd off the wire
    stream->decode( );
	if ( !ad.initFromStream(*stream) ) {
        dprintf( D_ALWAYS,
				 "::commandHandler(%s): Failed to read ClassAd [%s]\n",
                 from, utilToString(command) );
	}

	// And, read the EOM
    if( eom && (!stream->end_of_message()) ) {
        dprintf( D_ALWAYS,
				 "::commandHandler(%s): read EOM failed from %s\n",
				 from, stream->peer_description() );
    }

	// Pull out the sinful from the address
	MyString	sinful;
	if ( !ad.LookupString( ATTR_MY_ADDRESS, sinful ) ) {
		dprintf( D_ALWAYS,
				 "commandHandler(%s) ERROR: %s not in ad received from %s\n",
				 from, ATTR_MY_ADDRESS, sinful.Value() );
		return false;
	}
	peer.init( sinful.Value() );

    dprintf( D_FULLDEBUG,
			 "::commandHandler(%s) received command %s (%d) from %s\n",
			 from, utilToString(command), command, sinful.Value() );

	return true;
}

bool
ReplicationBase::sameFiles(
	const ReplicatorSimpleFileSet	&fileset,
	const ReplicatorPeer			&peer ) const
{
	if ( m_fileset->sameFiles(fileset) ) {
		return true;
	}

	const char	*gfiles    = fileset.getFileList().print_to_string();
	const char	*glogfiles = fileset.getLogfileList().print_to_string();
	const char	*efiles    = m_fileset->getFileListStr( );
	const char	*elogfiles = m_fileset->getLogfileListStr( );
	dprintf( D_ALWAYS,
			 "ERROR: File set from peer %s mismatch:\n"
			 " got:       files='%s' logfiles='%s'\n"
			 " exptected: files='%s' logfiles='%s'\n",
			 peer.getSinfulStr(),
			 gfiles ? gfiles : "", glogfiles ? glogfiles : "",
			 efiles ? efiles : "", elogfiles ? elogfiles : "" );
	if ( gfiles )    free( const_cast<char *>(gfiles) );
	if ( glogfiles ) free( const_cast<char *>(glogfiles) );
	if ( efiles )    free( const_cast<char *>(efiles) );
	if ( elogfiles ) free( const_cast<char *>(elogfiles) );
	return false;
}

const char *
ReplicationBase::getDownloadExtension( void )
{
	return ".tmp.down";
}

const char *
ReplicationBase::getUploadExtension( void )
{
	return ".tmp.up";
}
