/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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

#ifndef _QUERY_HANDLER_H
#define _QUERY_HANDLER_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "Scheduler.h"

#include <vector>

class ClassAdCollection;

/*
 * The QueryHandler is responsible for trailing a schedd's queue log,
 * keeping an in-memory representation, and responding to user queries.
 */

class QueryHandler: public Service {
 public:
	QueryHandler();
	virtual ~QueryHandler() {}

	void config();
	void init();
	void stop();

	ClassAdCollection * GetClassAds() {return m_scheduler->GetClassAds();}
 private:

	int m_periodic_timer_id;
	Scheduler *m_scheduler;
	ReliSock *m_register_sock;
	CondorError *m_register_errstack;
	classad::ClassAd m_register_ad;

	int handle(int, Stream* stream);
	int poll();

	void RegisterStart(bool success);
	static void RegisterStartCallback(bool success, Sock * sock, CondorError * /*errstack*/, void * misc_data)
	{
		QueryHandler *myself = static_cast<QueryHandler *>(misc_data);
		myself->m_register_sock = static_cast<ReliSock*>(sock);
		myself->RegisterStart(success);
	}
};

#endif
