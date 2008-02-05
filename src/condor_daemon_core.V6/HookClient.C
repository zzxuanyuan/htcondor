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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "HookClient.h"
#include "status_string.h"


HookClient::HookClient(const char* hook_path) {
	m_hook_path = strdup(hook_path);
	m_pid = -1;
	m_has_exited = false;
}


HookClient::~HookClient() {
	if (m_hook_path) {
		free(m_hook_path);
		m_hook_path = NULL;
	}
	if (m_pid != -1 && !m_has_exited) {
			// TODO
			// kill -9 m_pid
	}
}


bool
HookClient::spawn(ArgList args, MyString* hook_stdin, int reaper_id) {
    int std_fds[3];
    if (hook_stdin && hook_stdin->Length()) {
		std_fds[0] = DC_STD_FD_PIPE;
	}
	std_fds[1] = DC_STD_FD_PIPE;
	std_fds[2] = DC_STD_FD_PIPE;

	m_pid = daemonCore->
		Create_Process(m_hook_path, args, PRIV_CONDOR, reaper_id,
					   FALSE, NULL, NULL, NULL, NULL, std_fds);
	if (m_pid == FALSE) {
		dprintf( D_ALWAYS, "ERROR: Create_Process failed in HookClient::spawn()!\n");
		m_pid = 0;
		return false;
	}

		// If we've got initial input to write to stdin, do so now.
    if (hook_stdin && hook_stdin->Length()) {
		daemonCore->Write_Stdin_Pipe(m_pid, hook_stdin->Value(),
									 hook_stdin->Length());
		daemonCore->Close_Stdin_Pipe(m_pid);
	}

	return true;
}


MyString*
HookClient::getStdOut() {
	if (m_has_exited) {
		return &m_std_out;
	}
	return daemonCore->Read_Std_Pipe(m_pid, 1);
}


MyString*
HookClient::getStdErr() {
	if (m_has_exited) {
		return &m_std_err;
	}
	return daemonCore->Read_Std_Pipe(m_pid, 2);
}


void
HookClient::hookExited(int exit_status) {
	m_has_exited = true;

	MyString status_txt;
	status_txt.sprintf("HookClient %s (pid %d) ", m_hook_path, m_pid);
	statusString(exit_status, status_txt);
	dprintf(D_FULLDEBUG, "%s\n", status_txt.Value());

	MyString* std_out = daemonCore->Read_Std_Pipe(m_pid, 1);
	if (std_out) {
		m_std_out = *std_out;
	}
	MyString* std_err = daemonCore->Read_Std_Pipe(m_pid, 2);
	if (std_err) {
		m_std_err = *std_err;
	}
}
