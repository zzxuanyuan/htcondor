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

#ifndef _CONDOR_HOOK_CLIENT_MGR_H
#define _CONDOR_HOOK_CLIENT_MGR_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "HookClient.h"

class HookClientMgr : public Service
{
public:
	HookClientMgr();
	~HookClientMgr();

	bool initialize();

	int reaper(int exit_pid, int exit_status);
	bool spawn(HookClient* client, ArgList args, MyString* hook_stdin);
	bool remove(HookClient* client);

private:
	int m_reaper_id;
    SimpleList<HookClient*> m_client_list;

};

#endif /* _CONDOR_HOOK_CLIENT_MGR_H */
