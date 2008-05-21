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

#ifndef _GIDC_H
#define _GIDC_H

// this module provides client-side stubs for gidd's commands

#include <sys/types.h>

// allocates a GID from the gidd. uses the real UID to determine who
// to allocate the GID to. returns 0 if communication with the gidd
// succeeds, -1 if not. if a GID is successfully allocated, the gid
// parameter will be set to the GID; otherwise, it will be set to 0
//
int gidc_alloc(gid_t* gid)

// tests whether a user currently has one of a set of GIDs allocated.
// return 0 if communication with the gidd is successful, -1 if not.
// if successful, the ans parameter will be set to 1 if our real UID
// has at least one of the given GIDs allocated, 0 otherwise
//
int gidc_test(gid_t* gids, int ngids, int* ans);

#endif
