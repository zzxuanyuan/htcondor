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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include <list>
#include <map>

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

#include "lease_manager_lease_ent.h"


// **************************************************
// Lease manager lease ent class implementation
// **************************************************
LeaseManagerLeaseEnt::LeaseManagerLeaseEnt( void )
{
	m_lease_ad = NULL;
	m_lease_number = 0;
	m_leases_ad = NULL;
	m_expiration = 0;

	m_resource_ad = NULL;
	m_resource_name = "";
	m_lazy_expire = false;
}
LeaseManagerLeaseEnt::LeaseManagerLeaseEnt( 
	classad::ClassAd	*lease_ad,
	int					 lease_number,
	classad::ClassAd	*leases_ad,
	int					 expiration,
	classad::ClassAd	*resource_ad,
	const string		&resource_name,
	int					 lazy_expire
	)
{
	m_lease_ad = lease_ad;
	m_lease_number = lease_number;
	m_leases_ad = leases_ad;
	m_expiration = expiration;

	m_resource_ad = resource_ad;
	m_resource_name = resource_name;
	m_lazy_expire = lazy_expire;
}

LeaseManagerLeaseEnt::~LeaseManagerLeaseEnt( void )
{
	// nothing to do
}

bool
LeaseManagerLeaseEnt::getValid( bool &valid ) const
{
	return m_lease_ad->EvaluateAttrBool( "LeaseValid", valid );
}

bool
LeaseManagerLeaseEnt::getExpiredTime( int &expired_time ) const
{
	return m_lease_ad->EvaluateAttrInt( "LeaseExpiredTime", expired_time );
}

bool
LeaseManagerLeaseEnt::getCreationTime( int &creation_time ) const
{
	return m_lease_ad->EvaluateAttrInt( creation_time );
}
