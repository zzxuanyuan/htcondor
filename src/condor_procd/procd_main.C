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
#include "condor_config.h"
#include "condor_distribution.h"
#include "proc_family_monitor.h"
#include "proc_family_server.h"
#include "proc_family_io.h"

#if defined(WIN32)
#include "process_control.WINDOWS.h"
#endif

// our "local server address"
// (set with the "-A" option)
//
static char* local_server_address;

// the client prinical who will be allowed to connect to us
// (if not given, only root/SYSTEM will be allowed access).
// this string will be a SID on Windows and a UID on UNIX
//
static char* local_client_principal;

// the maximum number of seconds we'll wait in between
// taking snapshots (one minute by default)
// (set with the "-S" option)
//
static int max_snapshot_interval;

#if defined(LINUX)
// a range of group IDs that can be used to track process
// families by placing them in their supplementary group
// lists
//
static gid_t min_tracking_gid;
static gid_t max_tracking_gid;
#endif

#if defined(WIN32)
// on Windows, we use an external program (condor_softkill.exe)
// to send soft kills to jobs. the path to this program is passed
// via the -K option
//
static char* windows_softkill_binary;
#endif

static void
get_configuration()
{
	local_server_address = param("PROCD_ADDRESS");
	if (local_server_address == NULL) {
		EXCEPT("PROCD_ADDRESS not defined");
	}

	local_client_principal = param("PROCD_CLIENT_PRINCIPAL");

	max_snapshot_interval = param_integer("PROCD_MAX_SNAPSHOT_INTERVAL", 60);

#if defined(LINUX)
	min_tracking_gid = max_tracking_gid = 0;
	if (param_boolean("USE_GID_PROCESS_TRACKING", false)) {
		min_tracking_gid = param_integer("MIN_TRACKING_GID", 0, 0);
		max_tracking_gid = param_integer("MAX_TRACKING_GID", 0, 0);
		if (min_tracking_gid == 0) {
			EXCEPT("USE_GID_PROCESS_TRACKING is set, but "
			           "MIN_TRACKING_GID is not set or is 0");
		}
		if (max_tracking_gid == 0) {
			EXCEPT("USE_GID_PROCESS_TRACKING is set, but "
			           "MAX_TRACKING_GID is not set or is 0");
		}
	}
#endif

#if defined(WIN32)
	windows_softkill_binary = param("WINDOWS_SOFTKILL");
#endif
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

int
main(int argc, char* argv[])
{
	// do the distro thing: we may be "PROCD" or "CONDOR"
	//
	myDistro->Init(argc, argv);

	// read our configuration file
	//
	config();

	// initialize our logging subsystem
	//
	dprintf_config("PROCD");

	// this modifies our static configuration variables based on
	// values in the config file; the ProcD currently doesn't support
	// reconfiguration
	//
	get_configuration();

	// we take a single optional argument: a PID to monitor. if not
	// given, we default to monitoring our parent (but callers need
	// to be careful - monitoring the parent only makes sense if the
	// "-f" DaemonCore option is given)
	//
	pid_t root_pid = 0;
	if (argc > 1) {
		root_pid = (pid_t)strtoul(argv[1], NULL, 10);
		if (root_pid == 0) {
			EXCEPT("invalid PID given to monitor: %s", argv[1]);
		}
	}

	// determine the PID and birthday of the process we'll be
	// monitoring
	//
	birthday_t root_birthday;
	if (root_pid != 0) {
		procInfo* pi = NULL;
		int ignored;
		int status = ProcAPI::getProcInfo(root_pid, pi, ignored);
		if (status != PROCAPI_SUCCESS) {
			EXCEPT("getProcInfo failed on PID %u",
			       (unsigned)root_pid);
		}
		root_birthday = pi->birthday;
		delete pi;
	}
	else {
		get_parent_info(root_pid, root_birthday);
	}

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
	ProcFamilyMonitor monitor(root_pid,
	                          root_birthday,
	                          max_snapshot_interval);

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
		monitor.enable_group_tracking(min_tracking_gid,
		                              max_tracking_gid);
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

	// enter the server's wait loop
	//
	server.wait_loop();

	return 0;
}
