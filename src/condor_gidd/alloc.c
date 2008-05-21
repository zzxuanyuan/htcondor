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

// this program allocates a GID for tracking purposes using an "ethernet"-
// style approach. it:
//   1) scans the system for unsed GIDs
//   2) selects one and adds it to its own supplementary group list
//   3) scans the system again expecting to see only itself using the GID
//   4) if no collision has occurred, it is done
//   5) if a collision has occurred, it retries

#include "err.h"
#include "giduse.h"
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

static gid_t
parsegid(char* str)
{
	if (!isdigit(str[0])) {
		return 0;
	}
	char* end;
	gid_t gid = (gid_t)strtoul(str, &end, 10);
	if (*end != '\0') {
		return 0;
	}
	return gid;
}

int
main(int argc, char* argv[])
{
	if (argc != 3) {
		fprintf(stderr, "usage: gidd_alloc <min_gid> <max_gid>\n");
		return 1;
	}
	gid_t min = parsegid(argv[1]);
	if (min == 0) {
		fprintf(stderr,
		        "invalid value for minimum GID: %s\n",
		        argv[1]);
		return 1;
	}
	gid_t max = parsegid(argv[2]);
	if (max == 0) {
		fprintf(stderr,
		        "invalid value for maximum GID: %s\n",
		        argv[2]);
		return 1;
	}
	if (min > max) {
		fprintf(stderr,
		        "invalid GID range given: %u - %u\n",
		        (unsigned)min,
		        (unsigned)max);
		return 1;
	}

	int count = max - min + 1;
	int used[count];
	if (giduse_probe(min, count, used) == 1) {
		fprintf(stderr, "giduse_probe error: %s", err_str);
		return 1;
	}

	gid_t gid = 0;
	for (int i = 0; i < count; i++) {
		if (used[i] == 0) {
			gid = min + i;
			break;
		}
	}
	if (gid == 0) {
		fprintf(stderr, "no GIDs available\n");
		return 1;
	}

	if (setgroups(1, &gid) == -1) {
		fprintf(stderr, "setgroups failure: %s\n", strerror(errno));
		return 1;
	}

	int n;
	if (giduse_probe(gid, 1, &n) == -1) {
		fprintf(stderr,
		        "giduse_probe error checking GID %u: %s\n",
		        (unsigned)gid,
		        err_str);
		return 1;
	}
	if (n != 1) {
		fprintf(stderr, "collision on GID %u\n", (unsigned)gid);
		return 1;
	}

	printf("%u\n", (unsigned)gid);
	fclose(stdout);

	pause();

	return 0;
}
