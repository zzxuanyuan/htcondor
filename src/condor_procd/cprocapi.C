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

#include "cprocapi.h"
#include "procapi.h"

extern "C" const size_t cprocapi_size = sizeof(procInfo);

extern "C" void*
cprocapi_first(void)
{
	procInfo* pi = ProcAPI::getProcInfoList();
	return reinterpret_cast<void*>(pi);
}

extern "C" void*
cprocapi_next(void* blob)
{
	procInfo* curr = reinterpret_cast<procInfo*>(blob);
	procInfo* next = curr->next;
	delete curr;
	return reinterpret_cast<void*>(next);
}

extern "C" pid_t
cprocapi_pid(void* blob)
{
	procInfo* pi = reinterpret_cast<procInfo*>(blob);
	return pi->pid;
}
