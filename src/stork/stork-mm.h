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

#include "condor_common.h"

// Stork interface object to new "matchmaker lite" for SC2005 demo.
class StorkMatchMaker 
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
		// destination URL of the format "proto://host/dir/file".  This means a
		// transfer directory has been created on the destination host.
		// Function returns a pointer to dynamically allocated string, which
		// must be free()'ed by the caller after use.
		// Question: Where does the "file" portion come from?  Should we create
		// a random/unique filename on the fly?
		// Return NULL if there are no destinations available.
		//
		// Todd: if you want to dump the protocol arg,
		// and assume "gsiftp" for the entire demo, just update this
		// declaration, and I'll spot it.
		const char * getTransferDestination(const char *protocol);
		
		// Return a dynamic transfer destination to the matchmaker.  Stork
		// calls this method when it is no longer using the transfer
		// destination.  Matchmaker returns false upon error.
		bool returnTransferDestination(const char * url);

		// Inform the matchmaker that a dynamic transfer destination has
		// failed.  Matchmaker returns false upon error.
		bool failTransferDestination(const char * url);

}; // class StorkMatchMaker

#endif // _STORK_MM_

