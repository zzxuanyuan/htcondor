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

#ifndef __CACHED_SERVER_H__
#define __CACHED_SERVER_H__

#include <fstream>
#include "classad/classad_stl.h"
#include "file_transfer.h"
#include "classad_log.h"

class CondorError;

namespace compat_classad {
	class ClassAd;
}

class CachedServer: Service {

 public:
    CachedServer();
    ~CachedServer();

    void InitAndReconfig();

 private:
	
	enum CACHE_STATE {
		INVALID,
		UNCOMMITTED,
		UPLOADING,
		COMMITTED,
		OBSOLETE
	};


	// CMD API's
	int UpdateLease(int cmd, Stream *sock);
	int ListCacheDirs(int cmd, Stream *sock);
	int ListCacheDs(int cmd, Stream *sock);
	int GetMostReliableCacheD(int cmd, Stream *sock);
	int ListFilesByPath(int cmd, Stream *sock);
	int CheckConsistency(int cmd, Stream *sock);
	int ReceiveRedundancyAdvertisement(int  cmd, Stream *sock);
	int ProcessTask(int cmd, Stream *sock);
	int ReceiveProcessDataTask(int cmd, Stream* sock);
	int ReceiveProbeCachedServer(int cmd, Stream* sock);
	int ReceiveRequestRedundancy(int cmd, Stream* sock);
	int ReceiveInitializeCache(int cmd, Stream* sock);
	int ReceiveCleanRedundancySource(int cmd, Stream* sock);
	int DownloadRedundancy(int cmd, Stream * sock);
	int ReceiveRequestRecovery(int cmd, Stream* sock);
	int ReceiveUpdateRecovery(int cmd, Stream* sock);
	int ProbeCachedClient(int cmd, Stream* sock);

	// Cache interaction
	int GetCacheAd(const std::string &, compat_classad::ClassAd *&, CondorError &);
	int CreateCacheAd(const std::string &, CondorError &);
	int SetCacheUploadStatus(const std::string &, CACHE_STATE state);
	int CleanCache();
	std::string GetCacheDir(const std::string &dirname, CondorError &err);
	CACHE_STATE GetUploadStatus(const std::string &dirname);
	int DoRemoveCacheDir(const std::string &dirname, CondorError &err);

	int CheckRedundancyStatus(compat_classad::ClassAd& ad);
	int GetRedundancyAd(const std::string& dirname, compat_classad::ClassAd*& ad);
	int LinkRedundancyDirectory(const std::string& directory_path, const std::string& dirname);
	int CreateRedundancyDirectory(const std::string &dirname);
	int InitializeCache(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad);
	std::string GetTransferRedundancyDirectory(const std::string& dirname);
	std::string GetRedundancyDirectory(const std::string& dirname);
	int EvaluateTask(compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad);
	int DoProcessDataTask(const std::string &cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad);
	int RequestRedundancy(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad);
	int ProbeCachedServer(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad);
	int DistributeRedundancy(compat_classad::ClassAd& ad, compat_classad::ClassAd& return_ad);
	int CommitCache(compat_classad::ClassAd& ad);
	int CleanRedundancySource(compat_classad::ClassAd& ad);
	int NegotiateCacheflowManager(compat_classad::ClassAd& ad, compat_classad::ClassAd& return_ad);
		// DB manipulation
	int InitializeDB();
	int InitializeDB2();
	int RebuildDB();

	// Timer callback
	void AdvertiseRedundancy();
	void AdvertiseCacheDaemon();
	void CheckRedundancyCacheds();
	
	compat_classad::ClassAd GenerateClassAd();
	filesize_t CalculateCacheSize(std::string cache_name);
	int SetLogCacheSize(std::string cache_name, filesize_t size);
	int CreateCacheDirectory(const std::string &cache_name, CondorError &err);
	int LinkCacheDirectory(const std::string &source_directory, const std::string &destination_directory, CondorError &err);
	std::list<compat_classad::ClassAd> QueryCacheLog(const std::string& requirement);
	std::string ConvertIdtoDirname(const std::string cacheId);
	bool NegotiateCache(compat_classad::ClassAd cache_ad, compat_classad::ClassAd cached_ad);
	std::string NegotiateTransferMethod(compat_classad::ClassAd cache_ad, std::string my_methods);

	// Recovery
	int RecoverCacheRedundancy(compat_classad::ClassAd& cache_ad, std::unordered_map<std::string, std::string>& alive_map);
	int UpdateRecovery(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad);
	int RequestRecovery(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad);

	ClassAd* GetClassAd(const std::string& Key);
	bool DeleteClassAd(const std::string& Key);
	void SetAttributeInt(const std::string& Key, const std::string& AttrName, int AttrValue);
	void SetAttributeFloat(const std::string& Key, const std::string& AttrName, float AttrValue);
	void SetAttributeBool(const std::string& Key, const std::string& AttrName, bool AttrValue);
	void SetAttributeLong(const std::string& Key, const std::string& AttrName, long long AttrValue);
	void SetAttributeDouble(const std::string& Key, const std::string& AttrName, double AttrValue);
	void SetAttributeString(const std::string& Key, const std::string& AttrName, const std::string& AttrValue);
	bool GetAttributeInt(const std::string& Key, const std::string& AttrName, int& AttrValue);
	bool GetAttributeFloat(const std::string& Key, const std::string& AttrName, float& AttrValue);
	bool GetAttributeBool(const std::string& Key, const std::string& AttrName, bool& AttrValue);
	bool GetAttributeLong(const std::string& Key, const std::string& AttrName, long long& AttrValue);
	bool GetAttributeDouble(const std::string& Key, const std::string& AttrName, double& AttrValue);
	bool GetAttributeString(const std::string& Key, const std::string& AttrName, std::string& AttrValue);

	ClassAdLog<std::string, ClassAd*> *m_log;
	const static int m_schema_version;
	long long m_id;
	const static char *m_header_key;
	std::string m_db_fname;
	bool m_registered_handlers;
	int m_advertise_redundancy_timer;
	int m_advertise_cache_daemon_timer;
	int m_check_redundancy_cached_timer;
	int m_replication_check;
	int m_prune_bad_parents_timer;
	std::string m_daemonName;
	
	struct parent_struct {
		bool has_parent;
		bool parent_local;
		counted_ptr<compat_classad::ClassAd> parent_ad;
	};
	
	parent_struct m_parent;
	
	// Boot time
	time_t m_boot_time;
	
	typedef classad_unordered<std::string, time_t>  string_to_time;
	typedef classad_unordered<std::string, counted_ptr<string_to_time>> cache_to_unordered;
	cache_to_unordered cache_host_map;
	cache_to_unordered redundancy_host_map;
	classad_unordered<std::string, time_t> cache_expiry_map;
	std::fstream negotiate_fs;
	std::fstream redundancy_count_fs;
	std::fstream network_perf_fs;
	std::fstream recovery_fs;
	std::fstream redundancy_map_fs;
	std::set<std::string> initialized_set;
	std::set<std::string> finished_set;
	
	// A mapping of the requested caches URL to the status classad
	classad_unordered<std::string, compat_classad::ClassAd> m_requested_caches;
	
	// Bad parents that we have attempted to connect, but have failed
	classad_unordered<std::string, time_t> m_failed_parents;
	
};





#endif
