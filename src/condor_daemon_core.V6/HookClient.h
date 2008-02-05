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

#ifndef _CONDOR_HOOK_CLIENT_H
#define _CONDOR_HOOK_CLIENT_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

class HookClient : public Service
{
public:
	HookClient(const char* hook_path);
	virtual ~HookClient();
	bool spawn(ArgList args, MyString* hook_stdin, int reaper_id);

		// Functions to retrieve data about this client.
	int getPid() {return m_pid;};
	MyString* getStdOut();
	MyString* getStdErr();

		/**
		   Called when this hook client has actually exited.
		*/
	virtual void hookExited(int exit_status);

protected:
	char* m_hook_path;
	int m_pid;
	MyString m_std_out;
	MyString m_std_err;
	bool m_has_exited;
};


#endif /* _CONDOR_HOOK_CLIENT_H */
