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
#include "condor_fix_iostream.h"
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "setenv.h"
#include "proc_family_client.h"
#include "drone_manager.h"
#include "proc_family_state.h"

#include <iostream>
#include <fstream>
#include <string>
using namespace std;

char* mySubSystem = "PROCD_TEST_CONTROLLER";

int
main_init(int, char*[])
{
	char* tmp = param("PROCD_TEST_INPUT");
	if (tmp == NULL) {
		EXCEPT("PROCD_TEST_INPUT not defined");
	}
	ifstream in(tmp);
	if (!in) {
		EXCEPT("error opening input file %s", tmp);
	}
	free(tmp);

	daemonCore->Proc_Family_Init();	
	const char* procd_address = GetEnv("CONDOR_PROCD_ADDRESS");
	ASSERT(procd_address != NULL);
	ProcFamilyClient proc_family_client;
	if (!proc_family_client.initialize(procd_address)) {
		EXCEPT("error initializing ProcFamilyClient");
	}

	DroneManager drone_manager;

	pid_t initial_drone_pid = drone_manager.get_drone_pid(1);

	ProcFamilyState reference_state(initial_drone_pid, getpid());

	while (true) {

		dprintf(D_ALWAYS, "reference state:\n");
		reference_state.display();
		LocalClient* client = proc_family_client.dump(initial_drone_pid);
		ASSERT(client != NULL);
		ProcFamilyState actual_state(client);
		dprintf(D_ALWAYS, "procd state:\n");
		actual_state.display();

		string cmd;
		in >> cmd;
		if (!in) {
			break;
		}

		if (cmd == "SPAWN") {
		
			int parent_id, child_id;
			char registered;
			
			in >> parent_id >> child_id >> registered;
			registered = tolower(registered);
			if (!in) {
				EXCEPT("input error handling SPAWN\n");
			}
		}

		else if (cmd == "DIE") {

			int node_id;

			in >> node_id;
			if (!in) {
				EXCEPT("input error handling DIE\n");
			}
		}

		else {
			EXCEPT("unknown command: %s\n", cmd.c_str());
		}
	}

	return 0;
}

int
main_pre_dc_init(int, char*[])
{
	return 0;
}

int
main_pre_command_sock_init()
{
	return 0;
}

int
main_config(bool)
{
	ASSERT(0);
	return 0;
}

int
main_shutdown_graceful()
{
	ASSERT(0);
	return 0;
}

int
main_shutdown_fast()
{
	ASSERT(0);
	return 0;
	return 0;
}
