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

#ifndef __LEASE_MANAGER_LEASE_ENT_H__
#define __LEASE_MANAGER_LEASE_ENT_H__

#include <list>
#include <map>

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#ifndef WANT_CLASSAD_NAMESPACE
# define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"

// Map leaseId's to the lease ClassAd -- for internal use only
class LeaseManagerLeaseEnt
{
  public:
	LeaseManagerLeaseEnt( void );
	LeaseManagerLeaseEnt( const classad::ClassAd	&lease_state_ad,
						  int						 lease_number,
						  classad::ClassAd			&leases_ad,
						  int						 expiration,
						  classad::ClassAd			&resource_ad,
						  const string				&resource_name,
						  list<const string *>		&clean_list,
						  bool						 lazy_expire
						  );
	~LeaseManagerLeaseEnt( void );

	// Accessors: lease info
	classad::ClassAd &getAd( void ) const { return *m_lease_ad; };
	classad::ClassAd *getAdPtr( void ) const { return m_lease_ad; };
	int getLeaseNumber( void ) const { return m_lease_number; };
	classad::ClassAd &getLeasesAd( void ) const { return m_leases_ad; };
	int getExpiration( void ) const { return m_expiration; };
	void setExpiration( int expiration ) { m_expiration = expiration; };

	// Get things from the lease's ad
	bool getIsValid( bool &valid ) const;
	bool getExpiredTime( int & ) const;
	bool getCreationTime( int & ) const;

	// Accessors: parent resource
	classad::ClassAd &getResourceAd( void ) const { return m_resource_ad; };
	const string &getResourceName( void ) const { return m_resource_name; };
	bool getLazyExpire( void ) const { return m_lazy_expire; };
	
  private:
	classad::ClassAd		*m_lease_ad;
	int						 m_lease_number;
	classad::ClassAd		&m_leases_ad;
	int						 m_expiration;

	classad::ClassAd		&m_resource_ad;
	const string			 m_resource_name;
	bool					 m_lazy_expire;
};


#endif	//__LEASE_MANAGER_LEASE_ENT_H__
