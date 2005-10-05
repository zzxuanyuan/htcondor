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

#ifndef _CONDOR_DC_MATCH_LITE_H
#define _CONDOR_DC_MATCH_LITE_H

#include <list>
#include <string>
#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"
using namespace std;


class DCMatchLiteLease {
  public:
	DCMatchLiteLease( void );
	DCMatchLiteLease( classad::ClassAd * );
	DCMatchLiteLease( const classad::ClassAd & );
	DCMatchLiteLease( const string &lease_id,
					  int lease_duration = 0,
					  bool release_when_done = true );
	~DCMatchLiteLease( void );

	int initFromClassAd( classad::ClassAd *ad );
	int initFromClassAd( const classad::ClassAd	&ad );

	const string &LeaseId( void ) const
		{ return lease_id; };
	int LeaseDuration( void ) const
		{ return lease_duration; };
	classad::ClassAd *LeaseAd( void ) const
		{ return lease_ad; };
	bool ReleaseLeaseWhenDone( void ) const
		{ return release_lease_when_done; };

	int setLeaseId( const string & );
	int setLeaseDuration( int );

  private:
	classad::ClassAd	*lease_ad;
	string				lease_id;
	int					lease_duration;
	bool				release_lease_when_done;
};

// Free a list of leases
void DCMatchLiteLease_FreeList( list<DCMatchLiteLease *> &lease_list );


/** The subclass of the Daemon object for talking to a match lite daemon
*/
class DCMatchLite : public Daemon {
  public:

		/** Constructor.  Same as a Daemon object.
		  @param name The name (or sinful string) of the daemon, NULL
		              if you want local  
		  @param pool The name of the pool, NULL if you want local
		*/
	DCMatchLite( const char* const name = NULL, const char* pool = NULL );

		/// Destructor.
	~DCMatchLite( );


		/** Get lease(s) which to match the requirements passed in
			@param requestor_name The logical name of the requestor
			@param num The number of of leases requested
			@param duration The requested duration (in seconds) of the leases
			@param requirements The requirements expression for the match
			@param rank The rank expression for the match (ignored for now)
			@param leases STL List of lease information
			The list pointers should be delete()ed when no longer used
			@return true on success, false on invalid input (NULL)
		*/
	bool getMatches( const char *requestor_name,
					 int num, int duration,
					 const char* requirements, const char *rank,
					 list< DCMatchLiteLease *> &leases );


		/** Get lease(s) which to match the requirements passed in
			@param ad (New) ClassAd which discribe the request
			@param leases STL List of lease information
			The list pointers should be delete()ed when no longer used
			@return true on success, false on invalid input (NULL)
		*/
	bool getMatches( const classad::ClassAd &ad,
					 list< DCMatchLiteLease *> &leases );


		/** Renew the leases specified
			@param leases STL List of leases to renew
			Lease ID & duration are required
			@param out_leases STL list of renewed leases
			The list pointers should be delete()ed when no longer used
		*/
	bool renewLeases( list< const DCMatchLiteLease *> &leases,
					  list< DCMatchLiteLease *> &out_leases );


		/** Release the leases specified
			@param leases STL list of lease information on leases to release
			@return true on success, false on invalid input (NULL)
		*/
	bool releasewLeases( list <const DCMatchLiteLease *> &leases );


 private:

		// I can't be copied (yet)
	DCMatchLite( const DCMatchLite& );
	DCMatchLite& operator = ( const DCMatchLite& );

	// Helper methods to get/send leases
	bool SendLeases(
		Stream							*stream,
		list< const DCMatchLiteLease *>	&l_list
		);
	bool GetLeases(
		Stream							*stream,
		std::list< DCMatchLiteLease *>	&l_list
		);

};

#endif /* _CONDOR_DC_MATCH_LITE_H */
