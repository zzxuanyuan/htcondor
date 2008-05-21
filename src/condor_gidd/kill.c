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

// this tool takes two arguments: a PID and a signal number to send to the
// given PID. this tool is meant to be root-owned with the setuid bit set.
// it must only allow the signal to be sent if either:
//   - the calling user owns the given process
//   - the calling user otherwise has been granted permission to send signals
//     to the given process. initially, this means that the user has checked
//     out a GID for tracking purposes that appears in the supplementary
//     group list of the process

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int
main(int argc, char* argv[])
{
	// confirm our assertion that we are invoked as a root-owned binary
	// with the setuid bit set
	//
	if (geteuid() != 0) {
		fprintf(stderr,
		        "error: EUID not 0; "
		        	"procd_kill must be installed setuid root\n");
		return 1;
	}

	// make sure we have the proper number of arguments and that they
	// are reasonably well-formed
	//
	if (argc != 3) {
		fprintf(stderr, "usage: procd_kill <pid> <signo>\n");
		return 1;
	}

	pid_t pid = (pid_t)strtoul(argv[1], NULL, 10);
	if (pid == 0) {
		fprintf(stderr,
		        "error: PID %s is invalid\n",
		        argv[1]);
		return 1;
	}
	int signo = (int)strtoul(argv[2], NULL, 10);
	if (signo == 0) {
		fprintf(stderr,
		        "error: signal number %s is invalid\n",
		        argv[2]);
	}

	// first, attempt to send the signal as the calling UID
	//
	uid_t uid = getuid();
	if (setuid(0) == -1) {
		fprintf(stderr,
		        "error: couldn't setuid to root: %s\n",
		        strerror(errno));
		return 1;
	}
	if (seteuid(uid)
}
