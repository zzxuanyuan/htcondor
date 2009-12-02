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
#include "condor_attributes.h"
#include "condor_debug.h"
#include "ReplicatorRemoteVersion.h"
#include "ReplicatorPeer.h"

ReplicatorRemoteVersion::ReplicatorRemoteVersion( const ReplicatorPeer &peer )
		: ReplicatorVersion( m_realFileset ),
		  m_peer( peer )
{
}

bool
ReplicatorRemoteVersion::initialize( const ClassAd &ad )
{
    int   		gid = -1;
    int   		clock = -1;
	int   		primary = 0;
	MyString	file_set;
	MyString	logfile_set;
	int			mtime = 0;
	int			state;

	if ( !ad.LookupString( ATTR_HAD_REPLICATION_FILE_SET, file_set ) ) {
		dprintf( D_ALWAYS,
				 "ReplicatorVersion ERROR: %s not in ad received\n",
				 ATTR_HAD_REPLICATION_FILE_SET );
		return false;
    }
	if ( !ad.LookupString( ATTR_HAD_REPLICATION_LOGFILE_SET, logfile_set ) ) {
		dprintf( D_ALWAYS,
				 "ReplicatorVersion ERROR: %s not in ad received\n",
				 ATTR_HAD_REPLICATION_LOGFILE_SET );
		return false;
    }
	if ( !ad.LookupInteger( ATTR_HAD_REPLICATION_GID, gid ) ) {
		dprintf( D_ALWAYS,
				 "ReplicatorVersion ERROR: %s not in ad received\n",
				 ATTR_HAD_REPLICATION_GID);
		return false;
    }
	if ( !ad.LookupInteger( ATTR_HAD_REPLICATION_LOGICAL_CLOCK, clock ) ) {
		dprintf( D_ALWAYS,
				 "ReplicatorVersion ERROR: %s not in ad received\n",
				 ATTR_HAD_REPLICATION_LOGICAL_CLOCK);
		return false;
    }
	if ( !ad.LookupBool( ATTR_HAD_REPLICATION_IS_PRIMARY, primary ) ) {
		dprintf( D_ALWAYS,
				 "ReplicatorVersion ERROR: %s not in ad received\n",
				 ATTR_HAD_REPLICATION_LOGICAL_CLOCK);
		return false;
    }
	if ( !ad.LookupInteger( ATTR_HAD_REPLICATION_MTIME, mtime ) ) {
		dprintf( D_ALWAYS,
				 "ReplicatorVersion ERROR: %s not in ad received\n",
				 ATTR_HAD_REPLICATION_MTIME);
		return false;
    }
	if ( !ad.LookupInteger( ATTR_HAD_REPLICATION_STATE_NUM, state ) ) {
		dprintf( D_ALWAYS,
				 "ReplicatorVersion ERROR: %s not in ad received\n",
				 ATTR_HAD_REPLICATION_STATE_NUM );
		return false;
    }

    m_gid          = gid;
    m_logicalClock = clock;
	m_isPrimary    = primary;
	m_mtime        = mtime;
	m_state        = ReplicatorVersion::State(state);
	return m_fileset.init( file_set, logfile_set );
}

bool
ReplicatorRemoteVersion::isPeerValid( void ) const
{
	return m_peer.isValid();
}

const char *
ReplicatorRemoteVersion::toString( MyString &str ) const
{
    str = "logicalClock = ";
    str += m_logicalClock;
    str += ", gid = ";
    str += m_gid;
    
    return str.Value();
}

//
// Mutable version
//

ReplicatorRemoteVersionMutable::ReplicatorRemoteVersionMutable(
	const ReplicatorPeer &peer )
		: ReplicatorRemoteVersion( peer )
{
}

bool
ReplicatorRemoteVersionMutable::setPeer( const ReplicatorRemoteVersion &other )
{
	return setPeer( other.getPeer() );
}

bool
ReplicatorRemoteVersionMutable::setPeer( const ReplicatorPeer &peer )
{
	ReplicatorPeer	*my_peer = const_cast<ReplicatorPeer *>( &m_peer );
	return my_peer->init( peer );
}
