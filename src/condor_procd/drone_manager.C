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
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_arglist.h"
#include "basename.h"
#include "drone_manager.h"
#include "drone.h"

template class HashTable<int, DroneManager::DroneEntry*>;

static unsigned
int_hash(const int& i)
{
	return i;
}

DroneManager::DroneManager() :
	m_drone_table(int_hash)
{
	MyString drone_binary;
	char* tmp = param("PROCD_TEST_DRONE");
	ASSERT(tmp != NULL);
	drone_binary = tmp;
	free(tmp);

	int pipe_handles[2];
	if (daemonCore->Create_Pipe(pipe_handles) == FALSE) {
		EXCEPT("Create_Pipe error");
	}
	int std_fds[3] = {-1, pipe_handles[1], -1};

	ArgList args;
	args.AppendArg(condor_basename(drone_binary.Value()));
	args.AppendArg("-f");
	FamilyInfo fi;
	int root_drone_pid = daemonCore->Create_Process(drone_binary.Value(),
	                                                args,
	                                                PRIV_CONDOR,
	                                                1,
	                                                FALSE,
	                                                NULL,
	                                                NULL,
	                                                &fi,
	                                                NULL,
	                                                std_fds);
	if (root_drone_pid == FALSE) {
		EXCEPT("failed to create first drone");
	}
	if (daemonCore->Close_Pipe(pipe_handles[1]) == FALSE) {
		EXCEPT("Close_Pipe error");
	}

	MyString root_drone_sinful;
	while (true) {
		char buf[2];
		int bytes = daemonCore->Read_Pipe(pipe_handles[0], buf, sizeof(buf));
		if (bytes == -1) {
			EXCEPT("Read_Pipe error");
		}
		if (bytes == 0) {
			if (daemonCore->Close_Pipe(pipe_handles[0]) == FALSE) {
				EXCEPT("Close_Pipe error");
			}
			break;
		}
		root_drone_sinful += buf;
	}
	DroneEntry* de = new DroneEntry(root_drone_pid, root_drone_sinful.Value());
	int ret = m_drone_table.insert(1, de);
	ASSERT(ret != -1);
}

DroneManager::~DroneManager()
{
	int id;
	DroneEntry* de;
	m_drone_table.startIterations();
	while (m_drone_table.iterate(id, de)) {
		kill_drone(id);
	}
}

pid_t
DroneManager::create_drone(int parent_id, int child_id, bool registered)
{
	int result;

	DroneEntry* parent_de;
	result = m_drone_table.lookup(parent_id, parent_de);
	ASSERT(result != -1);

	Daemon daemon(DT_ANY, parent_de->m_sinful.Value());
	Sock* sock = daemon.startCommand(PROCD_TEST_CREATE_DRONE);
	ASSERT(sock != NULL);

	int should_register = registered ? TRUE : FALSE;
	result = sock->code(should_register);
	ASSERT(result != FALSE);

	sock->decode();

	int new_drone_pid;
	result = sock->code(new_drone_pid);
	ASSERT(result != FALSE);

	char* new_drone_sinful = NULL;
	result = sock->code(new_drone_sinful);
	ASSERT(result != FALSE);

	delete sock;

	DroneEntry* child_de = new DroneEntry(new_drone_pid, new_drone_sinful);
	ASSERT(child_de != NULL);
	result = m_drone_table.insert(child_id, child_de);
	ASSERT(result != -1);

	return new_drone_pid;
}

pid_t
DroneManager::kill_drone(int node_id)
{
	int result;

	DroneEntry* de;
	result = m_drone_table.lookup(node_id, de);
	assert(result != -1);

	Daemon daemon(DT_ANY, de->m_sinful.Value());
	Sock* sock = daemon.startCommand(PROCD_TEST_KILL_DRONE);
	ASSERT(sock != NULL);

	result = sock->end_of_message();
	ASSERT(result != FALSE);

	delete sock;

	result = m_drone_table.remove(node_id);
	assert(result != -1);

	pid_t pid = de->m_pid;
	delete de;

	return pid;
}

pid_t
DroneManager::get_drone_pid(int id)
{
	DroneEntry* de;
	int result = m_drone_table.lookup(id, de);
	ASSERT(result != -1);
	return de->m_pid;
}
