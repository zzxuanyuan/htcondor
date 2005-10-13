/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _CONDOR_DC_MATCH_LITE_LEASE_H
#define _CONDOR_DC_MATCH_LITE_LEASE_H

#include <list>
#include <string>
#include "condor_common.h"
#include "stream.h"
#include "daemon.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"
using namespace std;


class DCMatchLiteLease {
  public:

	// Constructors / destructors
	DCMatchLiteLease( time_t now = 0 );
	DCMatchLiteLease( const DCMatchLiteLease &, time_t now = 0 );
	DCMatchLiteLease( classad::ClassAd *, time_t now = 0 );
	DCMatchLiteLease( const classad::ClassAd &, time_t now = 0 );
	DCMatchLiteLease( const string &lease_id,
					  int lease_duration = 0,
					  bool release_when_done = true,
					  time_t now = 0 );
	~DCMatchLiteLease( void );

	// Various initialization methods
	int initFromClassAd( classad::ClassAd *ad, time_t now = 0 );
	int initFromClassAd( const classad::ClassAd	&ad, time_t now = 0 );
	int copyUpdates( const DCMatchLiteLease & );

	// Accessors
	const string &LeaseId( void ) const
		{ return lease_id; };
	int LeaseDuration( void ) const
		{ return lease_duration; };
	classad::ClassAd *LeaseAd( void ) const
		{ return lease_ad; };
	bool ReleaseLeaseWhenDone( void ) const
		{ return release_lease_when_done; };
	int LeaseTime( void ) const
		{ return lease_time; };

	// Set methods
	int setLeaseId( const string & );
	int setLeaseDuration( int );
	int setLeaseStart( time_t now = 0 );

	// Mark / getmark methods
	bool setMark( bool mark )
		{ this->mark = mark; return mark; };
	bool getMark( void ) const
		{ return mark; };

	// Helper methods to help determine how much time is left on a lease
	time_t getNow( time_t now = 0 ) const
		{ if ( !now ) { now = time( NULL ); } return now; };
	int LeaseExpiration( void ) const
		{ return lease_time + lease_duration; };
	int LeaseRemaining( time_t now = 0 ) const
		{ return getNow( now ) - ( lease_time + lease_duration ); };
	bool LeaseExpired( time_t now = 0 ) const
		{ return ( LeaseRemaining() > 0 ); };

  private:
	classad::ClassAd	*lease_ad;
	string				lease_id;
	int					lease_duration;
	int					lease_time;
	bool				release_lease_when_done;
	bool				mark;
};

// Free a list of leases
void DCMatchLiteLease_FreeList(
	list<DCMatchLiteLease *>		&lease_list
	);
int DCMatchLiteLease_RemoveLeases(
	list<DCMatchLiteLease *>		&lease_list,
	const list<const DCMatchLiteLease *> &remove_list
	);
int DCMatchLiteLease_UpdateLeases(
	list<DCMatchLiteLease *>		&lease_list,
	const list<const DCMatchLiteLease *> &update_list
	);
int DCMatchLiteLease_MarkLeases(
	list<DCMatchLiteLease *>		&lease_list,
	bool							mark
	);
int DCMatchLiteLease_RemoveMarkedLeases(
	list<DCMatchLiteLease *>		&lease_list,
	bool							mark
	);
int DCMatchLiteLease_CountMarkedLeases(
	const list<const DCMatchLiteLease *> &lease_list,
	bool							mark
	);

#endif /* _CONDOR_DC_MATCH_LITE_LEASE_H */
