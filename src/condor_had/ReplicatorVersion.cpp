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
#include "condor_classad.h"
#include "ReplicatorVersion.h"
#include "ReplicatorPeer.h"

ReplicatorVersion::ReplicatorVersion( ReplicatorSimpleFileSet &file_set,
									  State state )
		: m_fileset( file_set )
{
	reset( state );
}
ReplicatorVersion::~ReplicatorVersion( void ) 
{
	reset( );
}

#if 0
ReplicatorVersion::ReplicatorVersion( const ReplicatorVersion &other ) 
		: m_mtime( other.getMtime() ),
		  m_gid( other.getGid() ),
		  m_logicalClock( other.getLogicalClock() ),
		  m_isPrimary( other.getIsPrimary() ),
		  m_state( other.getState() ),
		  m_fileset( other.getFileSet() )
{
}
#endif

bool
ReplicatorVersion::reset( State state ) 
{
	m_mtime = -1;
	m_gid = 0;
	m_logicalClock = 0;
	m_isPrimary = false;
	m_state = state;
	return true;
}

bool
ReplicatorVersion::operator > ( const ReplicatorVersion &other ) const
{
	if ( isState(STATE_LEADER)  &&  !other.isState(STATE_LEADER) ) {
		return true;
	}
	if ( !isState(STATE_LEADER)  &&  other.isState(STATE_LEADER) ) {
		return false;
	}
	return getLogicalClock() > other.getLogicalClock();
}

bool
ReplicatorVersion::operator >= ( const ReplicatorVersion &other) const
{
    return !(other > *this);
}

const char *
ReplicatorVersion::toString( MyString &str ) const
{
    str += "logicalClock = ";
    str += m_logicalClock;
    str += ", gid = ";
    str += m_gid;
    
    return str.Value();
}

bool
ReplicatorVersion::update( const ReplicatorVersion &other )
{
	this->m_mtime			= other.getMtime( );
	this->m_gid				= other.getGid( );
	this->m_logicalClock	= other.getLogicalClock( );
	this->m_isPrimary		= other.getIsPrimary( );
	this->m_state			= other.getState( );
	this->m_lastUpdate		= time(NULL);
	return true;
}

bool
ReplicatorVersion::setRandomGid( void )
{
	int		tmp_gid;
	while( 1 ) {
		tmp_gid = rand();
		if ( diffGid(tmp_gid) ) {
			setGid( tmp_gid );
			return true;
		}
	}
	return true;
}

struct StateTable
{
	ReplicatorVersion::State	 m_num;
	const char					*m_name;
};
static StateTable table[] =
{
	{ ReplicatorVersion::STATE_INVALID,		"Invalid" },
	{ ReplicatorVersion::STATE_REQUESTING,	"Requesting" },
	{ ReplicatorVersion::STATE_DOWNLOADING,	"Downloading" },
	{ ReplicatorVersion::STATE_BACKUP,		"Backup" },
	{ ReplicatorVersion::STATE_LEADER,		"LEADER" },
	{ ReplicatorVersion::STATE_INVALID,		NULL }
};

const char *
ReplicatorVersion::getStateName( State state ) const
{
	int			 num;
	StateTable	*s;
	for( num = 0, s = &table[0]; s->m_name != NULL; num++, s++ ) {
		if ( s->m_num == state ) {
			return s->m_name;
		}
	}
	static char	buf[64];
	snprintf( buf, sizeof(buf), "Unknow state %d", (int)state);
	return buf;
};
