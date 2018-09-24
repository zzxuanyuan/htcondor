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

#ifndef __STORAGE_OPTIMIZER_SERVER_H__
#define __STORAGE_OPTIMIZER_SERVER_H__

#include "classad/classad_stl.h"
#include "file_transfer.h"
#include "probability_function.h"

class CondorError;

namespace compat_classad {
	class ClassAd;
}

// Storage Optimizer needs to know the following information for each CacheD. Those information can be collected from Collector or Storage Optimizer or both.
struct SOCachedInfo {
	std::string cached_name;
	class ProbabilityFunction probability_function;
	long long total_disk_space;
};

class StorageOptimizerServer: Service {

 public:
    StorageOptimizerServer();
    ~StorageOptimizerServer();

    void InitAndReconfig(){}
    void UpdateCollector();

    compat_classad::ClassAd GenerateClassAd();
    int dummy_reaper(Service *, int pid, int);
    void GetRuntimePdf();
    int GetCachedInfo(int /*cmd*/, Stream * sock);
    int ListStorageOptimizers(int /*cmd*/, Stream *sock);

 private:
    int m_update_collector_tid;
    int m_reaper_tid;
    std::unordered_map<std::string, std::list<SOCachedInfo>::iterator> m_cached_info_map;
    std::list<struct SOCachedInfo> m_cached_info_list;
};

#endif
