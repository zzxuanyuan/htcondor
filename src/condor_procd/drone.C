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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "drone.h"

char* mySubSystem = "PROCD_TEST_DRONE";

static int reaper_id;

int
reaper(Service*, int pid, int status)
{
	dprintf(D_ALWAYS, "pid %d exited with status %d\n", pid, status);
	return TRUE;
}

int
handle_spawn(Service*, int, Stream* stream)
{
	int result;
	char* tmp;

	int should_register;
	result = stream->code(should_register);
	ASSERT(result != FALSE);

	result = stream->end_of_message();
	ASSERT(result != FALSE);

	tmp = param("PROCD_TEST_DRONE");
	ASSERT(tmp != NULL);
	MyString drone_binary = tmp;
	free(tmp);

	ArgList args;
	args.AppendArg(drone_binary.Value());
	args.AppendArg("-f");

	FamilyInfo* fi = NULL;
	if (should_register == TRUE) {
		fi = new FamilyInfo;
	}

	int pipe_handles[2];
	if (daemonCore->Create_Pipe(pipe_handles) == FALSE) {
		EXCEPT("Create_Pipe error");
	}
	int std_fds[3] = {-1, pipe_handles[1], -1};

	int child_pid = daemonCore->Create_Process(drone_binary.Value(),
	                                           args,
	                                           PRIV_CONDOR,
	                                           reaper_id,
	                                           FALSE,
	                                           NULL,
	                                           NULL,
	                                           fi,
	                                           NULL,
	                                           std_fds);
	ASSERT(child_pid != FALSE);

	if (fi != NULL) {
		delete fi;
	}

	if (daemonCore->Close_Pipe(pipe_handles[1]) == FALSE) {
		EXCEPT("Close_Pipe error");
	}

	stream->encode();

	result = stream->code(child_pid);
	ASSERT(result != FALSE);


	MyString child_sinful;
	while (true) {
		char buf[33];
		int bytes = daemonCore->Read_Pipe(pipe_handles[0], buf, sizeof(buf) - 1);
		if (bytes == -1) {
			EXCEPT("Read_Pipe error");
		}
		if (bytes == 0) {
			if (daemonCore->Close_Pipe(pipe_handles[0]) == FALSE) {
				EXCEPT("Close_Pipe error");
			}
			break;
		}
		buf[bytes] = '\0';
		child_sinful += buf;
	}
	tmp = const_cast<char*>(child_sinful.Value());
	result = stream->code(tmp);
	ASSERT(result != FALSE);

	return TRUE;
}

int
handle_die(Service*, int, Stream* stream)
{
	int result = stream->end_of_message();
	ASSERT(result != FALSE);
	DC_Exit(0);
	return TRUE;
}

int
main_init(int, char *[])
{
	dprintf(D_ALWAYS, "main_init() called\n");

	int result;

	char* sinful = daemonCore->InfoCommandSinfulString();
	ASSERT(sinful != NULL);
	printf("%s", sinful);
	fclose(stdout);

	reaper_id = daemonCore->Register_Reaper("reaper",
	                                        reaper,
	                                        "reaper");
	ASSERT(result != FALSE);

	result = daemonCore->Register_Command(PROCD_TEST_CREATE_DRONE,
	                                      "PROCD_TEST_CREATE_DRONE",
	                                      handle_spawn,
	                                      "handle_spawn");
	ASSERT(result != -1);

	result = daemonCore->Register_Command(PROCD_TEST_KILL_DRONE,
	                                      "PROCD_TEST_KILL_DRONE",
	                                      handle_die,
	                                      "handle_die");
	ASSERT(result != -1);

	return TRUE;
}

int 
main_config( bool )
{
	dprintf(D_ALWAYS, "main_config() called\n");
	return TRUE;
}

int
main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

void
main_pre_dc_init( int, char*[] )
{
		// dprintf isn't safe yet...
}

void
main_pre_command_sock_init( )
{
}

