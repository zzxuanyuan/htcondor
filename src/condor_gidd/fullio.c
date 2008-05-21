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
#include "fullio.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

int
fullio_read(int fd, void* buf, size_t len)
{
	size_t left = len;
	while (left != 0) {
		ssize_t n = read(fd, buf, left);
		if (n == -1) {
			if (errno == EINTR) {
				continue;
			}
			err_sprintf("read failure: %s", strerror(errno));
			close(fd);
			return -1;
		}
		if (n == 0) {
			err_sprintf("read %u of expected %u bytes",
			            (unsigned)(len - left),
			            (unsigned)len);
			close(fd);
			return -1;
		}
		buf += n;
		left -= n;
	}
	return 0;
}

int
fullio_write(int fd, void* buf, size_t len)
{
	while (len != 0) {
		ssize_t n = write(fd, buf, len);
		if (n == -1) {
			if (errno == EINTR) {
				continue;
			}
			err_sprintf("write failure", strerror(errno));
			close(fd);
			return -1;
		}
		buf += n;
		len -= n;
	}
	return 0;
}
