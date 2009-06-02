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
#include "internet.h"

#include "ReplicatorPeer.h"

ReplicatorPeer::ReplicatorPeer( const char *sinful )
		: m_sinfulString( sinful ),
		  m_isPrimary( false )
{
	m_hostname = getHostFromAddr( m_sinfulString.Value( ) );
}

ReplicatorPeer::~ReplicatorPeer( void )
{
}

bool
ReplicatorPeer::operator == ( const char *hostname ) const
{
	return ( 0 == strcasecmp(hostname.Value(), m_hostname) );
}

bool
ReplicatorPeer::operator == ( const ReplicatorPeer &other ) const
{
	return *this == other.getHostName( );
}
