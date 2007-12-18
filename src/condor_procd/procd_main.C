/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_debug.h"
#include "proc_family_monitor.h"
#include "proc_family_server.h"
#include "proc_family_io.h"

#if defined(WIN32)
#include "process_control.WINDOWS.h"
#endif

// our "local server address"
// (set with the "-A" option)
//
static char* local_server_address = NULL;

// the client prinical who will be allowed to connect to us
// (if not given, only root/SYSTEM will be allowed access).
// this string will be a SID on Windows and a UID on UNIX
//
static char* local_client_principal = NULL;

// the PID of the process to monitor (set with the "-P"
// option). this option is only allowed if the ProcD is
// started by root. otherwise, the default behavior of
// monitoring the parent process must be used
//
static pid_t root_pid = 0;

// log file (no logging by default)
// (set with the "-L" option)
//
static char* log_file_name = NULL;

// the maximum number of seconds we'll wait in between
// taking snapshots (one minute by default)
// (set with the "-S" option)
//
static int max_snapshot_interval = 60;

#if defined(LINUX)
// a range of group IDs that can be used to track process
// families by placing them in their supplementary group
// lists
//
static gid_t min_tracking_gid = 0;
static gid_t max_tracking_gid = 0;
#endif

#if defined(WIN32)
// on Windows, we use an external program (condor_softkill.exe)
// to send soft kills to jobs. the path to this program is passed
// via the -K option
//
static char* windows_softkill_binary = NULL;
#endif

static inline void
fail_illegal_option(char* option)
{
	fprintf(stderr,
	        "error: illegal option: %s",
	        option);
	exit(1);
}

static inline void
fail_option_args(char* option, int args_required)
{
	fprintf(stderr,
	        "error: option \"%s\" requires %d arguments",
	        option,
	        args_required);
	exit(1);
}

static void
parse_command_line(int argc, char* argv[])
{
	int index = 1;
	while (index < argc) {

		// first, make sure the first char of the option is '-'
		// and that there is at least one more char after that
		//
		if (argv[index][0] != '-' || argv[index][1] == '\0') {
			fail_illegal_option(argv[index]);
		}

		// now switch on the option
		//
		switch(argv[index][1]) {

			// DEBUG: stop ourselves so a debugger can
			// attach if "-D" is given
			//
			case 'D':
				sleep(30);
				break;

			// local server address
			//
			case 'A':
				if (index + 1 >= argc) {
					fail_option_args("-A", 1);
				}
				index++;
				local_server_address = argv[index];
				break;

			// local client principal
			//
			case 'C':
				if (index + 1 >= argc) {
					fail_option_args("-C", 1);
				}
				index++;
				local_client_principal = argv[index];
				break;

			// pid to monitor
			//
			case 'P':
				if (index + 1 >= argc) {
					fail_option_args("-P", 1);
				}
				index++;
				root_pid = atoi(argv[index]);
				break;

			// log file name
			//
			case 'L':
				if (index + 1 >= argc) {
					fail_option_args("-L", 1);
				}
				index++;
				log_file_name = argv[index];
				break;

			// maximum snapshot interval
			//
			case 'S':
				if (index + 1 >= argc) {
					fail_option_args("-S", 1);
				}
				index++;
				max_snapshot_interval = atoi(argv[index]);
				break;

#if defined(LINUX)
			// tracking group ID range
			//
			case 'G':
				if (index + 2 >= argc) {
					fail_option_args("-G", 2);
				}
				index++;
				min_tracking_gid = (gid_t)atoi(argv[index]);
				index++;
				max_tracking_gid = (gid_t)atoi(argv[index]);
				break;
#endif

#if defined(WIN32)
			// windows condor_softkill.exe binary path
			//
			case 'K':
				if (index + 1 >= argc) {
					fail_option_args("-K", 1);
				}
				index++;
				windows_softkill_binary = argv[index];
				break;
#endif

			// default case
			//
			default:
				fail_illegal_option(argv[index]);
				break;
		}

		index++;
	}

	// now that we're done parsing, enforce constraints
	//
	if (local_server_address == NULL) {
		fprintf(stderr, "error: the \"-A\" option is required");
		exit(1);
	}
	if ((root_pid != 0) && (getuid() != 0)) {
		fprintf(stderr,
		        "error: only root can specify \"-P\" option\n");
		exit(1);
	}
}

static void
get_root_pid_info(pid_t& pid, birthday_t& birthday)
{
	procInfo* pi = NULL;
	int ret;
	int ignored;

	if (root_pid != 0) {
		ret = ProcAPI::getProcInfo(root_pid, pi, ignored);
		if (ret != PROCAPI_SUCCESS) {
			fprintf(stderr,
			        "error: getProcInfo failed on PID %u\n",
			        root_pid);
			exit(1);
		}	
	}
	else {
		procInfo* own_pi = NULL;
		ret = ProcAPI::getProcInfo(getpid(), own_pi, ignored);
		if (ret != PROCAPI_SUCCESS) {
			fprintf(stderr,
			        "error: getProcInfo failed on own PID");
			exit(1);
		}
		ret = ProcAPI::getProcInfo(own_pi->ppid, pi, ignored);
		if (ret != PROCAPI_SUCCESS) {
			fprintf(stderr,
			        "error: getProcInfo failed on parent PID");
			exit(1);
		}
		if (pi->birthday > own_pi->birthday) {
			fprintf(stderr,
			        "error: parent process's birthday is "
			            "later than our own\n");
			exit(1);
		}
		delete own_pi;
	}

	pid = pi->pid;
	birthday = pi->birthday;

	delete pi;
}

int
main(int argc, char* argv[])
{
	// close stdin and stdout right away, since we don't use them
	//
	fclose(stdin);
	fclose(stdout);

	// this modifies our static configuration variables based on
	// our command line parameters
	//
	parse_command_line(argc, argv);

	// get the PID and birthday of our "root pid" (whose process
	// tree we'll be monitoring)
	//
	birthday_t root_birthday;
	get_root_pid_info(root_pid, root_birthday);

	// setup logging if a file was given
	//
	extern FILE* debug_fp;
	if (log_file_name != NULL) {
		debug_fp = safe_fopen_wrapper(log_file_name, "w");
		if (debug_fp == NULL) {
			fprintf(stderr,
			        "error: couldn't open file \"%s\" for logging: %s (%d)",
					log_file_name,
			        strerror(errno),
			        errno);
			exit(1);
		}
	}

	// if a maximum snapshot interval was given, it needs to be either
	// a non-negative number, or -1 for "infinite"
	//
	if (max_snapshot_interval < -1) {
		fprintf(stderr,
		        "error: maximum snapshot interval must be non-negative or -1");
		exit(1);
	}

#if defined(WIN32)
	// on Windows, we need to tell our "process control" module what binary
	// to use for sending WM_CLOSE messages
	//
	if (windows_softkill_binary != NULL) {
		set_windows_soft_kill_binary(windows_softkill_binary);
	}
#endif

	// initialize the "engine" for tracking process families
	//
	ProcFamilyMonitor monitor(root_pid, root_birthday, max_snapshot_interval);

#if defined(LINUX)
	// if a "-G" option was given, enable group ID tracking in the
	// monitor
	//
	if (min_tracking_gid != 0) {
		if (min_tracking_gid > max_tracking_gid) {
			fprintf(stderr,
			        "invalid group range given: %u - %u\n",
			        min_tracking_gid,
			        max_tracking_gid);
			exit(1);
		}
		monitor.enable_group_tracking(min_tracking_gid, max_tracking_gid);
	}
#endif

	// initialize the server for accepting requests from clients
	//
	ProcFamilyServer server(monitor, local_server_address);

	// specify the client that we'll be accepting connections from. note
	// that passing NULL may have special meaning here: for example on
	// UNIX we'll check to see if we were invoked as a setuid root program
	// and if so use our real UID as the client principal
	//
	server.set_client_principal(local_client_principal);

	// now that we've initialized the server, close out standard error.
	// this way, calling programs can set up a pipe to block on until
	// we're accepting connections
	//
	fclose(stderr);

	// finally, enter the server's wait loop
	//
	server.wait_loop();

	return 0;
}
