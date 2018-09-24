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

#include "classad/classad_stl.h"
#include "file_transfer.h"
#include "classad_log.h"
#include "cached_cron_job_mgr.h"

class CondorError;

namespace compat_classad {
	class ClassAd;
}

class CachedServer: Service {

friend class UploadFilesHandler;

 public:
    CachedServer();
    ~CachedServer();

    void InitAndReconfig();
		void InitializeBittorrent();


 private:
	
	enum CACHE_STATE {
		INVALID,
		UNCOMMITTED,
		UPLOADING,
		COMMITTED
	};

	int dummy_reaper(Service *, int pid, int);

		// CMD API's
	int CreateCacheDir(int cmd, Stream *sock);
	int CreateCacheDir2(int cmd, Stream *sock);
	int UploadToServer(int cmd, Stream *sock);
	int DownloadFiles(int cmd, Stream *sock);
	int DownloadFiles2(int cmd, Stream *sock);
	int UploadFiles2(int cmd, Stream *sock);
	int RemoveCacheDir(int cmd, Stream *sock);
	int UpdateLease(int cmd, Stream *sock);
	int ListCacheDirs(int cmd, Stream *sock);
	int ListCacheDs(int cmd, Stream *sock);
	int ListFilesByPath(int cmd, Stream *sock);
	int CheckConsistency(int cmd, Stream *sock);
	int SetReplicationPolicy(int cmd, Stream *sock);
	int GetReplicationPolicy(int cmd, Stream *sock);
	int ReceiveCacheAdvertisement(int  cmd, Stream *sock);
	int ReceiveRedundancyAdvertisement(int  cmd, Stream *sock);
	int ReceiveLocalReplicationRequest(int cmd, Stream *sock);
	int ReceiveLocalReplicationRequest2(int cmd, Stream *sock);
	int DoEncodeDir(int cmd, Stream *sock);	
	int DoEncodeFile(int cmd, Stream *sock);
	int DoDecodeFile(int cmd, Stream *sock);
	int ReceiveDistributeReplicas(int cmd, Stream* sock);
	int ReceiveDistributeEncodedFiles(int cmd, Stream* sock);

	/* 
		When a server believes a replica should be stored on this server, they will
		call this command on the server.  It will verify that the cache can be
		stored on this machine, and then call the download function to properly.
	*/
	int CreateReplica(int cmd, Stream *sock);

		// Cache interaction
	int GetCacheAd(const std::string &, compat_classad::ClassAd *&, CondorError &);
	int CreateCacheAd(std::string &, CondorError &);
	int SetCacheUploadStatus(const std::string &, CACHE_STATE state);
	int CleanCache();
	std::string GetCacheDir(const std::string &dirname, CondorError &err);
	CACHE_STATE GetUploadStatus(const std::string &dirname);
	int DoRemoveCacheDir(const std::string &dirname, CondorError &err);


		// DB manipulation
	int InitializeDB();
	int RebuildDB();

	// Timer callback
	void CheckActiveTransfers();
	void AdvertiseCaches();
	void AdvertiseRedundancy();
	void AdvertiseCacheDaemon();
	void HandleTorrentAlerts();
	void CheckReplicationRequests();
	void PruneBadParents();
	
	compat_classad::ClassAd GenerateClassAd();
	filesize_t CalculateCacheSize(std::string cache_name);
	int SetLogCacheSize(std::string cache_name, filesize_t size);
	int CreateCacheDirectory(const std::string &cache_name, CondorError &err);
	int SetTorrentLink(std::string cache_name, std::string magnet_link);
	std::list<compat_classad::ClassAd> QueryCacheLog(const std::string& requirement);
	std::string ConvertIdtoDirname(const std::string cacheId);
	int CheckCacheReplicationStatus(std::string cache_name, std::string cached_origin);
	bool NegotiateCache(compat_classad::ClassAd cache_ad, compat_classad::ClassAd cached_ad);
	std::string NegotiateTransferMethod(compat_classad::ClassAd cache_ad, std::string my_methods);

	void DistributeEncodedDir(std::string &encode_dir, std::string &cache_name, int encode_data_num, int encode_parity_num);
	void DistributeEncodedFiles(std::string cache_name, std::vector<std::string>& encoded_files);
	int  DistributeReplicas(const std::vector<std::string> cached_servers, std::string& cache_name, time_t& expiry, const std::vector<std::string> transfer_files);
	int CreateRemoteCacheDir(const std::string cached_destination, std::string& cache_name, time_t& expiry, const std::string redundancy_policy, int data_number, int parity_number, std::string redundancy_candidates);
	int UploadFilesToRemoteCache(const std::string cached_destination, const std::string cache_name, const std::vector<std::string> transfer_files);

	/**
		* Find the parent cache for this cache.  It checks first to find the parent
		* on this localhost.  Then, if it is the parent on the node, finds the parent
		* on the cluster (if it exists).  Then, if this daemon does not have a parent, 
		* returns itself.
		*
		* parent: parent classad
		* returns: 1 if found parent, 0 if my own parent
		*
		*/
	int FindParentCache(counted_ptr<compat_classad::ClassAd> &parent);
	
	//int DoDirectDownload(compat_classad::ClassAd cache_ad, compat_classad::ClassAd cached_ad);
	int DoDirectDownload(std::string cache_source, compat_classad::ClassAd cache_ad);
	int DoDirectDownload2(std::string cache_source, compat_classad::ClassAd cache_ad);
	int DoDirectUpload2(std::string cache_source, compat_classad::ClassAd cache_ad);
	
	int DoBittorrentDownload(compat_classad::ClassAd& cache_ad, bool initial_download = true);
	
	int DoHardlinkTransfer(ReliSock* rsock, std::string cache_name);
	
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
	std::list<FileTransfer*> m_active_transfers;
	int m_active_transfer_timer;
	int m_advertise_caches_timer;
	int m_advertise_redundancy_timer;
	int m_advertise_cache_daemon_timer;
	int m_torrent_alert_timer;
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
	typedef classad_unordered<std::string, string_to_time*> cache_to_unordered;
	cache_to_unordered cache_host_map;
	cache_to_unordered redundancy_host_map;
	
	// A mapping of the requested caches URL to the status classad
	classad_unordered<std::string, compat_classad::ClassAd> m_requested_caches;
	
	// Bad parents that we have attempted to connect, but have failed
	classad_unordered<std::string, time_t> m_failed_parents;
	
	// Cron manager
	CachedCronJobMgr cron_job_mgr;
	
};





#endif
