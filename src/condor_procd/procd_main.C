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


#include "condor_common.h"
#include "condor_debug.h"
#include "sig_install.h"
#include "proc_family_monitor.h"
#include "proc_family_server.h"
#include "proc_family_io.h"

#if defined(WIN32)
#include "process_control.WINDOWS.h"
#endif

// for DaemonCore
//
char* mySubSystem = "PROCD";

// our "local server address"
// (set with the "-A" option)
//
static char* local_server_address = NULL;

// the client prinical who will be allowed to connect to us
// (if not given, only root/SYSTEM will be allowed access).
// this string will be a SID on Windows and a UID on UNIX
//
static char* local_client_principal = NULL;

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
	EXCEPT("illegal option: %s", option);
}

static inline void
fail_option_args(char* option, int args_required)
{
	EXCEPT("option \"%s\" requires %d arguments", option, args_required);
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

	// now that we're done parsing, enforce required options
	//
	if (local_server_address == NULL) {
		EXCEPT("the \"-A\" option is required");
	}
}

static void
get_parent_info(pid_t& parent_pid, birthday_t& parent_birthday)
{
	procInfo* own_pi = NULL;
	procInfo* parent_pi = NULL;

	int ignored;
	int status;
	status = ProcAPI::getProcInfo(getpid(), own_pi, ignored) ;
	if (status != PROCAPI_SUCCESS) {
		EXCEPT("getProcInfo failed on own PID");
	}
	status = ProcAPI::getProcInfo(own_pi->ppid, parent_pi, ignored);
	if (status != PROCAPI_SUCCESS) {
		EXCEPT("getProcInfo failed on parent PID");
	}
	if (parent_pi->birthday > own_pi->birthday) {
		EXCEPT("parent process's birthday is later than our own");
	}

	parent_pid = parent_pi->pid;
	parent_birthday = parent_pi->birthday;

	delete own_pi;
	delete parent_pi;
}

// this is a temporary hack until the ProcD becomes more fully integrated
// with DaemonCore. as it stands, we never return from main_init().
// however, we'd still like to be responsive to SIGQUIT and SIGTERM for
// easier administration. we'll just respond by dying for now
//
static void
fix_signal_handlers()
{
	install_sig_handler(SIGQUIT, SIG_DFL);
	install_sig_handler(SIGTERM, SIG_DFL);
	unblock_signal(SIGQUIT);
	unblock_signal(SIGTERM);
}

int
main_init(int argc, char* argv[])
{
	// invoke hack to allow us to repond (by dying) to SIGQUIT and
	// SIGTERM, even though we currently don't ever return to
	// DaemonCore
	//
	fix_signal_handlers();

	// this modifies our static configuration variables based on
	// our command line parameters
	//
	parse_command_line(argc, argv);

	// get the PID and birthday of our parent (whose process
	// tree we'll be monitoring)
	//
	pid_t parent_pid;
	birthday_t parent_birthday;
	get_parent_info(parent_pid, parent_birthday);

	// if a maximum snapshot interval was given, it needs to be either
	// a non-negative number, or -1 for "infinite"
	//
	if (max_snapshot_interval < -1) {
		EXCEPT("maximum snapshot interval must be non-negative or -1");
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
	ProcFamilyMonitor monitor(parent_pid, parent_birthday, max_snapshot_interval);

#if defined(LINUX)
	// if a "-G" option was given, enable group ID tracking in the
	// monitor
	//
	if (min_tracking_gid != 0) {
		if (min_tracking_gid > max_tracking_gid) {
			EXCEPT("invalid group range given: %u - %u\n",
			       min_tracking_gid,
			       max_tracking_gid);
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

	// TODO: we used to keep stderr open until we were listening for
	// requests on our named pipe, at which point we'd close it. this
	// allowed a parent process to block on a pipe until the ProcD
	// was initialized. now that we're DaemonCore, we really shouldn't
	// be using stderr for this (see Nick's comments in
	// daemon_core_main.C). we should have a general mechanism for this
	// sort of thing; we should use it for the Collector too

	// finally, enter the server's wait loop
	//
	server.wait_loop();

	return 0;
}

int
main_config(bool)
{
	return FALSE;
}

int
main_shutdown_fast()
{
	return FALSE;
}

int
main_shutdown_graceful()
{
	return FALSE;
}

void
main_pre_dc_init(int, char*[])
{
}

void
main_pre_command_sock_init()
{
}
