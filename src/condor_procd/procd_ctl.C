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
#include "proc_family_client.h"

int register_family(ProcFamilyClient& pfc, int argc, char* argv[]);
int get_usage(ProcFamilyClient& pfc, int argc, char* argv[]);
int signal_process(ProcFamilyClient& pfc, int argc, char* argv[]);
int suspend_family(ProcFamilyClient& pfc, int argc, char* argv[]);
int continue_family(ProcFamilyClient& pfc, int argc, char* argv[]);
int kill_family(ProcFamilyClient& pfc, int argc, char* argv[]);
int unregister_family(ProcFamilyClient& pfc, int argc, char* argv[]);
int quit(ProcFamilyClient& pfc, int argc, char* argv[]);

int
main(int argc, char* argv[])
{
	if (argc < 3) {
		fprintf(stderr,
		        "usage: %s <procd_addr> <cmd> [<arg> ...]\n",
		        argv[0]);
		return 1;
	}

	Termlog = 1;
	dprintf_config("TOOL");

	ProcFamilyClient pfc;
	if (!pfc.initialize(argv[1])) {
		fprintf(stderr, "error: failed to initialize ProcD client\n");
		return 1;
	}

	int cmd_argc = argc - 2;
	char** cmd_argv = argv + 2;
	if (strcmp(cmd_argv[0], "REGISTER_FAMILY") == 0) {
		return register_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcmp(cmd_argv[0], "GET_USAGE") == 0) {
		return get_usage(pfc, cmd_argc, cmd_argv);
	}
	else if (strcmp(cmd_argv[0], "SIGNAL_PROCESS") == 0) {
		return signal_process(pfc, cmd_argc, cmd_argv);
	}
	else if (strcmp(cmd_argv[0], "SUSPEND_FAMILY") == 0) {
		return suspend_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcmp(cmd_argv[0], "CONTINUE_FAMILY") == 0) {
		return continue_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcmp(cmd_argv[0], "KILL_FAMILY") == 0) {
		return kill_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcmp(cmd_argv[0], "UNREGISTER_FAMILY") == 0) {
		return unregister_family(pfc, cmd_argc, cmd_argv);
	}
	else if (strcmp(cmd_argv[0], "QUIT") == 0) {
		return quit(pfc, cmd_argc, cmd_argv);
	}
	else {
		fprintf(stderr, "error: invalid command: %s\n", cmd_argv[0]);
		return 1;
	}
}

int register_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 4) {
		fprintf(stderr,
		        "error 3 arguments reguired for %s command: "
		            "<pid> <watcher_pid> <max_snapshot interval>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	pid_t watcher = atoi(argv[2]);
	int max_snapshot_interval = atoi(argv[3]);
	bool success;
	if (!pfc.register_subfamily(pid,
	                            watcher,
	                            max_snapshot_interval,
	                            success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr, "error: %s command failed with ProcD\n", argv[0]);
		return 1;
	}
	return 0;
}

int
get_usage(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr,
		        "error: 1 argument required for %s command: <pid>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	if (pid == 0) {
		fprintf(stderr, "error: invalid pid: %s\n", argv[1]);
		return 1;
	}
	ProcFamilyUsage pfu;
	bool success;
	if (!pfc.get_usage(pid, pfu, success)) {
	fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr, "error: %s command failed with ProcD\n", argv[0]);
		return 1;
	}
	printf("Number of Processes: %d\n", pfu.num_procs);
	printf("User CPU Time (s): %ld\n", pfu.user_cpu_time);
	printf("System CPU Time (s): %ld\n", pfu.sys_cpu_time);
	printf("CPU Percentage (%%): %f\n", pfu.percent_cpu);
	printf("Maximum Image Size (KB): %lu\n", pfu.max_image_size);
	printf("Total Image Size(KB): %lu\n", pfu.total_image_size);
	return 0;
}

int
signal_process(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 3) {
		fprintf(stderr,
		        "error: 2 arguments required for %s command: "
			    "<pid> <signal>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	int signal = atoi(argv[2]);
	bool success;
	if (!pfc.signal_process(pid, signal, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr, "error: %s command failed with ProcD\n", argv[0]);
		return 1;
	}
	return 0;
}

int
suspend_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr,
		        "error: 1 argument required for %s command: <pid>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	bool success;
	if (!pfc.suspend_family(pid, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr, "error: %s command failed with ProcD\n", argv[0]);
		return 1;
	}
	return 0;
}

int
continue_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr,
		        "error: 1 argument required for %s command: <pid>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	bool success;
	if (!pfc.continue_family(pid, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr, "error: %s command failed with ProcD\n", argv[0]);
		return 1;
	}
	return 0;
}

int
kill_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr,
		        "error: 1 argument required for %s command: <pid>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	bool success;
	if (!pfc.kill_family(pid, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr, "error: %s command failed with ProcD\n", argv[0]);
		return 1;
	}
	return 0;
}

int
unregister_family(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr,
		        "error: 1 argument required for %s command: <pid>\n",
		        argv[0]);
		return 1;
	}
	pid_t pid = atoi(argv[1]);
	bool success;
	if (!pfc.unregister_family(pid, success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr, "error: %s command failed with ProcD\n", argv[0]);
		return 1;
	}
	return 0;
}

int
quit(ProcFamilyClient& pfc, int argc, char* argv[])
{
	if (argc != 1) {
		fprintf(stderr,
		        "error: 0 arguments required for %s command: <pid>\n",
		        argv[0]);
		return 1;
	}
	bool success;
	if (!pfc.quit(success)) {
		fprintf(stderr, "error: communication error with ProcD\n");
		return 1;
	}
	if (!success) {
		fprintf(stderr, "error: %s command failed with ProcD\n", argv[0]);
		return 1;
	}
	return 0;
}
