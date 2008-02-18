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
#include "HookClientMgr.h"
#include "HookClient.h"
#include "status_string.h"


HookClientMgr::HookClientMgr() {
	m_reaper_id = -1;
	m_reaper_ignore_id = -1;
}


HookClientMgr::~HookClientMgr() {
	HookClient *client;	
	m_client_list.Rewind();
	while (m_client_list.Next(client)) {
			// TODO: kill them, too?
		m_client_list.DeleteCurrent();
		delete client;
	}
	if (m_reaper_id != -1) {
		daemonCore->Cancel_Reaper(m_reaper_id);
	}
	if (m_reaper_ignore_id != -1) {
		daemonCore->Cancel_Reaper(m_reaper_ignore_id);
	}
}


bool
HookClientMgr::initialize() {
	m_reaper_id = daemonCore->
		Register_Reaper("HookClientMgr Output Reaper",
						(ReaperHandlercpp) &HookClientMgr::reaper,
						"HookClientMgr Output Reaper", this);
	m_reaper_ignore_id = daemonCore->
		Register_Reaper("HookClientMgr Ignore Reaper",
						(ReaperHandlercpp) &HookClientMgr::reaperIgnore,
						"HookClientMgr Ignore Reaper", this);

	return (m_reaper_id != FALSE && m_reaper_ignore_id != FALSE);
}


bool
HookClientMgr::spawn(HookClient* client, ArgList args, MyString *hook_stdin) {
	if (!client->spawn(args, hook_stdin, m_reaper_id)) {
		dprintf(D_ALWAYS|D_FAILURE, "ERROR: Failed to spawn hook client\n");
		return false;
	}
	m_client_list.Append(client);
	return true;
}


bool
HookClientMgr::remove(HookClient* client) {
    return m_client_list.Delete(client);
}


int
HookClientMgr::reaper(int exit_pid, int exit_status)
{
	bool found_it = false;
	HookClient *client;	
	m_client_list.Rewind();
	while (m_client_list.Next(client)) {
		if (exit_pid == client->getPid()) {
			found_it = true;
			break;
		}
	}

	if (!found_it) {
			// Uhh... now what?
		dprintf(D_ALWAYS|D_FAILURE, "Unexpected: HookClientMgr::reaper() "
				"called with pid %d but no HookClient found that matches.\n",
				exit_pid);
		return FALSE;
	}
	client->hookExited(exit_status);
	return TRUE;
}


int
HookClientMgr::reaperIgnore(int exit_pid, int exit_status)
{
		// Some hook that we don't care about the output for just
		// exited.  All we need is to print a log message (if that).
	MyString status_txt;
	status_txt.sprintf("Hook (pid %d) ", exit_pid);
	statusString(exit_status, status_txt);
	dprintf(D_FULLDEBUG, "%s\n", status_txt.Value());
	return TRUE;
}
