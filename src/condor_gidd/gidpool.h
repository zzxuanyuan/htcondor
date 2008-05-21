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

#ifndef _GIDPOOL_H
#define _GIDPOOL_H

// module for encapsulating a pool of GIDs dedicated for process tracking.
// this module is initialized with a range of GIDs. each GID can be in one
// of two states: allocated or free. each allocated GID is associated with
// the UID of the allocating user

#include <sys/types.h>

// initialize the module with the given range of GIDs. returns 0 on success,
// -1 on failure
//
int gidpool_init(gid_t min, int count);

// allocate a GID and associate it with the given UID. returns the
// allocated GID if successful or 0 if not
//
gid_t gidpool_alloc(uid_t uid);

// test if the given list of GIDs contains at least one that is allocated
// to the given UID. returns 1 if it does, 0 if it doesn't
//
int gidpool_test(uid_t uid, gid_t* gids, int ngids);

#endif
