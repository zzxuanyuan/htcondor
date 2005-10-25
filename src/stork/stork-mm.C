/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
#include "stork-mm.h"
#include "basename.h"
#include "condor_debug.h"
#include "condor_config.h"


// Stork interface object to new "matchmaker lite" for SC2005 demo.

// Instantiate some templates
template class list<DCMatchLiteLease*>;

void
StorkMatchEntry::initialize()
{
	Url = NULL;
	Lease = NULL;
	Expiration_time = 0;
	IdleExpiration_time = 0;
	CompletePath = NULL;
}


StorkMatchEntry::StorkMatchEntry()
{
	initialize();
}

StorkMatchEntry::StorkMatchEntry(DCMatchLiteLease * lite_lease)
{
	initialize();
	
	// init with lease ad
	ASSERT(lite_lease);
	Lease = lite_lease;

	// lookup Url here
	classad::ClassAd *ad = Lease->LeaseAd();
	ASSERT(ad);
	string dest;
	char *attr_name = param("STORK_MM_DEST_ATTRNAME");
	if ( !attr_name ) {
		attr_name = strdup("URL");  // default
	}
	ad->EvaluateAttrString(attr_name, dest);
	free(attr_name);
	if ( dest.length() ) {
		Url = strdup(dest.c_str());
	}
	ASSERT(Url);

	// setup Expiration_time here
	Expiration_time = Lease->LeaseExpiration();

}

StorkMatchEntry::StorkMatchEntry(const char* path)
{
	initialize();

	ASSERT(path);
	Url = condor_dirname( path );
	CompletePath = new MyString( path );
}


StorkMatchEntry::~StorkMatchEntry()
{
	if ( Url ) free(Url);
	if ( Lease ) delete Lease;
	if ( CompletePath ) delete CompletePath;
}


bool
StorkMatchEntry::ReleaseLeaseWhenDone()
{
	ASSERT(Lease);
	return Lease->ReleaseLeaseWhenDone();
}


int
StorkMatchEntry::operator< (const StorkMatchEntry& E2)
{
	time_t comp1, comp2;

	if ( Expiration_time==0 || E2.Expiration_time==0 ) {
		// must be searching just on Url, return false so we don't stop search early
		return 0;
	}

	if ( IdleExpiration_time ) {
		comp1 = MIN( IdleExpiration_time, Expiration_time );
	} else {
		comp1 = Expiration_time;
	}

	if ( E2.IdleExpiration_time ) {
		comp2 = MIN( E2.IdleExpiration_time, E2.Expiration_time );
	} else {
		comp2 = E2.Expiration_time;
	}

	if ( comp1 < comp2 ) {
		return 1;
	} else {
		return 0;
	}
}


int 
StorkMatchEntry::operator== (const StorkMatchEntry& E2)
{
	// If both entries have a lease id, that must be equal 

	if (Lease &&  E2.Lease ) {
		if ( ((Lease->LeaseId())==(E2.Lease->LeaseId())) ) {
			return 1;
		} else {
			return 0;
		}
	}

	// Else entrires are equal if their dest file is equal
	if (CompletePath && E2.CompletePath) {
		if ( *(CompletePath) == *(E2.CompletePath) ) {
			return 1;
		} else {
			return 0;
		}
	}

	// Else on Url
	if (Url && E2.Url ) {
		if ( strcmp(Url,E2.Url)==0 ) {
			return 1;
		} else {
			return 0;
		}
	}

	// If made it here, something is wrong!
	EXCEPT("StorkMatchEntry operator== : Internal inconsistancy!");
	return 0; // will never get here, just to make compiler happy
}

/*******************************************************************************/

// Constructor
StorkMatchMaker::
StorkMatchMaker(void)
{
	tid = -1;
	tid_interval = -1;

	char *mm_name = param("STORK_MM_NAME");
	char *mm_pool = param("STORK_MM_POOL");
	dcmm = new DCMatchLite(mm_name,mm_pool);
	ASSERT(dcmm);
	if (mm_name) free(mm_name);
	if (mm_pool) free(mm_pool);

	SetTimers();
}


// Destructor.
StorkMatchMaker::
~StorkMatchMaker(void)
{
	StorkMatchEntry* match;

	idleMatches.StartIterations();
	while ( idleMatches.Iterate(match) ) {
		ASSERT(match->Lease);
		// dprintf(D_FULLDEBUG,"TODD - idle remove %p ptr=%p cp=%p\n",match->Lease,match,match->CompletePath);
		delete match;
	}

	busyMatches.StartIterations();
	while ( busyMatches.Iterate(match) ) {
		ASSERT(match->Lease);
		// dprintf(D_FULLDEBUG,"TODD - busy remove %p ptr=%p cp=%p\n",match->Lease,match,match->CompletePath);
		delete match;
	}

	if (dcmm) delete dcmm;
}


// Get a dynamic transfer destination from the matchmaker by protocol.
// Caller must free the return value.
const char * 
StorkMatchMaker::
getTransferDestination(const char *protocol)
{
	StorkMatchEntry* match;

		// Grab some matches from the matchmaker if we don't have 
		// any locally cached.
	if ( idleMatches.Count() == 0 ) 
	{
		list<DCMatchLiteLease *> leases;
		int num = param_integer("STORK_MM_MATCHES_PER_REQUEST",10,1);
		int duration = param_integer("STORM_MM_MATCH_DURATION",1800,1);
		char *req = param("STORK_MM_REQUIREMENTS");
		if ( !req ) {
			req = strdup("True");
		}
		char *name = param("STORK_NAME");
		if ( !name ) {
			// TODO - need a unique name here
			name = strdup("whatever");
		}

		bool result = dcmm->getMatches(name, num, duration, req, "0.0", leases);

		free(req);
		free(name);

		if ( !result ) {
			dprintf(D_ALWAYS,"ERROR getmatches() failed, num=%d\n", num);
			return NULL;
		}

			// For all matches we get back, add to our idle set
		int count = 0;
		for ( list<DCMatchLiteLease *>::iterator iter=leases.begin();
			  iter != leases.end();
			  iter++ ) 
		{
			count++;
			match = new StorkMatchEntry( *iter );
			addToIdleSet(match);
		// dprintf(D_FULLDEBUG,"TODD - idle add %p ptr=%p cp=%p\n",match->Lease,match,match->CompletePath);
		}
		dprintf(D_ALWAYS,
			"MM: Requested %d matches from matchmaker, got %d back\n",
			num, count);
	}

		// Now if we have any idle matches, just give the first one.
	idleMatches.StartIterations();
	if ( idleMatches.Iterate(match) ) {
		// found one.

		// remove match from idle set.
		idleMatches.RemoveLast();

		// add it into the busy set.
		addToBusySet(match);

		// dprintf(D_FULLDEBUG,"TODD1 - idle add %p ptr=%p cp=%p\n",match->Lease,match,match->CompletePath);
		// return url + filename
		static int unique_num = 0;
		if ( unique_num == 0 ) {
			unique_num = (int) time(NULL);
		}
		if (!match->CompletePath) {
			match->CompletePath = new MyString;
			ASSERT(match->CompletePath);
		}
		match->CompletePath->sprintf("%s/file%d",match->GetUrl(),++unique_num);
		// dprintf(D_FULLDEBUG,"TODD2 - idle add %p ptr=%p cp=%p\n",match->Lease,match,match->CompletePath);
		return strdup(match->CompletePath->Value());
	}

	return NULL;	// failed!
}


// Return a dynamic transfer destination to the matchmaker. 
bool
StorkMatchMaker::
returnTransferDestination(const char * path)
{
	StorkMatchEntry match(path);

		// just move it from the busy set to the idle set
	return fromBusyToIdle( &match );
}


// Inform the matchmaker that a dynamic transfer destination has
// failed.
bool
StorkMatchMaker::
failTransferDestination(const char * path)
{
		// Remove this destination from BOTH busy and idle lists.
		// By doing so, we let the lease on this destination simply
		// expire, and ensure we do not give out this url again until that happens.

	ASSERT(path);
		// Create entry on *dirname* since we assume that all
		// transfers to this destination will fail if this one did.
	StorkMatchEntry match;
	match.Url = condor_dirname( path );
	
		// Call in a while loop, since we have multiple matches to this destination
	while ( destroyFromBusy( &match ) ) ;
	while ( destroyFromIdle( &match ) ) ;

	return true;	
}

bool
StorkMatchMaker::
destroyFromBusy(StorkMatchEntry*  match)
{
	StorkMatchEntry* temp;

	busyMatches.StartIterations();
	while (busyMatches.Iterate(temp)) {
		if ( *temp == *match ) {
			const char	*s;
			if ( temp->CompletePath ) {
				s = temp->CompletePath->GetCStr();
			} else if ( temp->Lease ) {
				s = temp->Lease->LeaseId().c_str();
			} else {
				s = temp->GetUrl();
			}
			dprintf( D_FULLDEBUG, "Destroying busy match %s\n", s );
			busyMatches.RemoveLast();
			delete temp;
			return true;
		}
	}
	return false;
}

bool
StorkMatchMaker::
destroyFromIdle(StorkMatchEntry*  match)
{
	StorkMatchEntry* temp;

	idleMatches.StartIterations();
	while (idleMatches.Iterate(temp)) {
		if ( *temp == *match ) {
			const char	*s;
			if ( temp->CompletePath ) {
				s = temp->CompletePath->GetCStr();
			} else if ( temp->Lease ) {
				s = temp->Lease->LeaseId().c_str();
			} else {
				s = temp->GetUrl();
			}
			dprintf( D_FULLDEBUG, "Destroying idle match %s\n", s );
			idleMatches.RemoveLast();
			delete temp;
			return true;
		}
	}
	return false;
}

bool
StorkMatchMaker::
addToBusySet(StorkMatchEntry* match)
{
	match->IdleExpiration_time = 0;
	ASSERT(match->Lease);
	busyMatches.Add(match);
	return true;
}

bool
StorkMatchMaker::
addToIdleSet(StorkMatchEntry* match)
{
	int n = param_integer("STORK_MM_MATCH_IDLE_TIME", 5 * 60, 0);

	match->IdleExpiration_time = time(NULL) + n;
	ASSERT(match->Lease);
	idleMatches.Add(match);
	return true;
}

bool
StorkMatchMaker::
fromBusyToIdle(StorkMatchEntry* match) 
{
	StorkMatchEntry* full_match = NULL;
	StorkMatchEntry* temp;

	if (match->Lease) {
		full_match = match;
		busyMatches.Remove(match);
	} else {
	  	// We need to find the entry
		busyMatches.StartIterations();
		while ( busyMatches.Iterate(temp) ) {
			if ( *temp==*match ) {
				// found it
				full_match = temp;
				busyMatches.RemoveLast();
				break;
			}
		}
		if ( !full_match ) {
			dprintf( D_FULLDEBUG,
					 "StorkMatchEntry fromBusyToIdle: Can't find match %s!\n",
					 match->GetUrl() );
			return false;
		}
	}

	if ( full_match->Lease ) {
		addToIdleSet(full_match);
		return true;
	} else {
		return false;
	}
}

void
StorkMatchMaker::
SetTimers()
{
#ifndef TEST_VERSION
	int interval = param_integer("STORK_MM_INTERVAL",30);

	if ( interval == tid_interval  && tid != -1 ) {
		// we are already done, since we already
		// have a timer set with the desired interval
		return;
	}

	// If we made it here, the timer needs to be re-created
	// either because (a) it never existed, or (b) the interval changed

	if ( tid != -1 ) {
		// destroy pre-existing timer
		daemonCore->Cancel_Timer(tid);
	}

	// Create a new timer the correct interval
	tid = daemonCore->Register_Timer(interval,interval,
		(TimerHandlercpp)&StorkMatchMaker::timeout,
		"StorkMatchMaker::timeout", this);
	tid_interval = interval;
#endif
}

void
StorkMatchMaker::
timeout()
{
	time_t now = time(NULL);
	int near_future = param_integer("STORK_MM_LEASE_REFRESH_PADDING",10*60,30);
	StorkMatchEntry* match;
	Set<StorkMatchEntry*> to_release, to_renew;
	bool result;
	

	// =====================================================================
	// First, deal with old idle matches.  For an old idle match,
	// we either need to simply remove it if it expired, or give it
	// back to the negotiator.
	idleMatches.StartIterations();
	while ( idleMatches.Iterate(match) ) {
		ASSERT(match->Expiration_time);
		ASSERT(match->IdleExpiration_time);
		if ( match->Expiration_time <= now ) {
			// This match expired, just delete it.
			idleMatches.RemoveLast();
			delete match;
			continue;
		}
		if ( match->IdleExpiration_time <= now ) {
			// This match has not expired, but has been sitting on our
			// idle list for too long.  Add it to the list of leases
			// to release, and remove from the list.
			to_release.Add(match);
			idleMatches.RemoveLast();
			continue;
		}
#ifdef USING_ORDERED_SET
		// If we made it here, we know everything else on the list is
		// for the future, since the list is ordered.  So break out,
		// we're done.
		break;
#endif
	}
	// Now actually connect to the matchmaker if we have leases to release.
	if ( to_release.Count() ) {
			// create list of lease ads to release
		list<const DCMatchLiteLease*> leases;
		to_release.StartIterations();
		while ( to_release.Iterate(match) ) {
			ASSERT(match->Lease);
			leases.push_back(match->Lease);
		}
			// send our list to the matchmaker
		result = dcmm->releaseLeases(leases);
		dprintf(D_ALWAYS,"MM: %s release %d leases\n", 
						result ? "Successful" : "Failed to ",
						to_release.Count());
			// deallocate memory
		to_release.StartIterations();
		while ( to_release.Iterate(match) ) {
			delete match;
		}
	}

	// =====================================================================
	// Now deal with busy matches that need to be refreshed.
	busyMatches.StartIterations();
	while ( busyMatches.Iterate(match) ) {
		ASSERT(match->Expiration_time);
		if ( match->Expiration_time <= now ) {
			// This match _already_ expired, just delete it.
			busyMatches.RemoveLast();
			delete match;
			continue;
		}
		if ( match->Expiration_time <= now + near_future ) {
			// This match is about to expire, move it to renew list.
			busyMatches.RemoveLast();
			to_renew.Add(match);
			continue;
		}
#ifdef USING_ORDERED_SET
		// If we made it here, we know everything else on the list is
		// for the future, since the list is ordered.  So break out, we're done.
		break;
#endif
	}
	// Now actually connect to the matchmaker if we have leases to renew.
	if ( to_renew.Count() ) {
			// create list of lease ads
		list<const DCMatchLiteLease*> input_leases;
		list<DCMatchLiteLease*> output_leases;
		to_renew.StartIterations();
		while ( to_renew.Iterate(match) ) {
			ASSERT(match->Lease);
			input_leases.push_back(match->Lease);
		}
			// send our list to the matchmaker
		result = dcmm->renewLeases(input_leases,output_leases);
		dprintf(D_ALWAYS,"MM: %s renew %d leases\n", 
						result ? "Successful" : "Failed to ",
						to_renew.Count());
			// update the expiration counters for all renewed leases
		if ( result ) {
			for ( list<DCMatchLiteLease *>::iterator iter=output_leases.begin();
				  iter != output_leases.end();
				  iter++ ) 
			{
				bool found = false;
				to_renew.StartIterations();
				while ( to_renew.Iterate(match) ) {
					ASSERT(match->Lease);
					if ( match->Lease->LeaseId() == (*iter)->LeaseId() ) {
						match->Lease->copyUpdates( **iter );
						found = true;
						break;
					}
				}
				ASSERT(found);	// an Id in our output list better be found in our input list!
			}
			DCMatchLiteLease_FreeList( output_leases );
		}
			// now put em all back onto the busy set, refreshed or not.
		to_renew.StartIterations();
		while ( to_renew.Iterate(match) ) {
			addToBusySet(match);
		}
	}

		// reset timers to go off again as appropriate
	SetTimers();
}
