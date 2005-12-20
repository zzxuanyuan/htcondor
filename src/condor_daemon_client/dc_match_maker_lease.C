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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_match_maker.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"
#include "newclassad_stream.h"
using namespace std;

#include <stdio.h>

// *** DCMatchLiteLease (class to hold lease informat)  class methods ***

DCMatchLiteLease::DCMatchLiteLease( time_t now )
{
	lease_ad = NULL;
	lease_duration = 0;
	release_lease_when_done = true;
	setLeaseStart( now );
}

DCMatchLiteLease::DCMatchLiteLease( const DCMatchLiteLease &lease,
									time_t now )
{
	if ( lease.LeaseAd( ) ) {
		this->lease_ad = new classad::ClassAd( *(lease.LeaseAd( )) );
	} else {
		this->lease_ad = NULL;
	}
	setLeaseId( lease.LeaseId() );
	setLeaseDuration( lease.LeaseDuration( ) );
	this->release_lease_when_done = lease.ReleaseLeaseWhenDone( );
	setLeaseStart( now );
}

DCMatchLiteLease::DCMatchLiteLease(
	classad::ClassAd		*ad,
	time_t					now
	)
{
	lease_ad = NULL;
	initFromClassAd( ad, now );
}

DCMatchLiteLease::DCMatchLiteLease(
const classad::ClassAd	&ad,
	time_t				now
	)
{
	lease_ad = NULL;
	initFromClassAd( ad, now );
}

DCMatchLiteLease::DCMatchLiteLease(
	const string		&lease_id,
	int					lease_duration,
	bool				release_when_done,
	time_t				now )
{
	this->lease_ad = NULL;
	setLeaseId( lease_id );
	setLeaseDuration( lease_duration );
	this->release_lease_when_done = release_when_done;
	setLeaseStart( now );
}

DCMatchLiteLease::~DCMatchLiteLease( void )
{
	if ( lease_ad ) {
		delete lease_ad;
	}
}

int
DCMatchLiteLease::setLeaseStart( time_t now )
{
	if ( !now ) {
		now = time( NULL );
	}
	this->lease_time = now;
	return 0;
}

int
DCMatchLiteLease::initFromClassAd(
	const classad::ClassAd	&ad,
	time_t					now
	)
{
	return initFromClassAd( new classad::ClassAd( ad ), now );
}

int
DCMatchLiteLease::initFromClassAd(
	classad::ClassAd	*ad,
	time_t				now
	)
{
	int		status = 0;
	if ( lease_ad && (lease_ad != ad ) ) {
		delete lease_ad;
		lease_ad = NULL;
	}
	if ( !ad ) {
		return 0;
	}

	lease_ad = ad;
	if ( !lease_ad->EvaluateAttrString( "LeaseId", lease_id ) ) {
		status = 1;
		lease_id = "";
	}
	if ( !lease_ad->EvaluateAttrInt( "LeaseDuration", lease_duration ) ) {
		status = 1;
		lease_duration = 0;
	}
	if ( !lease_ad->EvaluateAttrBool( "ReleaseWhenDone",
									  release_lease_when_done ) ) {
		status = 1;
		release_lease_when_done = true;
	}
	setLeaseStart( now );
	return status;
}

int
DCMatchLiteLease::copyUpdates( const DCMatchLiteLease &lease )
{

	// Don't touch the lease ID

	// Copy other attributes
	setLeaseDuration( lease.LeaseDuration( ) );
	this->release_lease_when_done = lease.ReleaseLeaseWhenDone( );
	setLeaseStart( lease.LeaseTime() );
	setMark( lease.getMark() );

	// If there's an ad in the lease to copy, free the old one & copy
	if ( lease.LeaseAd( ) ) {
		if ( this->lease_ad ) {
			delete lease_ad;
		}
		this->lease_ad = new classad::ClassAd( *(lease.LeaseAd( )) );
	}
	// Otherwise, if there is an old ad, update it
	else if ( this->lease_ad ) {
		this->lease_ad->InsertAttr( "LeaseDuration",
									this->lease_duration );
		this->lease_ad->InsertAttr( "ReleaseWhenDone",
									this->release_lease_when_done );
	}

	// All done
	return 0;
}

int
DCMatchLiteLease::setLeaseId(
	const string		&lease_id )
{
	this->lease_id = lease_id;
	return 0;
}

int
DCMatchLiteLease::setLeaseDuration(
	int					duration )
{
	this->lease_duration = duration;
	return 0;
}

// *** DCMatchLiteLease list helper functions

void
DCMatchLiteLease_FreeList( list<DCMatchLiteLease *> &lease_list )
{
	while( lease_list.size() ) {
		DCMatchLiteLease *lease = *(lease_list.begin( ));
		delete lease;
		lease_list.pop_front( );
	}
}

int
DCMatchLiteLease_RemoveLeases(
	list<DCMatchLiteLease *>		&lease_list,
	const list<const DCMatchLiteLease *>&remove_list
	)
{
	int		errors = 0;

	list<const DCMatchLiteLease *>::const_iterator iter;
	for( iter = remove_list.begin( ); iter != remove_list.end( ); iter++ ) {
		const DCMatchLiteLease	*remove = *iter;
		bool matched = false;
		for( list<DCMatchLiteLease *>::iterator iter = lease_list.begin();
			 iter != lease_list.end();
			 iter++ ) {
			DCMatchLiteLease	*lease = *iter;
			if ( remove->LeaseId() == lease->LeaseId() ) {
				matched = true;
				lease_list.erase( iter );	// Note: invalidates iter
				delete lease;
				break;
			}
		}
		if ( !matched ) {
			errors++;
		}
	}
	return errors;
}

int
DCMatchLiteLease_UpdateLeases(
	list<DCMatchLiteLease *>		&lease_list,
	const list<const DCMatchLiteLease *> &update_list
	)
{
	int		errors = 0;

	list<const DCMatchLiteLease *>::const_iterator iter;
	for( iter = update_list.begin( ); iter != update_list.end( ); iter++ ) {
		const DCMatchLiteLease	*update = *iter;
		bool matched = false;
		for( list<DCMatchLiteLease *>::iterator iter = lease_list.begin();
			 iter != lease_list.end();
			 iter++ ) {
			DCMatchLiteLease	*lease = *iter;
			if ( update->LeaseId() == lease->LeaseId() ) {
				matched = true;
				lease->copyUpdates( *update );
				break;
			}
		}
		if ( !matched ) {
			errors++;
		}
	}
	return errors;
}

int
DCMatchLiteLease_MarkLeases(
	list<DCMatchLiteLease *>		&lease_list,
	bool							mark
	)
{
	for( list<DCMatchLiteLease *>::iterator iter = lease_list.begin();
		 iter != lease_list.end();
		 iter++ ) {
		DCMatchLiteLease	*lease = *iter;
		lease->setMark( mark );
	}
	return 0;
}

int
DCMatchLiteLease_RemoveMarkedLeases(
	list<DCMatchLiteLease *>		&lease_list,
	bool							mark
	)
{
	list<DCMatchLiteLease *> remove_list;

	for( list<DCMatchLiteLease *>::iterator iter = lease_list.begin();
		 iter != lease_list.end();
		 iter++ ) {
		DCMatchLiteLease	*lease = *iter;
		if ( lease->getMark() == mark ) {
			remove_list.push_back( lease );
		}
	}

	for( list<DCMatchLiteLease *>::iterator iter = remove_list.begin();
		 iter != remove_list.end();
		 iter++ ) {
		DCMatchLiteLease	*lease = *iter;
		lease_list.remove( lease );
		delete lease;
	}

	return 0;
}

int
DCMatchLiteLease_CountMarkedLeases(
	const list<const DCMatchLiteLease *> &lease_list,
	bool							mark
	)
{
	int		count = 0;
	list<const DCMatchLiteLease *>::const_iterator iter;
	for( iter = lease_list.begin();
		 iter != lease_list.end();
		 iter++ ) {
		const DCMatchLiteLease	*lease = *iter;
		if ( mark == lease->getMark( ) ) {
			count++;
		}
	}
	return count;
}
