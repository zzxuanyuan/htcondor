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

#include "stork-mm.h"

// Stork interface object to new "matchmaker lite" for SC2005 demo.

// Constructor
StorkMatchMaker::
StorkMatchMaker(void)
{
	return;	// stubbed for now
}

// Destructor.
StorkMatchMaker::
~StorkMatchMaker(void)
{
	return;	// stubbed for now
}

// Get a dynamic transfer destination from the matchmaker by protocol,
const char * 
StorkMatchMaker::
getTransferDestination(const char *protocol)
{
	return NULL;	// stubbed for now
}

// Return a dynamic transfer destination to the matchmaker.  Stork
bool
StorkMatchMaker::
returnTransferDestination(const char * url)
{
	return true;	// stubbed for now
}

// Inform the matchmaker that a dynamic transfer destination has
// failed.
bool
StorkMatchMaker::
failTransferDestination(const char * url)
{
	return true;	// stubbed for now
}

