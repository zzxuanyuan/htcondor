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
#include "internet.h"
#include "command_strings.h"

#include "ReplicatorPeer.h"

ReplicatorPeer::ReplicatorPeer( void )
		: m_sinful( NULL ),
		  m_sinfulStr( NULL ),
		  m_hostName( NULL ),
		  m_timeout( 0 ),
		  m_version( *this )
{
}

ReplicatorPeer::ReplicatorPeer( const ReplicatorPeer &other )
		: m_sinful( new Sinful(other.getSinful()) ),
		  m_sinfulStr( m_sinful->getSinful() ),
		  m_hostName( strdup( other.getHostName() ) ),
		  m_timeout( other.getTimeout() ),
		  m_version( *this )
{
	m_version.update( other.getVersionInfo() );
}

bool
ReplicatorPeer::init( const char *sinful, const char *hostname )
{
	if ( NULL == sinful ) {
		return false;
	}
	reset( );

	m_sinful = new Sinful(sinful);
	if ( !m_sinful->valid() ) {
		return false;
	}
	m_sinfulStr = m_sinful->getSinful();

	if ( NULL == hostname ) {
		m_hostName = getHostFromAddr( m_sinful->getHost() );
	}
	else {
		m_hostName = strdup(hostname);
	}
	if ( NULL == m_hostName ) {
		dprintf( D_ALWAYS,
				 "Can't find hostname for sinful %s", sinful );
		return false;
	}

	return true;
}

bool
ReplicatorPeer::init( const ReplicatorPeer &other )
{
	reset( );
	return init( other.getSinful(), other.getHostName() );
}

bool
ReplicatorPeer::init( const Sinful &sinful, const char *hostname )
{
	reset( );

	m_sinful = new Sinful(sinful);
	if ( !m_sinful->valid() ) {
		return false;
	}
	m_sinfulStr = m_sinful->getSinful();

	if ( NULL == hostname ) {
		m_hostName = getHostFromAddr( m_sinful->getHost() );
	}
	else {
		m_hostName = strdup(hostname);
	}
	if ( NULL == m_hostName ) {
		dprintf( D_ALWAYS,
				 "Can't find hostname for sinful %s", m_sinfulStr );
		return false;
	}

	return true;
}

ReplicatorPeer::~ReplicatorPeer( void )
{
	reset( );
}

bool
ReplicatorPeer::reset( void )
{
	if ( m_sinful ) {
		delete( m_sinful );
		m_sinful = NULL;
	}
	// DON'T delete m_sinfulStr -- it's owned by m_sinful
	if ( m_hostName ) {
		free( m_hostName );
		m_hostName = NULL;
	}
	m_timeout = 0;
	return m_version.reset( );
}

bool
ReplicatorPeer::sendMessage( int command,
							 const ClassAd *ad,
							 ReliSock *sock ) const
{
	if ( !m_sinful ) {
		dprintf( D_ALWAYS, "sendMessage: no sinful defined!\n" );
		return false;
	}

    Daemon		daemon( DT_ANY, m_sinful->getSinful() );
	ReliSock	local_sock;
	bool		send_eom = false;
	if ( NULL == sock ) {
		sock = &local_sock;
		send_eom = true;
	}

    // no retries after 'm_connectionTimeout' seconds of unsuccessful
    // connection
	if ( m_timeout ) {
		sock->timeout( m_timeout );
		sock->doNotEnforceMinimalCONNECT_TIMEOUT( );
	}

    if( ! sock->connect( m_sinfulStr, 0, false ) ) {
        dprintf( D_ALWAYS,
				 "::startMessage(): unable to connect to %s\n",
				 m_sinfulStr );
		sock->close( );
        return false;
    }

    if( ! daemon.startCommand( command, sock, m_timeout ) ) {
        dprintf( D_ALWAYS,
				 "::startMessage() cannot start command %s to %s\n",
				 getCommandString(command), m_sinfulStr );
		sock->close( );
        return false;
    }

	if ( NULL != ad ) {
		dprintf( D_FULLDEBUG, "Sending ad\n" );
		ClassAd	*adptr = const_cast<ClassAd *>( ad );
		if ( !adptr->put(*sock) ) {
			dprintf( D_ALWAYS,
					 "::startMessage(): failed to send classad to %s\n",
					 m_sinfulStr );
			sock->close( );
			return false;
		}
	}

	if ( send_eom ) {
		dprintf( D_FULLDEBUG, "Sending EOM\n" );
		if( ! sock->eom( ) ) {
			sock->close( );
			dprintf( D_ALWAYS, "::sendCommand(): failed to send EOM\n" );
			return false;
		}
   	}
	else {
		dprintf( D_FULLDEBUG, "Not sending EOM\n" );
	}

	return true;
}

bool
ReplicatorPeer::operator == ( const Sinful &sinful ) const
{
	if ( !m_sinful ) {
		return false;
	}
	return *m_sinful == sinful;
}

bool
ReplicatorPeer::operator == ( const ReplicatorPeer &other ) const
{
	return *this == other.getSinful( );
}
