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
#include "gidc.h"
#include "gidd.h"

int
gidc_alloc(gid_t* gid)
{
	int fd = udc_connect(GIDD_SOCKET);
	if (fd == -1) {
		err_sprintf("gidc_alloc error: %s", err_str);
		return -1;
	}

	if (udc_authenticate(fd) == -1) {
		err_sprintf("gidc_alloc error: %s", err_str);
		return -1;
	}

	int cmd = GIDD_ALLOC;
	if (fullio_write(fd, &cmd, sizeof(cmd)) == -1) {
		err_sprintf("gidc_alloc error: %s", err_str);
		return -1;
	}

	uid_t uid = getuid();
	if (fullio_write(fd, &uid, sizeof(uid)) == -1) {
		err_sprintf("gidc_alloc error: %s", err_str);
		return -1;
	}

	if (fullio_read(fd, &gid, sizeof(*gid)) == -1) {
		err_sprintf("gidc_alloc error: %s", err_str);
		return -1;
	}

	close(fd);

	return 0;
}
int
gidc_test(gid_t* gids, int ngids, int* ans)
{
	int fd = udc_connect(GIDD_SOCKET);
	if (fd == -1) {
		err_sprintf("gidc_test error: %s", err_str);
		return -1;
	}

	if (udc_authenticate(fd) == -1) {
		err_sprintf("gidc_test error: %s", err_str);
		return -1;
	}

	uid_t uid = getuid();
	if (fullio_write(fd, &uid, sizeof(uid)) == -1) {
		err_sprintf("gidc_test error: %s", err_str);
		return -1;
	}

	if (fullio_write(fd, &ngids, sizeof(ngids)) == -1) {
		err_sprintf("gidc_test error: %s", err_str);
		return -1;
	}

	if (fullio_write(fd, &gids, ngids * sizeof(gid_t)) == -1) {
		err_sprintf("gidc_test error: %s", err_str);
		return -1;
	}

	if (fullio_read(fd, ans, sizeof(*ans)) == -1) {
		err_sprintf("gidc_test error: %s", err_str);
		return -1;
	}

	return 0;
}

