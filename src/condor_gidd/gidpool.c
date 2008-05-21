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

#include "err.h"
#include "gidpool.h"
#include "giduse.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// structure for information regarding the state of a tracking GID
//
typedef struct gidlist {
	gid_t gid;
	uid_t uid;
	struct gidlist* next;
} gidlist_t;

// we maintain two lists of gidinfo structures, one for allocated GIDs and
// another for free GIDs
//
static gidlist_t* s_used = NULL;
static gidlist_t* s_free = NULL;

// the range of GIDs that we've been initialized to use
//
static gid_t s_min = 0;
static int s_count = 0;

// helper to free up the memory used for a GID list
//
static void
freelist(gidlist_t* list)
{
	while (list != NULL) {
		gidlist_t* next = list->next;
		free(list);
		list = next;
	}
}

// helper to reclaim GIDs that were allocated but are no longer in use
//
static void
reclaim(void)
{
	int used[s_count];
	if (giduse_probe(s_min, s_count, used) == -1) {
		// FIXME: figure out logging
		assert(0);
	}
	gidlist_t** lastptr = &s_used;
	gidlist_t* node = s_used;
	while (node != NULL) {
		if (used[node->gid - s_min] == 0) {
			*lastptr = node->next;
			node->next = s_free;
			s_free = node;
			node = *lastptr;
			// FIXME: figure out logging
		}
		else {
			lastptr = &node->next;
			node = node->next;
		}
	}
}

int
gidpool_init(gid_t min, int count)
{
	s_min = min;
	s_count = count;
	for (gid_t gid = min + count - 1; gid >= min; gid--) {
		gidlist_t* node = malloc(sizeof(struct gidlist));
		if (node == NULL) {
			err_sprintf("malloc failure: %s", strerror(errno));
			freelist(s_free);
			return -1;
		}
		node->gid = gid;
		node->uid = 0;
		node->next = s_free;
		s_free = node;
	}
	return 0;
}

gid_t
gidpool_alloc(uid_t uid)
{
	if (s_free == NULL) {
		reclaim();
		if (s_free == NULL) {
			return 0;
		}
	}
	gidlist_t* node = s_free;
	s_free = s_free->next;
	node->uid = uid;
	node->next = s_used;
	s_used = node;
	return node->gid;
}

int
gidpool_test(uid_t uid, gid_t* gids, int ngids)
{
	for (gidlist_t* node = s_used; node != NULL; node = node->next) {
		for (int i = 0; i < ngids; i++) {
			if ((node->gid = gids[i]) && (node->uid == uid)) {
				return 1;
			}
		}
	}
	return 0;
}
