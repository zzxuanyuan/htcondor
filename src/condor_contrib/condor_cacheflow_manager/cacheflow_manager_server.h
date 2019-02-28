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
#include <unordered_map>
#include <list>

class CondorError;

namespace compat_classad {
	class ClassAd;
}

// Cacheflow Manager needs to know the following information for each CacheD. Those information can be collected from Collector or Storage Optimizer or both.
struct CMCachedInfo {
	std::string cached_name;
	double failure_rate;
	long long total_disk_space;
};

class CacheflowManagerServer: Service {

	public:
		CacheflowManagerServer();
		virtual ~CacheflowManagerServer();

		void Init();
		void InitAndReconfig(){}
		void UpdateCollector();
		int  Ping();
		int  GetStoragePolicy(int /*cmd*/, Stream * sock);

		compat_classad::ClassAd GenerateClassAd();
		compat_classad::ClassAd NegotiateStoragePolicy(compat_classad::ClassAd& jobAd);
		int GetCachedInfo(compat_classad::ClassAd& jobAd);
		compat_classad::ClassAd SortedReplication(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint);
		compat_classad::ClassAd RandomReplication(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint);
		compat_classad::ClassAd SortedErasureCoding(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint);
		compat_classad::ClassAd RandomErasureCoding(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint);
		int dummy_reaper(Service *, int pid, int);
//		void CreateDummyCacheDs(DISTRIBUTION_TYPE, int n = 1000);

	private:
		int m_update_collector_timer;
		int m_reaper;
		std::unordered_map<std::string, std::list<CMCachedInfo>::iterator> m_cached_info_map;
		std::list<struct CMCachedInfo> m_cached_info_list;
};

#endif
