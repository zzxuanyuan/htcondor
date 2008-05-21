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
#include "gidd.h"
#include "gidpool.h"
#include "log.h"
#include "uds.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// this function is called on initialization to do the standard stuff in
// order to become a UNIX daemon. this is mostly borrowed from APUE
//
// TODO: move the openlog call into log.c
// TODO: close all open FDs, not just 0, 1, and 2
// TODO: set up signal handlers for the standard stuff: HUP, TERM, ...
//
static void
daemonize(void)
{
	// initialize syslog. note that LOG_ODELAY is (redundantly, since it
	// is the default) given so that no FD is opened until the first
	// syslog() call. this is important since we don't want the code
	// below that sets up FDs 0, 1, and 2 to have to worry about closing
	// our log FD or anything like that
	//
	openlog("gidd", LOG_ODELAY, LOG_DAEMON);

	// attach FDs 0, 1, and 2 to /dev/null
	//
	int fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		log_fatal("couldn't open /dev/null: %s", strerror(errno));
	}
	for (int i = 0; i <= 2; i++) {
		if (dup2(fd, i) == -1) {
			log_fatal("couldn't dup FD %d to /dev/null", i);
		}
	}
	if (fd > 2) {
		close(fd);
	}
	
	// fork and have the parent exit, while continuing in the child.
	// this:
	//   - makes our parent (most likely the shell) think we're done
	//   - makes sure we're not a process group leader, so that setsid()
	//     can succeed
	//
	pid_t pid = fork();
	if (pid == -1) {
		log_fatal("couldn't fork into background: %s",
		          strerror(errno));
	}
	if (pid != 0) {
		exit(0);
	}

	// become a session leader so that we no longer have a controlling
	// terminal
	//
	if (setsid() == -1) {
		log_fatal("setsid failed: %s", strerror(errno));
	}

	// clear any bits set in our umask
	//
	umask(0);

	// chdir to / so we don't prevent file systems from being unmounted
	//
	if (chdir("/") == -1) {
		log_fatal("couldn't chdir to /: %s", strerror(errno));
	}
}

static void
alloc(int fd)
{
	uid_t uid;
	if (fullio_read(fd, &uid, sizeof(uid_t)) == -1) {
		log_warning("error reading UID for ALLOC command: %s",
		            err_str);
		return;
	}
	gid_t gid = gidpool_alloc(uid);
	if (fullio_write(fd, &gid, sizeof(gid_t)) == -1) {
		log_warning("error writing GID for ALLOC command: %s",
		            err_str);
		return;
	}
	close(fd);
	if (gid != 0) {
		log_info("allocated GID %u to UID %u",
		         (unsigned)gid,
		         (unsigned)uid);
	}
	else {
		log_info("no GID available for UID %u", (unsigned)uid);
	}
}

static void
test(int fd)
{
	uid_t uid;
	if (fullio_read(fd, &uid, sizeof(uid_t)) == -1) {
		log_warning("error reading UID for TEST command: %s",
		            err_str);
		return;
	}
	int ngids;
	if (fullio_read(fd, &ngids, sizeof(int)) == -1) {
		log_warning("error reading GID count for TEST command: %s",
		            err_str);
		return;
	}
	if (ngids <= 0) {
		log_warning("invalid GID count given for TEST command: %d",
		            ngids);
		close(fd);
		return;
	}
	gid_t* gids = malloc(ngids * sizeof(gid_t));
	if (gids == NULL) {
		log_fatal("malloc failure handling TEST command: %s",
		          strerror(errno));
	}
	if (fullio_read(fd, gids, ngids * sizeof(gid_t)) == -1) {
		log_warning("error reading GID list for TEST command: %s",
		            err_str);
		free(gids);
		return;
	}
	int ret = gidpool_test(uid, gids, ngids);
	free(gids);
	if (fullio_write(fd, &ret, sizeof(int)) == -1) {
		log_warning("error writing response for TEST command: %s",
		            err_str);
		return;
	}
	close(fd);
}

static void
waitloop(void)
{
	while (1) {
		int fd = uds_accept();
		if (fd == -1) {
			log_fatal(err_str);
		}
		uid_t uid;
		if (uds_authenticate(fd, &uid) == -1) {
			log_warning("client failed to authenticate: %s",
			            err_str);
			continue;
		}
		if (uid != 0) {
			log_warning("connection attempted by UID %u",
			            (unsigned)uid);
			close(fd);
			continue;
		}
		int cmd;
		if (fullio_read(fd, &cmd, sizeof(int)) == -1) {
			log_warning(err_str);
			continue;
		}
		switch (cmd) {
			case GIDD_ALLOC:
				alloc(fd);
				break;
			case GIDD_TEST:
				test(fd);
				break;
			default:
				log_warning("unknown command: %d", cmd);
				break;
		}
	}
}

int
main(int argc, char* argv[])
{
	daemonize();
	if (gidpool_init(701, 10) == 1) {
		log_fatal(err_str);
	}
	if (uds_init(GIDD_SOCKET) == -1) {
		log_fatal(err_str);
	}
	waitloop();
}
