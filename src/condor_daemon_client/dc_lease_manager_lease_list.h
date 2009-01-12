/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
#ifndef _CONDOR_DC_LEASE_MANAGER_LEASE_LIST_H
#define _CONDOR_DC_LEASE_MANAGER_LEASE_LIST_H

#include <list>
#include <string>
#include "condor_common.h"
#include "stream.h"
#include "daemon.h"
#include <stdio.h>


#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

class DCLeaseManagerLease;

//
// Simple class to help with lists of leases
// This class does *no* automatic memory management for you or anything
//  like that, although it does provide some free() methods to assist you.
//
class DCLeaseManagerLeaseListRw;
class DCLeaseManagerLeaseListConst;

class DCLeaseManagerLeaseListBase
{
  public:
	DCLeaseManagerLeaseListBase( void );
	DCLeaseManagerLeaseListBase( const DCLeaseManagerLeaseListBase & );

	// Get size of list
	int count( void ) const;

	// Free a list of leases -- returns count
	int freeList( void );

  protected:
	list<DCLeaseManagerLease *>	leases;

};

class DCLeaseManagerLeaseListRw
{
  public:

	DCLeaseManagerLeaseList( list<DCLeaseManagerLease *> &leases );

	// Get the lease list
	list<DCLeaseManagerLease *> &getList( void ) const
		{ return lease_list; };

	// Remove leases from a list
	int removeLeases(
		const list<const DCLeaseManagerLease *> &remove_list
		);
	int removeLeases(
		const DCLeaseManagerConstLeaseList &remove_list
		);

	// Update a list of leases
	int updateLeases(
		const list<const DCLeaseManagerLease *> &update_list
	);
	int updateLeases(
		const DCLeaseManagerConstLeaseList &remove_list
	);


	// Lease mark operations
	int markLeases( bool mark_value );
	int removeMarkedLeases( bool mark_value );
	int countMarkedLeases( bool mark_value ) const;
	int getMarkedLeases(
		bool								 mark_value,
		list<const DCLeaseManagerLease *>	&marked_lease_list
		) const;
	int getMarkedLeases(
		bool								 mark_value,
		DCLeaseManagerLeaseConstList 		&marked_lease_list
		) const;

	
  private:
	list<DCLeaseManagerLease *>		*m_lease_list;

};

//
// Like the above, but works on an SLT list of const lease pointers
//
class DCLeaseManagerConstLeaseList
{
	DCLeaseManagerConstLeaseList( list<const DCLeaseManagerLease *> &leases );
	DCLeaseManagerConstLeaseList( list<DCLeaseManagerLease *> &leases );
	DCLeaseManagerLeaseList( const DCLeaseManagerLeaseList &leases );

	// Get the const lease list -- returns a ref, not a pointer
	list<const DCLeaseManagerLease *> &getConstList( const )
		{ return lease_list; };

  private:
	list<const DCLeaseManagerLease *>	*m_lease_list;
};



// Get a 'const' list of leases from a list of leases
list<const DCLeaseManagerLease *> &
DCLeaseManagerLease_getConstList(
	const list<DCLeaseManagerLease *>		&non_const_list
	);

// Write out a list of leases to a file (returns count)
int
DCLeaseManagerLease_fwriteList(
	const list<const DCLeaseManagerLease *> &lease_list,
	FILE									*fp
	);

// Read a list of leases from a file (returns count)
int
DCLeaseManagerLease_freadList(
	list<DCLeaseManagerLease *>				&lease_list,
	FILE									*fp
	);


#endif /* _CONDOR_DC_LEASE_MANAGER_LEASE_H */
