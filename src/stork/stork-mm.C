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
//template class Set<StorkMatchEntry>;
//template class OrderedSet<StorkMatchEntry>;
template class list<DCMatchLiteLease*>;

StorkMatchEntry::StorkMatchEntry()
{
	Url = NULL;
	Lease = NULL;
	Expiration_time = 0;
	IdleExpiration_time = 0;
	deallocate = false;
	CompletePath = NULL;
}

StorkMatchEntry::StorkMatchEntry(DCMatchLiteLease * lite_lease)
{
	StorkMatchEntry();
	
	// init with lease ad
	ASSERT(lite_lease);
	Lease = lite_lease;

	// lookup Url here
	classad::ClassAd *ad = Lease->LeaseAd();
	ASSERT(ad);
	string dest;
	char *attr_name = param("STORM_MM_DEST_ATTRNAME");
	if ( !attr_name ) {
		attr_name = strdup("STORK_DEST_SUBDIR");  // default
	}
	ad->EvaluateAttrString(attr_name, dest);
	free(attr_name);
	if ( dest.c_str() ) {
		Url = strdup(dest.c_str());
	}
	ASSERT(Url);

	// setup Expiration_time here
	Expiration_time = time(NULL) + Lease->LeaseDuration();

}

StorkMatchEntry::StorkMatchEntry(const char* path)
{
	StorkMatchEntry();

	ASSERT(path);
	Url = strdup( dirname(path) );
	CompletePath = new MyString(path);
}


StorkMatchEntry::~StorkMatchEntry()
{
	if ( deallocate ) {
		if ( Url ) free(Url);
		if ( Lease ) delete Lease;
		if ( CompletePath ) delete CompletePath;
	}
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
	// TODO - set timer
	return;	// stubbed for now
}


// Destructor.
StorkMatchMaker::
~StorkMatchMaker(void)
{
	StorkMatchEntry match;

	idleMatches.StartIterations();
	while ( idleMatches.Iterate(match) ) {
		match.Deallocate();	
	}

	busyMatches.StartIterations();
	while ( busyMatches.Iterate(match) ) {
		match.Deallocate();	
	}
}


// Get a dynamic transfer destination from the matchmaker by protocol.
// Caller must free the return value.
const char * 
StorkMatchMaker::
getTransferDestination(const char *protocol)
{
	StorkMatchEntry match;

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
		char *mm_name = param("STORK_MM_NAME");
		char *mm_pool = param("STORK_MM_POOL");
		DCMatchLite dcmm(mm_name,mm_pool);

		bool result = dcmm.getMatches(name, num, duration, req, "0.0", leases);

		free(req);
		free(name);
		if (mm_name) free(mm_name);
		if (mm_pool) free(mm_pool);

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
			StorkMatchEntry match( *iter );
			addToIdleSet(match);
		}
		dprintf(D_ALWAYS,"Requested %d matches from matchmaker, got %d back\n",
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

		// return url + filename
		static int unique_num = 0;
		if ( unique_num == 0 ) {
			unique_num = (int) time(NULL);
		}
		if (!match.CompletePath) {
			match.CompletePath = new MyString;
			ASSERT(match.CompletePath);
		}
		match.CompletePath->sprintf("%s/file%d",match.GetUrl(),++unique_num);
		return strdup(match.CompletePath->Value());
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
	return fromBusyToIdle( match );
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
	match.Url = dirname(path);
	
		// Call in a while loop, since we have multiple matches to this destination
	while ( destroyFromBusy( match ) ) ;
	while ( destroyFromIdle( match ) ) ;

	return true;	
}

bool
StorkMatchMaker::
destroyFromBusy(StorkMatchEntry & match)
{
	StorkMatchEntry temp;

	busyMatches.StartIterations();
	while (busyMatches.Iterate(temp)) {
		if (temp==match) {
			temp.Deallocate();
			busyMatches.RemoveLast();
			return true;
		}
	}
	return false;
}

bool
StorkMatchMaker::
destroyFromIdle(StorkMatchEntry & match)
{
	StorkMatchEntry temp;

	idleMatches.StartIterations();
	while (idleMatches.Iterate(temp)) {
		if (temp==match) {
			temp.Deallocate();
			idleMatches.RemoveLast();
			return true;
		}
	}
	return false;
}

bool
StorkMatchMaker::
addToBusySet(StorkMatchEntry & match)
{
	match.IdleExpiration_time = 0;
	ASSERT(match.Lease);
	busyMatches.Add(match);
	return true;
}

bool
StorkMatchMaker::
addToIdleSet(StorkMatchEntry & match)
{
	int n = param_integer("STORK_MM_MATCH_IDLE_TIME", 5 * 60, 0);

	match.IdleExpiration_time = time(NULL) + n;
	ASSERT(match.Lease);
	idleMatches.Add(match);
	return true;
}

bool
StorkMatchMaker::
fromBusyToIdle(StorkMatchEntry & match) 
{
	StorkMatchEntry full_match, temp;

	if (match.Lease) {
		full_match = match;
		busyMatches.Remove(match);
	} else {
	  	// We need to find the entry
		busyMatches.StartIterations();
		while ( busyMatches.Iterate(temp) ) {
			if ( temp==match ) {
				// found it
				full_match = temp;
				busyMatches.RemoveLast();
				break;
			}
		}
	}

	if ( full_match.Lease ) {
		addToIdleSet(full_match);
		return true;
	} else {
		return false;
	}
}


