/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#ifndef __CACHEFLOW_MANAGER_SERVER_H__
#define __CACHEFLOW_MANAGER_SERVER_H__

#include "classad/classad_stl.h"
#include "file_transfer.h"
#include "classad_hashtable.h"

class CondorError;
template <typename K, typename AltK, typename AD> class ClassAdLog;
template <typename K, typename AltK, typename AD> class filter_iterator;

namespace compat_classad {
	class ClassAd;
}

class CacheflowManagerServer: Service {

	public:
		CacheflowManagerServer();
		virtual ~CacheflowManagerServer();

		void Init();
		void InitAndReconfig(){}
		void UpdateCollector();
		compat_classad::ClassAd GenerateClassAd();
		int dummy_reaper(Service *, int pid, int);

	private:
		int update_collector_tid;
		int reaper_tid;

};





#endif