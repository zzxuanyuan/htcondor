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
#ifndef _STORK_MM_
#define _STORK_MM_

/* #define TEST_VERSION 1 */

#ifndef TEST_VERSION
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#endif

#include "dc_match_lite.h"
#include "MyString.h"

// Turn on/off the use of ordered sets
#define USING_ORDERED_SET
#if defined( USING_ORDERED_SET )
# include "OrderedSet.h"
#else
# include "Set.h"
#endif

class StorkMatchMaker; // forward reference

class StorkMatchEntry {
	friend class StorkMatchMaker;
	public:
		StorkMatchEntry();
		StorkMatchEntry(DCMatchLiteLease * lite_lease);
		StorkMatchEntry(const char* url);
		~StorkMatchEntry();

		const char* GetUrl() { return Url; };

		bool ReleaseLeaseWhenDone();
		time_t GetExpirationTime() {return Expiration_time; };
		
		int operator< (const StorkMatchEntry& E2);
		int operator== (const StorkMatchEntry& E2);

	protected:
		void initialize();
		time_t Expiration_time;
		time_t IdleExpiration_time;
		char *Url;
		DCMatchLiteLease* Lease;
		MyString *CompletePath;

		
};

// Stork interface object to new "matchmaker lite" for SC2005 demo.
class StorkMatchMaker 
#ifndef TEST_VERSION
	: public Service
#endif
{
	public:
		// Constructor.  For now, I'll assume there are no args to the
		// constructor.  Perhaps later we'll decide to specify a matchmaker to
		// connect to.  Stork calls the constructor at daemon startup time.
		StorkMatchMaker(void);

		// Destructor.  Stork calls the destructor at daemon shutdown time.
		~StorkMatchMaker(void);

		// Get a dynamic transfer destination from the matchmaker by protocol,
		// e.g. "gsiftp", "http", "file", etc.  This method returns a
		// destination URL of the format "proto://host/dir/".  This means a
		// transfer directory has been created on the destination host.
		// Function returns a pointer to dynamically allocated string, which
		// must be free()'ed by the caller after use.
		// Return NULL if there are no destinations available.
		const char * getTransferDirectory(const char *protocol);

		// WARNING:  This method not yet tested!
		// Get a dynamic transfer destination from the matchmaker by protocol,
		// e.g. "gsiftp", "http", "file", etc.  This method returns a
		// destination URL of the format "proto://host/dir/file".  This means a
		// transfer directory has been created on the destination host.
		// Function returns a pointer to dynamically allocated string, which
		// must be free()'ed by the caller after use.
		// Return NULL if there are no destinations available.
		const char * getTransferFile(const char *protocol); // NOT TESTED!

		// Return a dynamic transfer destination to the matchmaker.  Stork
		// calls this method when it is no longer using the transfer
		// destination.  Matchmaker returns false upon error.
		bool returnTransferDestination(const char * url);

		// Inform the matchmaker that a dynamic transfer destination has
		// failed.  Matchmaker returns false upon error.
		bool failTransferDestination(const char * url);

		// Returns false if no matches are avail, else true
		bool areMatchesAvailable();

	protected:
		bool getMatchesFromMatchmaker();
		StorkMatchEntry * getTransferDestination(const char *protocol);
		bool destroyFromBusy(StorkMatchEntry * match);
		bool destroyFromIdle(StorkMatchEntry * match);
		bool addToBusySet(StorkMatchEntry * match);
		bool addToIdleSet(StorkMatchEntry * match);
		bool fromBusyToIdle(StorkMatchEntry * match);
		void SetTimers();
		void timeout();

	private:
#if defined( USING_ORDERED_SET )
		OrderedSet<StorkMatchEntry*> busyMatches;
		OrderedSet<StorkMatchEntry*> idleMatches;
#else
		Set<StorkMatchEntry*> busyMatches;
		Set<StorkMatchEntry*> idleMatches;
#endif
		char *mm_name;
		char *mm_pool;
		int tid, tid_interval;

}; // class StorkMatchMaker

#endif // _STORK_MM_

