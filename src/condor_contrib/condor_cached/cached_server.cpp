#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "cached_server.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "get_daemon_name.h"
#include "ipv6_hostname.h"
#include <list>
#include <map>
#include "basename.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include "directory.h"

//#include "cached_torrent.h"
#include "cached_ec.h"
#include "dc_cached.h"

#include <sstream>

namespace fs = ::boost::filesystem;

#define SCHEMA_VERSION 1

const int CachedServer::m_schema_version(SCHEMA_VERSION);
const char *CachedServer::m_header_key("CACHE_ID");

static int dummy_reaper(Service *, int pid, int) {
	dprintf(D_ALWAYS,"dummy-reaper: pid %d exited; ignored\n",pid);
	return TRUE;
}

CachedServer::CachedServer():
	m_registered_handlers(false)
{
	m_boot_time = time(NULL);
	if ( !m_registered_handlers )
	{
		m_registered_handlers = true;

		// Register the commands
		int rc = daemonCore->Register_Command(
			CACHED_CREATE_CACHE_DIR2,
			"CACHED_CREATE_CACHE_DIR2",
			(CommandHandlercpp)&CachedServer::CreateCacheDir2,
			"CachedServer::CreateCacheDir2",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_CREATE_CACHE_DIR,
			"CACHED_CREATE_CACHE_DIR",
			(CommandHandlercpp)&CachedServer::CreateCacheDir,
			"CachedServer::CreateCacheDir",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_LINK_CACHE_DIR,
			"CACHED_LINK_CACHE_DIR",
			(CommandHandlercpp)&CachedServer::LinkCacheDir,
			"CachedServer::LinkCacheDir",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_UPLOAD_FILES,
			"CACHED_UPLOAD_FILES",
			(CommandHandlercpp)&CachedServer::UploadToServer,
			"CachedServer::UploadFiles",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_REPLICA_DOWNLOAD_FILES,
			"CACHED_REPLICA_DOWNLOAD_FILES",
			(CommandHandlercpp)&CachedServer::DownloadFiles,
			"CachedServer::DownloadFiles",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_REPLICA_UPLOAD_FILES2,
			"CACHED_REPLICA_UPLOAD_FILES2",
			(CommandHandlercpp)&CachedServer::UploadFiles2,
			"CachedServer::UploadFiles2",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_DOWNLOAD_REDUNDANCY,
			"CACHED_DOWNLOAD_REDUNDANCY",
			(CommandHandlercpp)&CachedServer::DownloadRedundancy,
			"CachedServer::DownloadRedundancy",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_REPLICA_DOWNLOAD_FILES2,
			"CACHED_REPLICA_DOWNLOAD_FILES2",
			(CommandHandlercpp)&CachedServer::DownloadFiles2,
			"CachedServer::DownloadFiles2",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_DOWNLOAD_FILES,
			"CACHED_DOWNLOAD_FILES",
			(CommandHandlercpp)&CachedServer::DownloadFiles,
			"CachedServer::DownloadFiles",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_REMOVE_CACHE_DIR,
			"CACHED_REMOVE_CACHE_DIR",
			(CommandHandlercpp)&CachedServer::RemoveCacheDir,
			"CachedServer::RemoveCacheDir",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_UPDATE_LEASE,
			"CACHED_UPDATE_LEASE",
			(CommandHandlercpp)&CachedServer::UpdateLease,
			"CachedServer::UpdateLease",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_LIST_CACHE_DIRS,
			"CACHED_LIST_CACHE_DIRS",
			(CommandHandlercpp)&CachedServer::ListCacheDirs,
			"CachedServer::ListCacheDirs",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_LIST_CACHEDS,
			"CACHED_LIST_CACHEDS",
			(CommandHandlercpp)&CachedServer::ListCacheDs,
			"CachedServer::ListCacheDs",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );


		rc = daemonCore->Register_Command(
			CACHED_LIST_FILES_BY_PATH,
			"CACHED_LIST_FILES_BY_PATH",
			(CommandHandlercpp)&CachedServer::ListFilesByPath,
			"CachedServer::ListFilesByPath",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_CHECK_CONSISTENCY,
			"CACHED_CHECK_CONSISTENCY",
			(CommandHandlercpp)&CachedServer::CheckConsistency,
			"CachedServer::CheckConsistency",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_SET_REPLICATION_POLICY,
			"CACHED_SET_REPLICATION_POLICY",
			(CommandHandlercpp)&CachedServer::SetReplicationPolicy,
			"CachedServer::SetReplicationPolicy",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_GET_REPLICATION_POLICY,
			"CACHED_GET_REPLICATION_POLICY",
			(CommandHandlercpp)&CachedServer::GetReplicationPolicy,
			"CachedServer::GetReplicationPolicy",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_CREATE_REPLICA,
			"CACHED_CREATE_REPLICA",
			(CommandHandlercpp)&CachedServer::CreateReplica,
			"CachedServer::CreateReplica",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_INITIALIZE_CACHE,
			"CACHED_INITIALIZE_CACHE",
			(CommandHandlercpp)&CachedServer::ReceiveInitializeCache,
			"CachedServer::ReceiveInitializeCache",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_PROCESS_DATA_TASK,
			"CACHED_PROCESS_DATA_TASK",
			(CommandHandlercpp)&CachedServer::ReceiveProcessDataTask,
			"CachedServer::ReceiveProcessDataTask",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_ADVERTISE_REDUNDANCY,
			"CACHED_ADVERTISE_REDUNDANCY",
			(CommandHandlercpp)&CachedServer::ReceiveRedundancyAdvertisement,
			"CachedServer::ReceiveRedundancyAdvertisement",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_ADVERTISE_TO_ORIGIN,
			"CACHED_ADVERTISE_TO_ORIGIN",
			(CommandHandlercpp)&CachedServer::ReceiveCacheAdvertisement,
			"CachedServer::ReceiveCacheAdvertisement",
			this,
			DAEMON,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_REQUEST_LOCAL_REPLICATION,
			"CACHED_REQUEST_LOCAL_REPLICATION",
			(CommandHandlercpp)&CachedServer::ReceiveLocalReplicationRequest,
			"CachedServer::ReceiveLocalReplicationRequest",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_REQUEST_LOCAL_REPLICATION2,
			"CACHED_REQUEST_LOCAL_REPLICATION2",
			(CommandHandlercpp)&CachedServer::ReceiveLocalReplicationRequest2,
			"CachedServer::ReceiveLocalReplicationRequest2",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_DISTRIBUTE_REPLICAS,
			"CACHED_DISTRIBUTE_REPLICAS",
			(CommandHandlercpp)&CachedServer::ReceiveDistributeReplicas,
			"CachedServer::ReceiveDistributeReplicas",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_PROCESS_TASK,
			"CACHED_PROCESS_TASK",
			(CommandHandlercpp)&CachedServer::ProcessTask,
			"CachedServer::ProcessTask",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_ENCODE_DIR,
			"CACHED_ENCODE_DIR",
			(CommandHandlercpp)&CachedServer::DoEncodeDir,
			"CachedServer::DoEncodeDir",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_ENCODE_FILE,
			"CACHED_ENCODE_FILE",
			(CommandHandlercpp)&CachedServer::DoEncodeFile,
			"CachedServer::DoEncodeFile",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_DECODE_FILE,
			"CACHED_DECODE_FILE",
			(CommandHandlercpp)&CachedServer::DoDecodeFile,
			"CachedServer::DoDecodeFile",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_DISTRIBUTE_ENCODED_FILES,
			"CACHED_DISTRIBUTE_ENCODED_FILE",
			(CommandHandlercpp)&CachedServer::ReceiveDistributeEncodedFiles,
			"CachedServer::ReceiveDistributeEncodedFiles",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_PROBE_CACHED_SERVER,
			"CACHED_PROBE_CACHED_SERVER",
			(CommandHandlercpp)&CachedServer::ReceiveProbeCachedServer,
			"CachedServer::ReceiveProbeCachedServer",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_REQUEST_REDUNDANCY,
			"CACHED_REQUEST_REDUNDANCY",
			(CommandHandlercpp)&CachedServer::ReceiveRequestRedundancy,
			"CachedServer::ReceiveRequestRedundancy",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Reaper("dummy_reaper",
			(ReaperHandler)&dummy_reaper,
			"dummy_reaper",NULL);
		ASSERT( rc >= 0 );

	}

	// Create the name of the cache
	std::stringstream os;
	std::string param_name;
	param(param_name, "CACHED_NAME");
	char* raw_name;
	if (param_name.empty()) {
		os << "cached-" << daemonCore->getpid();
		raw_name = build_valid_daemon_name(os.str().c_str());
	} else {
		raw_name = build_valid_daemon_name(param_name.c_str());
	}
	m_daemonName = raw_name;
	dprintf(D_FULLDEBUG, "Setting name to %s\n", m_daemonName.c_str());
	delete [] raw_name;

	InitAndReconfig();

	// Register a timer to monitor the transfers
	m_active_transfer_timer = daemonCore->Register_Timer(60,
		(TimerHandlercpp)&CachedServer::CheckActiveTransfers,
		"CachedServer::CheckActiveTransfers",
		(Service*)this );

	// Register timer to advertise the caches on this server
	m_advertise_caches_timer = daemonCore->Register_Timer(60,
		(TimerHandlercpp)&CachedServer::AdvertiseCaches,
		"CachedServer::AdvertiseCaches",
		(Service*)this );

	// And run it:
	AdvertiseCaches();

	// Register timer to advertise the redundancy of all caches on this server
	m_advertise_redundancy_timer = daemonCore->Register_Timer(60,
		(TimerHandlercpp)&CachedServer::AdvertiseRedundancy,
		"CachedServer::AdvertiseRedundancy",
		(Service*)this );
	AdvertiseRedundancy();

	// Register timer to advertise the caches on this server
	// TODO: make the timer a variable
	m_advertise_cache_daemon_timer = daemonCore->Register_Timer(600,
		(TimerHandlercpp)&CachedServer::AdvertiseCacheDaemon,
		"CachedServer::AdvertiseCacheDaemon",
		(Service*)this );	

	// Advertise the daemon the first time
	AdvertiseCacheDaemon();	

	m_torrent_alert_timer = daemonCore->Register_Timer(10,
		(TimerHandlercpp)&CachedServer::HandleTorrentAlerts,
		"CachedServer::HandleTorrentAlerts",
		(Service*)this );	


	InitializeBittorrent();


	// Register timer to check up on pending replication requests
	/*
	m_replication_check = daemonCore->Register_Timer(10,
		(TimerHandlercpp)&CachedServer::CheckReplicationRequests,
		"CachedServer::CheckReplicationRequests",
		(Service*)this );
	*/
	m_check_redundancy_cached_timer = daemonCore->Register_Timer(10,
		(TimerHandlercpp)&CachedServer::CheckRedundancyCacheds,
		"CachedServer::CheckRedundancyCacheds",
		(Service*)this );
	CheckRedundancyCacheds();

	if(FindParentCache(m_parent.parent_ad)) {
		m_parent.has_parent = true;
	} else {
		m_parent.has_parent = false;
	}

	m_prune_bad_parents_timer = daemonCore->Register_Timer(60,
		(TimerHandlercpp)&CachedServer::PruneBadParents,
		"CachedServer::PruneBadParents",
		(Service*)this );	

	cron_job_mgr.Initialize( "cached" );
}


void CachedServer::PruneBadParents() {

	classad_unordered<std::string, time_t>::iterator it = m_failed_parents.begin();

	while (it != m_failed_parents.end()) {
		std::string parent_name = it->first;
		time_t bad_time = it->second;
		time_t current_time = time(NULL);

		if ((current_time - bad_time) > 3600) {
			dprintf(D_FULLDEBUG, "Parent %s has been pruned from cached bad parents\n", parent_name.c_str());
			it = m_failed_parents.erase(it);
			continue;
		}
		it++;
	}
}

void CachedServer::CheckReplicationRequests() {

	dprintf(D_FULLDEBUG, "In CheckReplicationRequests\n");


	counted_ptr<compat_classad::ClassAd> parent_ad;
	FindParentCache(parent_ad);

	dprintf(D_FULLDEBUG, "Completed Find Parent Cache\n");

	classad_unordered<std::string, compat_classad::ClassAd>::iterator it = m_requested_caches.begin();

	std::string parent_name;
	if (m_parent.has_parent) {
		m_parent.parent_ad->EvalString(ATTR_NAME, NULL, parent_name);
	}
	//m_parent.parent_ad->EvalString(ATTR_NAME, NULL, parent_name);

	for (it = m_requested_caches.begin(); it != m_requested_caches.end(); it++) {
		std::string cache_name = it->first;
		compat_classad::ClassAd cache_ad = it->second;
		std::string cached_origin;
		cache_ad.EvaluateAttrString(ATTR_CACHE_ORIGINATOR_HOST, cached_origin);

		dprintf(D_FULLDEBUG, "Checking replication status of %s from %s\n", cache_name.c_str(), cached_origin.c_str());

		CheckCacheReplicationStatus(cache_name, cached_origin);
	}

	// Now loop through the updated structure and look take action, if necessary
	it = m_requested_caches.begin();
	while (it != m_requested_caches.end()) {

		std::string cache_name = it->first;
		compat_classad::ClassAd cache_ad = it->second;

		std::string cached_origin, cached_parent;
		cache_ad.EvaluateAttrString(ATTR_CACHE_ORIGINATOR_HOST, cached_origin);

		std::string cache_status;
		int cache_state;

		// Check if we should do anything
		if (m_requested_caches[cache_name].EvaluateAttrInt(ATTR_CACHE_STATE, cache_state)) {
			std::string my_replication_methods;
			if (m_parent.parent_local) {
				my_replication_methods = "DIRECT";
			} else {
				param(my_replication_methods, "CACHE_REPLICATION_METHODS");
			}
			std::string transfer_method = NegotiateTransferMethod(m_requested_caches[cache_name], my_replication_methods);

			if (!m_parent.has_parent) {
				parent_name = cached_origin;
			}


			if((transfer_method == "DIRECT") && (cache_state == COMMITTED)) {

				// Put it in the log
				m_log->BeginTransaction();
				if (!m_log->AdExistsInTableOrTransaction(cache_name.c_str())) {
					LogNewClassAd* new_log = new LogNewClassAd(cache_name.c_str(), "*", "*");
					m_log->AppendLog(new_log);
				}
				int state = UNCOMMITTED;
				SetAttributeInt(cache_name, ATTR_CACHE_STATE, state);
				SetAttributeBool(cache_name, ATTR_CACHE_ORIGINATOR, false);
				SetAttributeString(cache_name, ATTR_CACHE_PARENT_CACHED, parent_name);
				m_log->CommitTransaction();

				DoDirectDownload(parent_name, m_requested_caches[cache_name]);
				it = m_requested_caches.erase(it);
				continue;

			} else if (transfer_method == "BITTORRENT") {

				// Put it in the log
				m_log->BeginTransaction();
				if (!m_log->AdExistsInTableOrTransaction(cache_name.c_str())) {
					LogNewClassAd* new_log = new LogNewClassAd(cache_name.c_str(), "*", "*");
					m_log->AppendLog(new_log);
				}
				int state = UNCOMMITTED;
				SetAttributeInt(cache_name, ATTR_CACHE_STATE, state);
				SetAttributeBool(cache_name, ATTR_CACHE_ORIGINATOR, false);
				SetAttributeString(cache_name, ATTR_CACHE_PARENT_CACHED, parent_name);
				m_log->CommitTransaction();

				DoBittorrentDownload(cache_ad);
				it = m_requested_caches.erase(it);
				continue;
			}
		}
		it++;
	}
//	daemonCore->Reset_Timer(m_replication_check, 10);
}


/**
 *	This function will initialize the torrents
 *
 */

void CachedServer::InitializeBittorrent() {


//	InitTracker();

	// Get all of the torrents that where being served by this cache
	std::stringstream query;
	query << "stringListIMember(\"BITTORRENT\"," << ATTR_CACHE_REPLICATION_METHODS << ")";
	std::list<compat_classad::ClassAd> caches = QueryCacheLog(query.str());

	// Add the torrents
	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	for(std::list<compat_classad::ClassAd>::iterator it = caches.begin(); it != caches.end(); it++) {

		// First, check if we have the torrent file
		std::string cache_name;
		it->LookupString(ATTR_CACHE_NAME, cache_name);
		CondorError err;
		std::string cache_dir = GetCacheDir(cache_name, err);
		StatInfo stat(cache_dir.c_str(), ".torrent");
		if(stat.Errno()) {
			// Torrent file doesn't exist
			std::string magnet_uri;
			it->LookupString(ATTR_CACHE_MAGNET_LINK, magnet_uri);
			dprintf(D_FULLDEBUG, "Restarting cache %s from magnet link\n", cache_name.c_str());
//			DownloadTorrent(magnet_uri, caching_dir, "");

		} else {

			std::string torrent_file = cache_dir + "/.torrent";
//			AddTorrentFromFile(torrent_file, cache_dir);
			dprintf(D_FULLDEBUG, "Restarting cache %s from torrent file\n", cache_name.c_str());

		}

	}

}



/**
 *	This function will be called on a time in order to check the active 
 *	transfers. This is where we can gather statistics on the transfers.
 */

void CachedServer::CheckActiveTransfers() {

	// We iterate the iterator inside the loop, list semantics demand it
	for(std::list<FileTransfer*>::iterator it = m_active_transfers.begin(); it != m_active_transfers.end();) {
		FileTransfer* ft_ptr = *it;
		FileTransfer::FileTransferInfo fi = ft_ptr->GetInfo();
		if (!fi.in_progress)
		{
			dprintf(D_FULLDEBUG, "CheckActiveTransfers: Finished transfers, removing file transfer object.\n");
			it = m_active_transfers.erase(it);
			delete ft_ptr;

		} else {
			dprintf(D_FULLDEBUG, "CheckActiveTransfers: Unfinished transfer detected\n");
			it++;
		}

	}

	daemonCore->Reset_Timer(m_active_transfer_timer, 60);

}

/**
 *	Handle the libtorrent alerts
 *
 */
void CachedServer::HandleTorrentAlerts() {

	dprintf(D_FULLDEBUG, "Handling Alerts\n");

	std::list<std::string> completed_torrents;
	std::list<std::string> errored_torrents;

//	HandleAlerts(completed_torrents, errored_torrents);

	for (std::list<std::string>::iterator it = completed_torrents.begin(); it != completed_torrents.end(); it++)
	{

		dprintf(D_FULLDEBUG, "Completed torrent %s\n", (*it).c_str());

		// Convert the magnet URI to an actual cache
		std::string query;
		query += ATTR_CACHE_MAGNET_LINK;
		query += " == \"";
		query += *it;
		query += "\"";
		std::list<compat_classad::ClassAd> caches = QueryCacheLog(query);

		if (caches.size() != 1) {
			int caches_size = caches.size();
			dprintf(D_FAILURE | D_ALWAYS, "Caches has size not equal to 1, but equal to %i\n", caches_size);
		} else {

			// Extract the Cache name
			compat_classad::ClassAd cache = caches.front();
			std::string cache_id;
			cache.LookupString(ATTR_CACHE_NAME, cache_id);


			SetCacheUploadStatus(cache_id, COMMITTED);
		}




	}

	daemonCore->Reset_Timer(m_torrent_alert_timer, 10);
}

/**
 * Generate the daemon's classad, with all the information
 */

compat_classad::ClassAd CachedServer::GenerateClassAd() {

	// Update the available caches on this server
	compat_classad::ClassAd published_classad;

	daemonCore->publish(&published_classad);

	cron_job_mgr.GetAds(published_classad);


	published_classad.InsertAttr("CachedServer", true);

	// Advertise the available disk space
	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	long long total_disk = sysapi_disk_space(caching_dir.c_str());
	published_classad.Assign( ATTR_TOTAL_DISK, total_disk );

	published_classad.InsertAttr(ATTR_NAME, m_daemonName.c_str());

	classad::ClassAdParser   parser;
	ExprTree    *tree;
	tree = parser.ParseExpression("MY.TotalDisk > TARGET.DiskUsage");
	published_classad.Insert(ATTR_REQUIREMENTS, tree);

	return published_classad;

}

/**
 * Comparison for caching ads
 */
bool compare_cachedname (const compat_classad::ClassAd first, const compat_classad::ClassAd second) {

	std::string machine1, machine2;

	first.LookupString(ATTR_CACHE_ORIGINATOR_HOST, machine1);
	second.LookupString(ATTR_CACHE_ORIGINATOR_HOST, machine2);

	if (machine1 < machine2) 
		return true;
	else 
		return false;



}

/**
 * Advertise the daemon to the collector
 *
 */
void CachedServer::AdvertiseCacheDaemon() {

	// Update the available caches on this server
	compat_classad::ClassAd published_classad = GenerateClassAd();

	dPrintAd(D_FULLDEBUG, published_classad);
	dprintf(D_FULLDEBUG, "About to send update to collectors...\n");
	int rc = daemonCore->sendUpdates(UPDATE_AD_GENERIC, &published_classad);
	if (rc == 0) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to send commands to collectors, rc = %i\n", rc);
	} else {
		dprintf(D_FULLDEBUG, "Sent updates to %i collectors\n", rc);
	}


	// Update the cache originators that we have their caches

	// Query the cache log for all caches which we don't own.
	classad::ClassAdParser	parser;
	char buf[512];
	sprintf(buf, "(%s == %i) && (%s =?= false)", ATTR_CACHE_STATE, COMMITTED, ATTR_CACHE_ORIGINATOR);

	std::list<compat_classad::ClassAd> caches = QueryCacheLog(buf);

	// Sort by the cached name, so we can send multiple udpates with 1 negotiation
	caches.sort(compare_cachedname);

	std::list<compat_classad::ClassAd>::iterator i = caches.begin();
	while (i != caches.end()) {
		// Connect to the daemon
		std::string remote_daemon_name;
		if(i->EvalString(ATTR_CACHE_ORIGINATOR_HOST, NULL, remote_daemon_name) == 0) {
			std::string cache_name;
			i->EvalString(ATTR_CACHE_NAME, NULL, cache_name);
			dprintf(D_FAILURE | D_ALWAYS, "Cache %s does not have an originator daemon, ignoring\n", cache_name.c_str());
			i++;
			continue;
		}

		// Query the collector for the cached
		dprintf(D_FULLDEBUG, "Querying for the daemon %s\n", remote_daemon_name.c_str());
		CollectorList* collectors = daemonCore->getCollectorList();
		CondorQuery query(ANY_AD);
		query.addANDConstraint("CachedServer =?= TRUE");
		std::string created_constraint = "Name =?= \"";
		created_constraint += remote_daemon_name.c_str();
		created_constraint += "\"";
		QueryResult add_result = query.addANDConstraint(created_constraint.c_str());
		ClassAdList adList;
		QueryResult result = collectors->query(query, adList, NULL);
		dprintf(D_FULLDEBUG, "Got %i ads from query\n", adList.Length());

		if (adList.Length() < 1) {
			dprintf(D_FAILURE | D_ALWAYS, "Failed to locate daemon %s\n", remote_daemon_name.c_str());
			i++;
			continue;
		}

		adList.Open();
		ClassAd* remote_cached_ad = adList.Next();
		dPrintAd(D_FULLDEBUG, *remote_cached_ad);
		DaemonAllowLocateFull remote_cached(remote_cached_ad, DT_GENERIC, NULL);

		//Daemon remote_cached(DT_GENERIC, remote_daemon_name.c_str());
		if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
			dprintf(D_FAILURE | D_ALWAYS, "Unable to locate daemon %s, error: %s\n", remote_daemon_name.c_str(), remote_cached.error());
			i++;
			continue;
		}

		ReliSock* rsock = (ReliSock*)remote_cached.startCommand(CACHED_ADVERTISE_TO_ORIGIN, Stream::reli_sock, 20);
		if (!rsock) {
			dprintf(D_FAILURE | D_ALWAYS, "Unable to start command to daemon %s at address %s, error: %s\n", remote_daemon_name.c_str(), remote_cached.addr(), remote_cached.error());
			i++;
			continue;
		}

		// Send the full classad that we are hosting, no reason not to?
		dprintf(D_FULLDEBUG, "Sending classad to %s:\n", remote_daemon_name.c_str());
		i->InsertAttr(ATTR_MACHINE, m_daemonName);
		dPrintAd(D_FULLDEBUG, *i);

		if (!putClassAd(rsock, *i) || !rsock->end_of_message()) {
			dprintf(D_FAILURE | D_ALWAYS, "Failed to send Ad to %s\n", remote_daemon_name.c_str());
		}
		compat_classad::ClassAd terminator_classad;
		terminator_classad.InsertAttr("FinalAdvertisement", true);
		i++;

		// Now loop through all the caches that have the same remote daemon name
		while(i != caches.end()) {

			// Can we get the originator host, if not, bail.
			std::string new_remote_daemon_name;
			if(i->EvalString(ATTR_CACHE_ORIGINATOR_HOST, NULL, new_remote_daemon_name) == 0) {
				if (!putClassAd(rsock, terminator_classad) || !rsock->end_of_message()) {
					dprintf(D_FAILURE | D_ALWAYS, "Failed to send Ad to %s\n", remote_daemon_name.c_str());
				}
				i++;
				break;
			}

			// If this daemon name is not the same as the previous, start the process over.
			if(new_remote_daemon_name != remote_daemon_name) {
				if (!putClassAd(rsock, terminator_classad) || !rsock->end_of_message()) {
					dprintf(D_FAILURE | D_ALWAYS, "Failed to send Ad to %s\n", remote_daemon_name.c_str());
				}
				break;
			}

			// else, send the cache classad
			// Insert the machine name in the classad
			i->InsertAttr(ATTR_MACHINE, m_daemonName);
			if (!putClassAd(rsock, *i) || !rsock->end_of_message()) {
				dprintf(D_FAILURE | D_ALWAYS, "Failed to send Ad to %s\n", remote_daemon_name.c_str());
			}
			i++;

		}

		// Finally, send the terminator and close the socket.
		dprintf(D_FULLDEBUG, "Sending terminator classad\n");
		if (!putClassAd(rsock, terminator_classad) || !rsock->end_of_message()) {
			dprintf(D_FAILURE | D_ALWAYS, "Failed to send Ad to %s\n", remote_daemon_name.c_str());
		}
		rsock->close();
		//rsock->close();
		delete rsock;


	}



	// Reset the timer
	daemonCore->Reset_Timer(m_advertise_cache_daemon_timer, 60);

}


/**
 *	Advertise the caches stored on this server
 *
 */
void CachedServer::AdvertiseCaches() {

	classad::ClassAdParser	parser;

	// Create the requirements expression
	char buf[512];
	sprintf(buf, "(%s == %i) && (%s =?= true)", ATTR_CACHE_STATE, COMMITTED, ATTR_CACHE_ORIGINATOR);
	dprintf(D_FULLDEBUG, "AdvertiseCaches: Cache Query = %s\n", buf);

	std::list<compat_classad::ClassAd> caches = QueryCacheLog(buf);

	// If there are no originator caches, then don't do anything
	if (caches.size() == 0) {
		return;
	}

	// Get the caching daemons from the collector
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	query.addANDConstraint("CachedServer =?= TRUE");
	std::string created_constraint = "Name =!= \"";
	created_constraint += m_daemonName.c_str();
	created_constraint += "\"";
	QueryResult add_result = query.addANDConstraint(created_constraint.c_str());


	switch(add_result) {
		case Q_OK:
			break;
		case Q_INVALID_CATEGORY:
			dprintf(D_FAILURE | D_ALWAYS, "Invalid category: failed to query collector\n");
			break;
		case Q_MEMORY_ERROR:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to add query collector: Memory error\n");
			break;
		case Q_PARSE_ERROR:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to add query collector: Parse constraints error\n");
			break;
		case Q_COMMUNICATION_ERROR:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to add query collector: Communication error\n");
			break;
		case Q_INVALID_QUERY:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to add query collector: Invalid Query\n");
			break;
		case Q_NO_COLLECTOR_HOST:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to add query collector: No collector host\n");
			break;
		default:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to add query collector\n");

	}



	ClassAdList adList;
	ClassAd query_classad;
	query.getQueryAd(query_classad);
	dPrintAd(D_FULLDEBUG, query_classad);
	QueryResult result = collectors->query(query, adList, NULL);

	switch(result) {
		case Q_OK:
			break;
		case Q_INVALID_CATEGORY:
			dprintf(D_FAILURE | D_ALWAYS, "Invalid category: failed to query collector\n");
			break;
		case Q_MEMORY_ERROR:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to query collector: Memory error\n");
			break;
		case Q_PARSE_ERROR:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to query collector: Parse constraints error\n");
			break;
		case Q_COMMUNICATION_ERROR:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to query collector: Communication error\n");
			break;
		case Q_INVALID_QUERY:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to query collector: Invalid Query\n");
			break;
		case Q_NO_COLLECTOR_HOST:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to query collector: No collector host\n");
			break;
		default:
			dprintf(D_FAILURE | D_ALWAYS, "Failed to query collector\n");

	}

	dprintf(D_FULLDEBUG, "Got %i ads from query\n", adList.Length());
	ClassAd *ad;
	adList.Open();

	// Loop through the caches and the cached's and attempt to match.
	while ((ad = adList.Next())) {
		DCCached cached_daemon(ad, NULL);
		if(!cached_daemon.locate(Daemon::LOCATE_FULL)) {
			dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
			continue;
		} else {
			dprintf(D_FULLDEBUG, "Located daemon at %s\n", cached_daemon.name());
		}
		ClassAdList matched_caches;
		std::list<compat_classad::ClassAd>::iterator cache_iterator = caches.begin();


		while ((cache_iterator != caches.end())) {

			if(NegotiateCache(*cache_iterator, *ad)) {
				dprintf(D_FULLDEBUG, "Cache matched cached\n");
				compat_classad::ClassAd *new_ad = (compat_classad::ClassAd*)cache_iterator->Copy();
				new_ad->SetParentScope(NULL);
				matched_caches.Insert(new_ad);
			} else {
				dprintf(D_FULLDEBUG, "Cache did not match cache\n");
			}

			cache_iterator++;
		}

		//dPrintAd(D_FULLDEBUG, *ad);

		// Now send the matched caches to the remote cached
		if (matched_caches.Length() > 0) {
			// Start the command

			matched_caches.Open();

			for (int i = 0; i < matched_caches.Length(); i++) {
				compat_classad::ClassAd * ad = matched_caches.Next();

				compat_classad::ClassAd response;
				CondorError err;
				std::string origin_server, cache_name;

				ad->EvalString(ATTR_CACHE_ORIGINATOR_HOST, NULL, origin_server);
				ad->EvalString(ATTR_CACHE_NAME, NULL, cache_name);

				cached_daemon.requestLocalCache(origin_server, cache_name, response, err);



			}
		}
	}



	dprintf(D_FULLDEBUG, "Done with query of collector\n");

	daemonCore->Reset_Timer(m_advertise_caches_timer, 60);


}


CachedServer::~CachedServer()
{
}


	void
CachedServer::InitAndReconfig()
{
	m_db_fname = param("CACHED_DATABASE");
	m_log = new ClassAdLog<std::string,ClassAd*>(m_db_fname.c_str());
	InitializeDB2();
}


	int
CachedServer::InitializeDB()
{
	// Check for all caches that we are the origin and update the originator name.
	std::string cache_query = ATTR_CACHE_ORIGINATOR;
	cache_query += " == true";
	std::list<compat_classad::ClassAd> caches = QueryCacheLog(cache_query);
	dprintf(D_FULLDEBUG, "In CachedServer::InitializeDB(), caches.size = %d\n", caches.size());
	
	for (std::list<compat_classad::ClassAd>::iterator it = caches.begin(); it != caches.end(); it++) {

		m_log->BeginTransaction();

		std::string cache_name;
		std::string cache_id_str;
		it->EvalString(ATTR_CACHE_NAME, NULL, cache_name);
		it->EvalString(ATTR_CACHE_ID, NULL, cache_id_str);
		std::string dirname = cache_name + "+" + cache_id_str;
		if (!m_log->AdExistsInTableOrTransaction(dirname.c_str())) { continue; }
		// TODO: Convert this to a real state.
		std::string origin_host = "\"" + m_daemonName + "\"";
		dprintf(D_FULLDEBUG, "In CachedServer::InitializeDB(), origin_host = %s\n", origin_host.c_str());
		SetAttributeString(dirname, ATTR_CACHE_ORIGINATOR_HOST, origin_host);

		m_log->CommitTransaction();
	}
	

	return 0;

}

int CachedServer::InitializeDB2()
{
	// Check for all caches that we are the origin and update the originator name.
	std::string cache_query = "CacheOriginator";
	cache_query += " == true";
	std::list<compat_classad::ClassAd> caches = QueryCacheLog(cache_query);
	dprintf(D_FULLDEBUG, "In CachedServer::InitializeDB2(), caches.size = %d\n", caches.size());
	
	for (std::list<compat_classad::ClassAd>::iterator it = caches.begin(); it != caches.end(); it++) {

		std::string cache_name;
		std::string cache_id_str;
		it->EvalString(ATTR_CACHE_NAME, NULL, cache_name);
		it->EvalString(ATTR_CACHE_ID, NULL, cache_id_str);
		std::string dirname = cache_name + "+" + cache_id_str;
		CondorError err;
		DoRemoveCacheDir(dirname.c_str(), err);
		dprintf(D_FULLDEBUG, "In CachedServer::InitializeDB2(), dirname = %s\n", dirname.c_str());
	}
	
	return 0;
}


// We keep enough information in the cache directory to rebuild the DB contents
// It's not written out atomically - if the DB is shutdown uncleanly and then
// HTCondor is upgraded, we might be in trouble.
	int
CachedServer::RebuildDB()
{
	return 0;
	// Iterate through each of the cache directories.
	// Read in the cache.ad and lease.ad files.
	// Make the appropriate SQL call.
}

static int PutErrorAd(Stream *sock, int rc, const std::string &methodName, const std::string &errMsg)
{
	compat_classad::ClassAd ad;
	ad.InsertAttr(ATTR_ERROR_CODE, rc);
	ad.InsertAttr(ATTR_ERROR_STRING, errMsg);
	dprintf(D_FULLDEBUG, "%s: rc=%d, %s\n", methodName.c_str(), rc, errMsg.c_str());
	if (!putClassAd(sock, ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to send response ad (rc=%d, %s).\n", rc, errMsg.c_str());
		return 1;
	}
	return 1;
}

int CachedServer::CreateCacheDir2(int /*cmd*/, Stream *sock)
{
	Sock *real_sock = (Sock*)sock;
	CondorError err;

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to read request for CreateCacheDir.\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In CreateCacheDir2, printing classad\n");//##
	dPrintAd(D_FULLDEBUG, request_ad);//##
	std::string cache_name;
	std::string cache_id_str;
	time_t lease_expiry;
	std::string version;
	std::string requesting_cached_server;
	std::string redundancy_method;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrInt("LeaseExpiration", lease_expiry))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing LeaseExpiration attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", cache_name))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing CacheName attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing CacheID attribute");
	}
	if (!request_ad.EvaluateAttrString("RequestingCachedServer", requesting_cached_server))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing RequestingCachedServer attribute");
	}
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing RedundancyMethod attribute");
	}
	int data_number = -1;
	int parity_number = -1;
	std::string redundancy_candidates;
	if(!request_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing DataNumber attribute");
	}
	if (!request_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing ParityNumber attribute");
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir2", "Request missing RedundancyCandidates attribute");
	}
	time_t now = time(NULL);
	time_t lease_lifetime = lease_expiry - now;
	if (lease_lifetime < 0)
	{
		return PutErrorAd(sock, 3, "CreateCacheDir2", "Requested expiration is already past");
	}
	time_t max_lease_lifetime = param_integer("MAX_CACHED_LEASE", 86400);
	if (lease_lifetime > max_lease_lifetime)
	{
		lease_expiry = now + max_lease_lifetime;
	}

	std::string dirname = cache_name + "+" + cache_id_str;
	// Make sure the cache doesn't already exist
	compat_classad::ClassAd* cache_ad;
	int existed = -1;
	if (GetCacheAd(dirname.c_str(), cache_ad, err)) {
		// Cache ad exists, cannot recreate
		existed = 1;
		dprintf(D_FULLDEBUG, "Client requested to create cache %s, but it already exists\n", dirname.c_str());
	} else {
		existed = 0;
		CreateCacheDirectory(dirname, err);
	}

	std::string authenticated_user = real_sock->getFullyQualifiedUser();

	m_log->BeginTransaction();
	SetAttributeString(dirname, ATTR_CACHE_NAME, cache_name);
	SetAttributeString(dirname, ATTR_CACHE_ID, cache_id_str);
	SetAttributeLong(dirname, ATTR_LEASE_EXPIRATION, lease_expiry);
	SetAttributeString(dirname, ATTR_OWNER, authenticated_user);
	SetAttributeString(dirname, ATTR_CACHE_ORIGINATOR_HOST, m_daemonName);
	
	// TODO: Make requirements more dynamic by using ATTR values.
	SetAttributeString(dirname, ATTR_REQUIREMENTS, "MY.DiskUsage < TARGET.TotalDisk");
	SetAttributeString(dirname, ATTR_CACHE_REPLICATION_METHODS, "DIRECT");
	SetAttributeBool(dirname, ATTR_CACHE_ORIGINATOR, true);
	int state = UNCOMMITTED;
	SetAttributeInt(dirname, ATTR_CACHE_STATE, state);
	SetAttributeString(dirname, "RedundancyManager", requesting_cached_server);
	SetAttributeString(dirname, "RedundancyMethod", redundancy_method);
	SetAttributeInt(dirname, "DataNumber", data_number);
	SetAttributeInt(dirname, "ParityNumber", parity_number);
	SetAttributeString(dirname, "RedundancyCandidates", redundancy_candidates);
	if(requesting_cached_server == m_daemonName) {
		SetAttributeBool(dirname, "IsRedundancyManager", true);
	} else {
		SetAttributeBool(dirname, "IsRedundancyManager", false);
	}
	m_log->CommitTransaction();

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_CACHE_NAME, dirname);
	response_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to write CreateCacheDir response to client.\n");
	}

	return existed ? 1 : 0;
}

int CachedServer::CreateCacheDir(int /*cmd*/, Stream *sock)
{
	Sock *real_sock = (Sock*)sock;
	CondorError err;

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to read request for CreateCacheDir.\n");
		return 1;
	}
	std::string dirname;
	time_t lease_expiry;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrInt("LeaseExpiration", lease_expiry))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir", "Request missing LeaseExpiration attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", dirname))
	{
		return PutErrorAd(sock, 1, "CreateCacheDir", "Request missing CacheName attribute");
	}
	time_t now = time(NULL);
	time_t lease_lifetime = lease_expiry - now;
	if (lease_lifetime < 0)
	{
		return PutErrorAd(sock, 3, "CreateCacheDir", "Requested expiration is already past");
	}
	time_t max_lease_lifetime = param_integer("MAX_CACHED_LEASE", 86400);
	if (lease_lifetime > max_lease_lifetime)
	{
		lease_expiry = now + max_lease_lifetime;
	}

	// Make sure the cache doesn't already exist
	compat_classad::ClassAd* cache_ad;
	if (GetCacheAd(dirname.c_str(), cache_ad, err)) {
		// Cache ad exists, cannot recreate
		dprintf(D_ALWAYS | D_FAILURE, "Client requested to create cache %s, but it already exists\n", dirname.c_str());
		return PutErrorAd(sock, 1, "CreateCacheDir", "Cache already exists.  Cannot recreate.");

	}

	// Insert ad into cache
	// Create a uuid for the cache
	boost::uuids::uuid u = boost::uuids::random_generator()();
	const std::string cache_id_str = boost::lexical_cast<std::string>(u);
	//long long cache_id = m_id++;
	//std::string cache_id_str = boost::lexical_cast<std::string>(cache_id);
	boost::replace_all(dirname, "$(UNIQUE_ID)", cache_id_str);

	CreateCacheDirectory(dirname, err);

	std::string authenticated_user = real_sock->getFullyQualifiedUser();

	m_log->BeginTransaction();
	SetAttributeString(dirname, ATTR_CACHE_NAME, dirname);
	SetAttributeString(dirname, ATTR_CACHE_ID, cache_id_str);
	SetAttributeLong(dirname, ATTR_LEASE_EXPIRATION, lease_expiry);
	SetAttributeString(dirname, ATTR_OWNER, authenticated_user);
	SetAttributeString(dirname, ATTR_CACHE_ORIGINATOR_HOST, m_daemonName);
	
	// TODO: Make requirements more dynamic by using ATTR values.
	SetAttributeString(dirname, ATTR_REQUIREMENTS, "MY.DiskUsage < TARGET.TotalDisk");
	SetAttributeString(dirname, ATTR_CACHE_REPLICATION_METHODS, "DIRECT");
	SetAttributeBool(dirname, ATTR_CACHE_ORIGINATOR, true);
	int state = UNCOMMITTED;
	SetAttributeInt(dirname, ATTR_CACHE_STATE, state);
	m_log->CommitTransaction();

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_CACHE_NAME, dirname);
	response_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to write CreateCacheDir response to client.\n");
	}

	return 0;
}

int CachedServer::LinkCacheDir(int /*cmd*/, Stream *sock)
{

	dprintf(D_FULLDEBUG, "In LinkCacheDir 1");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to read request for CreateCacheDir.\n");
		return 1;
	}
	std::string cache_name;
	time_t lease_expiry;
	std::string version;
	std::string directory_path;
//	std::string requesting_cached_server;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		return PutErrorAd(sock, 1, "InitializeCacheDir", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrInt("LeaseExpiration", lease_expiry))
	{
		return PutErrorAd(sock, 1, "InitializeCacheDir", "Request missing LeaseExpiration attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", cache_name))
	{
		return PutErrorAd(sock, 1, "InitializeCacheDir", "Request missing CacheName attribute");
	}
	if (!request_ad.EvaluateAttrString("DirectoryPath", directory_path))
	{
		return PutErrorAd(sock, 1, "InitializeCacheDir", "Request missing DirectoryPath attribute");
	}
	time_t now = time(NULL);
	time_t lease_lifetime = lease_expiry - now;
	if (lease_lifetime < 0)
	{
		return PutErrorAd(sock, 3, "InitializeCacheDir", "Requested expiration is already past");
	}
	time_t max_lease_lifetime = param_integer("MAX_CACHED_LEASE", 86400);
	if (lease_lifetime > max_lease_lifetime)
	{
		lease_expiry = now + max_lease_lifetime;
	}
	dprintf(D_FULLDEBUG, "In LinkCacheDir 2");//##

	// Insert ad into cache
	// Create a uuid for the cache
	boost::uuids::uuid u = boost::uuids::random_generator()();
	const std::string cache_id_str = boost::lexical_cast<std::string>(u);
	//long long cache_id = m_id++;
	//std::string cache_id_str = boost::lexical_cast<std::string>(cache_id);
	const std::string dirname = cache_name + "+" + cache_id_str;

	CondorError err;
	if(LinkCacheDirectory(directory_path, dirname, err)) {
		return PutErrorAd(sock, 3, "InitializeCacheDir", "LinkCacheDirecoty failed");
	}

	std::string authenticated_user = ((Sock *)sock)->getFullyQualifiedUser();

	dprintf(D_FULLDEBUG, "In LinkCacheDir 3");//##
	m_log->BeginTransaction();
	SetAttributeString(dirname, ATTR_CACHE_NAME, cache_name);
	SetAttributeString(dirname, ATTR_CACHE_ID, cache_id_str);
	SetAttributeLong(dirname, ATTR_LEASE_EXPIRATION, lease_expiry);
	SetAttributeString(dirname, ATTR_OWNER, authenticated_user);
	SetAttributeString(dirname, ATTR_CACHE_ORIGINATOR_HOST, m_daemonName);
	
	// TODO: Make requirements more dynamic by using ATTR values.
	SetAttributeString(dirname, ATTR_REQUIREMENTS, "MY.DiskUsage < TARGET.TotalDisk");
	SetAttributeString(dirname, ATTR_CACHE_REPLICATION_METHODS, "DIRECT");
	SetAttributeBool(dirname, ATTR_CACHE_ORIGINATOR, true);
	int state = COMMITTED;
	SetAttributeInt(dirname, ATTR_CACHE_STATE, state);
	SetAttributeString(dirname, "RedundancyManager", m_daemonName);
	SetAttributeBool(dirname, "IsRedundancyManager", false);
	m_log->CommitTransaction();

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	response_ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
	response_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	response_ad.InsertAttr("StorageCost", 0);
	response_ad.InsertAttr("ComputationCost", 0);
	response_ad.InsertAttr("NetworkCost", 0);
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to write CreateCacheDir response to client.\n");
	}
	dprintf(D_FULLDEBUG, "In LinkCacheDir 4");//##

	return 0;
}


class UploadFilesHandler : public Service
{
	friend class CachedServer;

	public:
	int handle(FileTransfer *);

	private:
	UploadFilesHandler(CachedServer &server, const std::string &cacheName);

	CachedServer &m_server;
	std::string m_cacheName;
};


	UploadFilesHandler::UploadFilesHandler(CachedServer &server, const std::string &cacheName)
: m_server(server),
	m_cacheName(cacheName)
{}


	int
UploadFilesHandler::handle(FileTransfer * ft_ptr)
{
	if (!ft_ptr) { return 0; }
	FileTransfer::FileTransferInfo fi = ft_ptr->GetInfo();
	if (!fi.in_progress)
	{
		dprintf(D_FULLDEBUG, "Finished transfer\n");
		if (fi.success) {
			// Anything that needs to be done when a cache uploaded is completed should be here

			filesize_t cache_size = m_server.CalculateCacheSize(m_cacheName);
			m_server.SetLogCacheSize(m_cacheName, (cache_size / 1000)+1);	
			m_server.SetCacheUploadStatus(m_cacheName, CachedServer::COMMITTED);
			CondorError err;
			dprintf(D_FULLDEBUG, "Creating torrent\n");
			std::string cache_dir = m_server.GetCacheDir(m_cacheName, err);
			compat_classad::ClassAd* cache_ad;
			m_server.GetCacheAd(m_cacheName, cache_ad, err);
			std::string cache_id;
			cache_ad->LookupString(ATTR_CACHE_ID, cache_id);

			// If this is a replication, then don't make another magnet link, use the
			// current one.
			std::string parent_cached;
			if(cache_ad->EvalString(ATTR_CACHE_MAGNET_LINK, NULL, parent_cached)) 
			{
				//m_server.DoBittorrentDownload(*cache_ad, false);
			}
			else 
			{
				std::string magnet_link;
//				std::string magnet_link = MakeTorrent(cache_dir, cache_id);
//				m_server.SetTorrentLink(m_cacheName, magnet_link);
			}



		} else {
			dprintf(D_FAILURE | D_ALWAYS, "Transfer failed\n");
			CondorError err;

			// Remove the cache if it fails to be transferred
			m_server.DoRemoveCacheDir(m_cacheName, err);
			//m_server.SetCacheUploadStatus(m_cacheName, CachedServer::UNCOMMITTED);
		}
		delete this;
	}
	return 0;
}


int CachedServer::UploadToServer(int /*cmd*/, Stream * sock)
{
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to read request for UploadFiles.\n");
		return 1;
	}
	std::string cache_name;
	std::string cache_id_str;
	std::string version;
	filesize_t diskUsage;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in UploadToServer request\n");
		return PutErrorAd(sock, 1, "UploadFiles", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", cache_name))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName in UploadToServer request\n");
		return PutErrorAd(sock, 1, "UploadFiles", "Request missing CacheName attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName in UploadToServer request\n");
		return PutErrorAd(sock, 1, "UploadFiles", "Request missing CacheID attribute");
	}
	if (!request_ad.LookupInteger(ATTR_DISK_USAGE, diskUsage))
	{
		dprintf(D_FULLDEBUG, "Client did not include %s in UploadToServer request\n", ATTR_DISK_USAGE);
		return PutErrorAd(sock, 1, "UploadFiles", "Request missing DiskUsage attribute");
	}

	CondorError err;
	std::string dirname = cache_name + "+" + cache_id_str;
	compat_classad::ClassAd *cache_ad;
	if (!GetCacheAd(dirname, cache_ad, err))
	{
		dprintf(D_ALWAYS, "Unable to find dirname = %s in log\n", dirname.c_str());
		return PutErrorAd(sock, 1, "UploadFiles", err.getFullText());
	}

	// Make sure the authenticated user is allowed to upload to this cache
	std::string authenticated_user = ((Sock*)sock)->getFullyQualifiedUser();
	std::string cache_owner;
	cache_ad->EvalString(ATTR_OWNER, NULL, cache_owner);

	if ( authenticated_user != cache_owner ) {
		PutErrorAd(sock, 1, "UploadFiles", "Error, cache owner does not match authenticated owner. Client may only upload to their own cache.");
	}

	// Check if the current dir is in a committed state
	CACHE_STATE cache_state = GetUploadStatus(dirname);

	if (cache_state == COMMITTED) {
		dprintf(D_ALWAYS, "Cache %s is already commited, cannot upload files.\n", dirname.c_str());
		return PutErrorAd(sock, 1, "UploadFiles", "Cache is already committed.  Cannot upload more files.");
	} else if (cache_state == UPLOADING) {
		dprintf(D_ALWAYS, "Cache %s is uploading, cannot upload files.\n", dirname.c_str());
		return PutErrorAd(sock, 1, "UploadFiles", "Cache is already uploading.  Cannot upload more files.");
	} else if (cache_state == INVALID) {
		dprintf(D_ALWAYS, "Cache %s is in invalid state, cannot upload files.\n", dirname.c_str());
		return PutErrorAd(sock, 1, "UploadFiles", "Cache is invalid.  Cannot upload more files.");
	}


	std::string cachingDir = GetCacheDir(dirname, err);
	compat_classad::ClassAd response_ad;
	std::string my_version = CondorVersion();
	response_ad.InsertAttr("CondorVersion", my_version);
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);


	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_ALWAYS, "Failed to send return message to client\n");
		return 1;
	}

	dprintf(D_FULLDEBUG, "Successfully sent response_ad to client\n");
	// From here on out, this is the file transfer server socket.
	int rc;
	FileTransfer* ft = new FileTransfer();
	m_active_transfers.push_back(ft);
	cache_ad->InsertAttr(ATTR_JOB_IWD, cachingDir.c_str());
	cache_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, cachingDir);

	// TODO: Enable file ownership checks
	rc = ft->SimpleInit(cache_ad, false, true, static_cast<ReliSock*>(sock));
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed simple init\n");
	} else {
		dprintf(D_FULLDEBUG, "Successfully SimpleInit of filetransfer\n");
	}

	ft->setPeerVersion(version.c_str());
	//	UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	//	ft->RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);

	// Restrict the amount of data that the file transfer will transfer
	ft->setMaxDownloadBytes(diskUsage);


	rc = ft->DownloadFiles();
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed DownloadFiles\n");
	} else {
		dprintf(D_FULLDEBUG, "Successfully began downloading files\n");
		SetCacheUploadStatus(dirname.c_str(), UPLOADING);

	}

	UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);//##
	handler->handle(ft);//##
	return KEEP_STREAM;
}

int CachedServer::DownloadFiles(int cmd, Stream * sock)
{
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for DownloadFiles.\n");
		return 1;
	}
	std::string dirname;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in DownloadFiles request\n");
		return PutErrorAd(sock, 1, "DownloadFiles", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", dirname))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName in DownloadFiles request\n");
		return PutErrorAd(sock, 1, "DownloadFiles", "Request missing CacheName attribute");
	}

	CondorError err;
	compat_classad::ClassAd *cache_ad;
	if (!GetCacheAd(dirname, cache_ad, err))
	{
		return PutErrorAd(sock, 1, "DownloadFiles", err.getFullText());
	}

	// Make sure the authenticated user is allowed to download this cache
	std::string authenticated_user = ((Sock*)sock)->getFullyQualifiedUser();
	std::string cache_owner;
	cache_ad->EvalString(ATTR_OWNER, NULL, cache_owner);

	if ( (cmd != CACHED_REPLICA_DOWNLOAD_FILES) && (authenticated_user != cache_owner) ) {
		dprintf(D_FAILURE | D_ALWAYS, "Download Files authentication error: authenticated: %s != cache: %s, denying download\n", authenticated_user.c_str(), cache_owner.c_str());
		return PutErrorAd(sock, 1, "DownloadFiles", "Error, cache owner does not match authenticated owner. Client may only download their own cache.");
	}

	if ( GetUploadStatus(dirname) != COMMITTED ) {
		return PutErrorAd(sock, 1, "DownloadFiles", "Cannot download cache which is not COMMITTED");
	}

	std::string requested_methods;
	if(request_ad.EvaluateAttrString(ATTR_CACHE_REPLICATION_METHODS, requested_methods)) {
		std::string method = NegotiateTransferMethod(request_ad, "HARDLINK, DIRECT");
		if(method == "HARDLINK") {
			compat_classad::ClassAd ad;
			ad.InsertAttr(ATTR_CACHE_REPLICATION_METHODS, method);
			if (!putClassAd(sock, ad) || !sock->end_of_message())
			{
				// Can't send another response!  Must just hang-up.
				return 1;
			}
			return DoHardlinkTransfer((ReliSock*)sock, dirname);
		}
	}

	// Return the cache ad.
	std::string my_version = CondorVersion();
	cache_ad->InsertAttr("CondorVersion", my_version);
	cache_ad->InsertAttr(ATTR_ERROR_CODE, 0);

	if (!putClassAd(sock, *cache_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
	}

	compat_classad::ClassAd transfer_ad;

	// Set the files to transfer
	std::string cache_dir = GetCacheDir(dirname, err);
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, cache_dir.c_str());
	transfer_ad.InsertAttr(ATTR_JOB_IWD, cache_dir.c_str());
	MyString err_str;

	if (!FileTransfer::ExpandInputFileList(&transfer_ad, err_str)) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to expand transfer list %s: %s\n", cache_dir.c_str(), err_str.c_str());
		//PutErrorAd(sock, 1, "DownloadFiles", err_str.c_str());
	}

	std::string transfer_files;
	transfer_ad.EvaluateAttrString(ATTR_TRANSFER_INPUT_FILES, transfer_files);
	dprintf(D_FULLDEBUG, "Expanded file list: %s", transfer_files.c_str());

	// From here on out, this is the file transfer server socket.
	// TODO: Handle user permissions for the files

	// The file transfer object is deleted automatically by the upload file 
	// handler.
	FileTransfer* ft = new FileTransfer();
	ft->SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(sock));
	ft->setPeerVersion(version.c_str());
	//UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	//ft->RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
	ft->UploadFiles();
	return KEEP_STREAM;
}

int CachedServer::UploadFiles2(int cmd, Stream * sock)
{
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for DownloadFiles.\n");
		return 1;
	}
	std::string cache_name;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in DownloadFiles request\n");
		return PutErrorAd(sock, 1, "DownloadFiles", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", cache_name))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName in DownloadFiles request\n");
		return PutErrorAd(sock, 1, "DownloadFiles", "Request missing CacheName attribute");
	}

	CondorError err;
	std::string dest = GetCacheDir(cache_name, err);
	dprintf(D_FULLDEBUG, "dest = %s\n", dest.c_str());//##
	CreateCacheDirectory(cache_name, err);

	// Return the cache ad.
	compat_classad::ClassAd* cache_ad = new compat_classad::ClassAd();
	std::string my_version = CondorVersion();
	cache_ad->InsertAttr("CondorVersion", my_version);
	cache_ad->InsertAttr(ATTR_ERROR_CODE, 0);

	if (!putClassAd(sock, *cache_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
	}

	// The file transfer object is deleted automatically by the upload file 
	// handler.
	FileTransfer* ft = new FileTransfer();
	m_active_transfers.push_back(ft);
	compat_classad::ClassAd* transfer_ad = new compat_classad::ClassAd();
	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	dprintf(D_FULLDEBUG, "caching_dir = %s\n", caching_dir.c_str());//##
	transfer_ad->InsertAttr(ATTR_JOB_IWD, caching_dir);
	transfer_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, caching_dir);
	dprintf(D_FULLDEBUG, "caching_dir here is %s\n", caching_dir.c_str());
	ft->SimpleInit(transfer_ad, false, true, static_cast<ReliSock*>(sock));
	ft->setPeerVersion(version.c_str());
	ft->DownloadFiles();
	return 0;
}

int CachedServer::DownloadFiles2(int cmd, Stream * sock)
{
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for DownloadFiles.\n");
		return 1;
	}
	std::string dirname;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in DownloadFiles request\n");
		return PutErrorAd(sock, 1, "DownloadFiles", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", dirname))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName in DownloadFiles request\n");
		return PutErrorAd(sock, 1, "DownloadFiles", "Request missing CacheName attribute");
	}

	CondorError err;
	compat_classad::ClassAd *cache_ad;
	if (!GetCacheAd(dirname, cache_ad, err))
	{
		return PutErrorAd(sock, 1, "DownloadFiles", err.getFullText());
	}

	// Return the cache ad.
	std::string my_version = CondorVersion();
	cache_ad->InsertAttr("CondorVersion", my_version);
	cache_ad->InsertAttr(ATTR_ERROR_CODE, 0);

	if (!putClassAd(sock, *cache_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
	}

	compat_classad::ClassAd transfer_ad;

	// Set the files to transfer
	std::string cache_dir = GetCacheDir(dirname, err);
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, cache_dir.c_str());
	transfer_ad.InsertAttr(ATTR_JOB_IWD, cache_dir.c_str());
	MyString err_str;

	if (!FileTransfer::ExpandInputFileList(&transfer_ad, err_str)) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to expand transfer list %s: %s\n", cache_dir.c_str(), err_str.c_str());
		//PutErrorAd(sock, 1, "DownloadFiles", err_str.c_str());
	}

	std::string transfer_files;
	transfer_ad.EvaluateAttrString(ATTR_TRANSFER_INPUT_FILES, transfer_files);
	dprintf(D_FULLDEBUG, "Expanded file list: %s", transfer_files.c_str());

	// From here on out, this is the file transfer server socket.
	// TODO: Handle user permissions for the files

	// The file transfer object is deleted automatically by the upload file 
	// handler.
	FileTransfer* ft = new FileTransfer();
	ft->SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(sock));
	ft->setPeerVersion(version.c_str());
	ft->UploadFiles();
	return KEEP_STREAM;
}

int CachedServer::RemoveCacheDir(int /*cmd*/, Stream * sock)
{

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for RemoveCacheDir.\n");
		return 1;
	}
	std::string dirname;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in RemoveCacheDir request\n");
		return PutErrorAd(sock, 1, "RemoveCacheDir", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", dirname))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName in RemoveCacheDir request\n");
		return PutErrorAd(sock, 1, "RemoveCacheDir", "Request missing CacheName attribute");
	}

	CondorError err;
	compat_classad::ClassAd *cache_ad;
	if (!GetCacheAd(dirname, cache_ad, err))
	{
		return PutErrorAd(sock, 1, "RemoveCacheDir", err.getFullText());
	}



	// Make sure the authenticated user is allowed to download this cache
	std::string authenticated_user = ((Sock*)sock)->getFullyQualifiedUser();
	std::string cache_owner;
	cache_ad->EvalString(ATTR_OWNER, NULL, cache_owner);

	// TODO: Also have to allow the admin user to delete caches
	if ( authenticated_user != cache_owner ) {
		return PutErrorAd(sock, 1, "RemoveCacheDir", "Cache owner does not match authenticated owner. Client may only remove to their own cache.");
	}

	// Delete the classad and the cache directories
	if ( DoRemoveCacheDir(dirname, err) ) {
		return PutErrorAd(sock, 1, "RemoveCacheDir", err.getFullText());
	}

	// Return a success message
	compat_classad::ClassAd return_classad;
	std::string my_version = CondorVersion();
	return_classad.InsertAttr("CondorVersion", my_version);
	return_classad.InsertAttr(ATTR_ERROR_CODE, 0);

	if (!putClassAd(sock, return_classad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
	}

	dprintf(D_FULLDEBUG, "Successfully removed %s\n", dirname.c_str());
	return 0;

}

int CachedServer::UpdateLease(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
}

int CachedServer::ListCacheDirs(int /*cmd*/, Stream * sock)
{

	dprintf(D_FULLDEBUG, "In ListCacheDirs\n");
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ListCacheDirs.\n");
		return 1;
	}
	std::string dirname;
	std::string version;
	std::string requirements;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ListCacheDirs request\n");
		return PutErrorAd(sock, 1, "ListCacheDirs", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, dirname) && !request_ad.EvaluateAttrString(ATTR_REQUIREMENTS, requirements))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName or Requirements in ListCacheDirs request\n");
		return PutErrorAd(sock, 1, "ListCacheDirs", "Request missing CacheName or Requirements attribute");
	}

	std::list<compat_classad::ClassAd> cache_ads;
	// If they provided the cache name, then get that
	if (!dirname.empty()) {
		CondorError err;
		compat_classad::ClassAd * cache_ad;
		dprintf(D_FULLDEBUG, "Checking for cache with name = %s\n", dirname.c_str());
		if (!GetCacheAd(dirname, cache_ad, err)) {
			return PutErrorAd(sock, 1, "ListCacheDirs", err.getFullText());
		}

		cache_ads.push_back(*cache_ad);	

	} else if (!requirements.empty()) {
		// Ok, now we have a requirements expression
		dprintf(D_FULLDEBUG, "Checking for cache with requirements = %s\n", requirements.c_str());
		cache_ads = QueryCacheLog(requirements);

	}

	dprintf(D_FULLDEBUG, "Returning %i cache ads\n", cache_ads.size());

	compat_classad::ClassAd final_ad;
	final_ad.Assign("FinalAd", true);


	for (std::list<compat_classad::ClassAd>::iterator it = cache_ads.begin(); it != cache_ads.end(); it++) {

		if (!putClassAd(sock, *it) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			break;
		}

	}


	if (!putClassAd(sock, final_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
	}




	return 0;
}

int CachedServer::ListCacheDs(int /*cmd*/, Stream *sock)
{
	dprintf(D_FULLDEBUG, "In ListCacheDs\n");
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ListCacheDirs.\n");
		return 1;
	}
	std::string version;
	std::string requirements;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ListCacheDirs request\n");
		return PutErrorAd(sock, 1, "ListCacheDirs", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_REQUIREMENTS, requirements))
	{
		dprintf(D_FULLDEBUG, "Client did not include Requirements in ListCacheDirs request\n");
		return PutErrorAd(sock, 1, "ListCacheDirs", "Request missing CacheName or Requirements attribute");
	}

	dprintf(D_FULLDEBUG, "Querying for local daemons.\n");
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	// Make sure it's a cache server
	query.addANDConstraint("CachedServer =?= TRUE");
	if(!requirements.empty()) {
		query.addANDConstraint(requirements.c_str());
	}

	ClassAdList cached_ads;
	QueryResult result = collectors->query(query, cached_ads, NULL);
	dprintf(D_FULLDEBUG, "Got %i ads from query for total CacheDs in cluster\n", cached_ads.Length());

	compat_classad::ClassAd final_ad;
	final_ad.Assign("FinalAd", true);

	compat_classad::ClassAd *ad;
	cached_ads.Open();
	while ((ad = cached_ads.Next())) {
		if (!putClassAd(sock, *ad) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			dprintf(D_FULLDEBUG, "Can't put CacheD ClassAd to socket.\n");
			break;
		}
	}

	if (!putClassAd(sock, final_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "Can't put final ClassAd to socket.\n");
	}
	dprintf(D_FULLDEBUG, "Finish CachedServer::ListCacheDs()\n");

	return 0;
}

int CachedServer::ListFilesByPath(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
}

int CachedServer::CheckConsistency(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
}

int CachedServer::SetReplicationPolicy(int /*cmd*/, Stream * sock)
{
	dprintf(D_FULLDEBUG, "In SetReplicationPolicy\n");

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for SetReplicationPolicy.\n");
		return 1;
	}
	std::string dirname;
	std::string version;
	std::string replication_policy;
	std::string replication_methods;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in SetReplicationPolicy request\n");
		return PutErrorAd(sock, 1, "SetReplicationPolicy", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, dirname))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName in SetReplicationPolicy request\n");
		return PutErrorAd(sock, 1, "SetReplicationPolicy", "Request missing CacheName attribute");
	}

	// See if the cache actually exists
	CondorError err;
	compat_classad::ClassAd *cache_ad;
	if (!GetCacheAd(dirname, cache_ad, err))
	{
		return PutErrorAd(sock, 1, "DownloadFiles", err.getFullText());
	}

	if (!request_ad.EvaluateAttrString(ATTR_CACHE_REPLICATION_POLICY, replication_policy))
	{
		dprintf(D_FULLDEBUG, "Client did not include ReplicationPolicy in SetReplicationPolicy request\n");
		return PutErrorAd(sock, 1, "SetReplicationPolicy", "Request missing ReplicationPolicy attribute");
	}

	if (!request_ad.EvaluateAttrString(ATTR_CACHE_REPLICATION_METHODS, replication_methods))
	{
		dprintf(D_FULLDEBUG, "Client did not include ReplicationMethods in SetReplicationPolicy request\n");
	}

	classad::ClassAdParser	parser;
	ExprTree	*tree;

	if ( !( tree = parser.ParseExpression(replication_policy.c_str()) )) {
		return PutErrorAd(sock, 1, "SetReplicationPolicy", "Unable to parse replication policy");
	}

	// Set the requirements attribute
	m_log->BeginTransaction();
	SetAttributeString(dirname.c_str(), ATTR_REQUIREMENTS, replication_policy.c_str());
	m_log->CommitTransaction();

	if (replication_methods.size() != 0) {
		// Make sure the replication methods are quoted
		if (replication_methods.at(0) != '\"') {
			replication_methods.insert(0, "\"");
		}

		if (replication_methods.at(replication_methods.length()-1) != '\"') {
			replication_methods.append("\"");
		}
		m_log->BeginTransaction();
		SetAttributeString(dirname.c_str(), ATTR_CACHE_REPLICATION_METHODS, replication_methods.c_str());
		m_log->CommitTransaction();
	}

	dprintf(D_FULLDEBUG, "Set replication policy for %s to %s\n", dirname.c_str(), replication_policy.c_str());

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_CACHE_NAME, dirname);
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to write CreateCacheDir response to client.\n");
	}

	return 0;
}

int CachedServer::GetReplicationPolicy(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
}


/** 
 * Receive a request to create replicas from a source.
 * Protocol as follows:
 * 1. Send cache classads that should be replicated to this node
 * 2. Send a final classad with the peer ad, in order to contact, with the special
 *		 attribute, FinalReplicationRequest = true.
 *
 */

int CachedServer::CreateReplica(int /*cmd*/, Stream * sock)
{

	dprintf(D_FULLDEBUG, "In CreateReplica\n");

	// First, get the multiple replication requests
	compat_classad::ClassAdList replication_requests;
	compat_classad::ClassAd request_ad;
	compat_classad::ClassAd peer_ad;
	compat_classad::ClassAd my_ad = GenerateClassAd();
	// Re-create the machine's classad locally
	while(true) {

		if (!getClassAd(sock, request_ad) || !sock->end_of_message())
		{
			dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for CreateReplica.\n");
			return 1;
		}

		// Check if this request if the final
		int final_request = 0;
		if (!request_ad.EvalBool("FinalReplicationRequest", NULL, final_request) || !final_request) {
			// Not the final request, so add it to the class list

			// Only add cache ads that actually match us
			if(NegotiateCache(request_ad, my_ad)) {
				//dprintf(D_FULLDEBUG, "Cache matched cached\n");
				compat_classad::ClassAd *new_ad = (compat_classad::ClassAd*)request_ad.Copy();
				new_ad->SetParentScope(NULL);
				replication_requests.Insert(new_ad);

			} else {
				//dprintf(D_FULLDEBUG, "Cache did not match cache\n");
			}

		} else {
			// If this is the final request
			if (final_request) {
				peer_ad = request_ad;
				break;
			} else {
				dprintf(D_FULLDEBUG, "FinalReplicationRequest defined, but not true...\n");
			}
		}
		request_ad.Clear();
	}

	std::string remote_host = ((Sock*)sock)->default_peer_description();
	dprintf(D_FULLDEBUG, "Got %i replication requests from %s\n", replication_requests.Length(), remote_host.c_str());

	// Ok, done with the socket, close it

	replication_requests.Open();
	compat_classad::ClassAd* request_ptr;
	while ((request_ptr = replication_requests.Next())) {

		std::string cache_name;
		long long cache_size;
		request_ptr->LookupString(ATTR_CACHE_NAME, cache_name);
		request_ptr->EvalInteger(ATTR_DISK_USAGE, NULL, cache_size);
		CondorError err;

		compat_classad::ClassAd* test_ad;


		// Check if the cache is already here:
		if(GetCacheAd(cache_name, test_ad, err)) {
			dprintf(D_FULLDEBUG, "A remote host requested that we replicate the cache %s, but we already have one named the same, ignoring\n", cache_name.c_str());
			continue;
		}

		if (CreateCacheDirectory(cache_name, err)) {
			dprintf(D_FAILURE | D_ALWAYS, "Failed to create cache %s\n", cache_name.c_str());
			continue;
		}




		// Clean up the cache ad, and put it in the log
		m_log->BeginTransaction();
		SetAttributeBool(cache_name, ATTR_CACHE_ORIGINATOR, false);
		int state = UNCOMMITTED;
		SetAttributeInt(cache_name, ATTR_CACHE_ORIGINATOR, state);
		m_log->CommitTransaction();

		std::string magnet_uri;
		std::string replication_methods;
		param(replication_methods, "CACHE_REPLICATION_METHODS");
		std::string selected_method = NegotiateTransferMethod(*request_ptr, replication_methods);

		if (selected_method.empty()) {
			dprintf(D_FAILURE | D_ALWAYS, "No replication methods found, deleting cache\n");
			CondorError err;
			DoRemoveCacheDir(cache_name.c_str(), err);
			continue;
		}

		dprintf(D_FULLDEBUG, "Selected %s as replication method\n", selected_method.c_str());

		if (selected_method == "BITTORRENT") {
			if(DoBittorrentDownload(*request_ptr)) {
				CondorError err;
				DoRemoveCacheDir(cache_name.c_str(), err);
			}
			continue;
		}

		/*
		   The rest of this is for direct transfers
		   */

		std::string dest = GetCacheDir(cache_name, err);

		// Initiate the transfer
		DaemonAllowLocateFull new_daemon(&peer_ad, DT_GENERIC, "");
		if(!new_daemon.locate(Daemon::LOCATE_FULL)) {
			dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
			continue;
		} else {
			dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
		}

		ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
				CACHED_REPLICA_DOWNLOAD_FILES, Stream::reli_sock, 20 );

		compat_classad::ClassAd ad;
		std::string version = CondorVersion();
		ad.InsertAttr("CondorVersion", version);
		ad.InsertAttr(ATTR_CACHE_NAME, cache_name);

		if (!putClassAd(rsock, ad) || !rsock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			delete rsock;
			return 1;
		}

		// Receive the response
		ad.Clear();
		rsock->decode();
		if (!getClassAd(rsock, ad) || !rsock->end_of_message())
		{
			delete rsock;
			err.push("CACHED", 1, "Failed to get response from remote condor_cached");
			return 1;
		}
		int rc;
		if (!ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
		{
			err.push("CACHED", 2, "Remote condor_cached did not return error code");
		}

		if (rc)
		{
			std::string error_string;
			if (!ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
			{
				err.push("CACHED", rc, "Unknown error from remote condor_cached");
			}
			else
			{
				err.push("CACHED", rc, error_string.c_str());
			}
			delete rsock;
			return rc;
		}


		// We are the client, act like it.
		FileTransfer* ft = new FileTransfer();
		m_active_transfers.push_back(ft);
		compat_classad::ClassAd* cache_ad = new compat_classad::ClassAd();
		cache_ad->InsertAttr(ATTR_JOB_IWD, dest.c_str());
		cache_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, dest);

		// TODO: Enable file ownership checks
		rc = ft->SimpleInit(cache_ad, false, true, static_cast<ReliSock*>(rsock));
		if (!rc) {
			dprintf(D_ALWAYS | D_FAILURE, "Failed simple init\n");
		} else {
			dprintf(D_FULLDEBUG, "Successfully SimpleInit of filetransfer\n");
		}

		ft->setPeerVersion(version.c_str());
		UploadFilesHandler *handler = new UploadFilesHandler(*this, cache_name);
		ft->RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);

		// Restrict the amount of data that the file transfer will transfer
		dprintf(D_FULLDEBUG, "Setting max download bytes to: %lli\n", cache_size);
		ft->setMaxDownloadBytes((cache_size*1024)+4);


		rc = ft->DownloadFiles(false);
		if (!rc) {
			dprintf(D_ALWAYS | D_FAILURE, "Failed DownloadFiles\n");
		} else {
			dprintf(D_FULLDEBUG, "Successfully began downloading files\n");
			SetCacheUploadStatus(cache_name.c_str(), UPLOADING);

		}


	}


	return 0;



}

/**
 *
 * Receive a redundancy advertisement for which we are the redundancy manager
 * Protocol: 
 * 	Receive cache ClassAds (right now we only support receiving single cache
 * 	in the future, we might duplicate ReceiveCacheAdvertise and support multiple caches
 *
 */
int CachedServer::ReceiveRedundancyAdvertisement(int /* cmd */, Stream *sock) 
{
	compat_classad::ClassAd advertisement_ad;
	if (!getClassAd(sock, advertisement_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to read request for RecieveRedundancyAdvertisement.\n");
		return 1;
	}

	// Extract the attributes to access the advertisement data structure
	std::string cache_name;
	std::string cache_machine;
	if(!advertisement_ad.EvalString(ATTR_CACHE_ID, NULL, cache_name)) {
		dprintf(D_ALWAYS, "RecieveRedundancyAdvertisement: Failed to read request, no %s in advertisement\n", ATTR_CACHE_ID);
	}

	if(!advertisement_ad.EvalString("CachedServerName", NULL, cache_machine)) {
		dprintf(D_ALWAYS, "RecieveRedundancyAdvertisement: Failed to read request, no %s in advertisement\n", ATTR_MACHINE);
	}

	if(redundancy_host_map.count(cache_name) == 0) {
		redundancy_host_map[cache_name] = new string_to_time;
	}

	string_to_time* host_map;
	host_map = redundancy_host_map[cache_name];

	// TODO: how to get time in unix epoch?
	(*host_map)[cache_machine] = time(NULL);

	compat_classad::ClassAd return_ad;
	return_ad.InsertAttr("RedundancyAcknowledgement", "SUCCESS");
	if (!putClassAd(sock, return_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
	}
	dprintf(D_FULLDEBUG, "Recieved advertisement for redundancy %s from hosted at %s\n", cache_name.c_str(), cache_machine.c_str());

	return 0;
}


/**
 *
 * Receive a cache advertisement for which we are the originator
 * Protocol: 
 * 	Receive cache ClassAds (may be multiple)
 * 	Final ClassAd will be blank other than FinalAdvertisement = true
 *
 */
int CachedServer::ReceiveCacheAdvertisement(int /* cmd */, Stream *sock) 
{



	// Loop through the cache 
	while(true) {

		compat_classad::ClassAd advertisement_ad;
		if (!getClassAd(sock, advertisement_ad) || !sock->end_of_message())
		{
			dprintf(D_ALWAYS, "Failed to read request for RecieveCacheAdvertisement.\n");
			break;
		}

		int terminator = 0;
		if(advertisement_ad.EvalBool("FinalAdvertisement", NULL, terminator)) {
			if (terminator) {
				break;
			}
		}

		// Extract the attributes to access the advertisement data structure
		std::string cache_name;
		std::string cache_machine;
		if(!advertisement_ad.EvalString(ATTR_CACHE_ID, NULL, cache_name)) {
			dprintf(D_ALWAYS, "RecieveCacheAdvertisement: Failed to read request, no %s in advertisement\n", ATTR_CACHE_ID);
			continue;
		}

		if(!advertisement_ad.EvalString(ATTR_MACHINE, NULL, cache_machine)) {
			dprintf(D_ALWAYS, "RecieveCacheAdvertisement: Failed to read request, no %s in advertisement\n", ATTR_MACHINE);
			continue;
		}


		if(cache_host_map.count(cache_name) == 0) {
			cache_host_map[cache_name] = new string_to_time;
		}

		string_to_time* host_map;
		host_map = cache_host_map[cache_name];

		// TODO: how to get time in unix epoch?
		(*host_map)[cache_machine] = time(NULL);

		dprintf(D_FULLDEBUG, "Recieved advertisement for cache %s from hosted at %s\n", cache_name.c_str(), cache_machine.c_str());


	}

	return 0;

}


/**
 *	This function receives and processes local replication requests.
 *	Replication requests can be from local file transfer plugins, or children
 * cached's that this cached is serving
 *
 */

int CachedServer::ReceiveLocalReplicationRequest(int /* cmd */, Stream* sock) 
{

	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveLocalReplicationRequest\n");

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ReceiveLocalReplicationRequest.\n");
		return 1;
	}
	std::string cached_origin;
	std::string cache_name;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ORIGINATOR_HOST, cached_origin))
	{
		dprintf(D_FULLDEBUG, "Client did not include %s in ReceiveLocalReplicationRequest request\n", ATTR_CACHE_ORIGINATOR_HOST);
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheOriginatorHost attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "Client did not include %s in ReceiveLocalReplicationRequest request\n", ATTR_CACHE_NAME);
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheName attribute");
	}

	CondorError err;

	// Check if we have a record of this URL in the cache log
	compat_classad::ClassAd cache_ad;
	compat_classad::ClassAd* tmp_ad;
	if(GetCacheAd(cache_name, tmp_ad, err))
	{
		dprintf(D_FULLDEBUG, "cache exists: %s!\n", cache_name.c_str());//##
		cache_ad = *tmp_ad;
		cache_ad.InsertAttr(ATTR_CACHE_REPLICATION_STATUS, "CLASSAD_READY");
		if (!putClassAd(sock, cache_ad) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			return 1;
		}
	}
	else if (m_requested_caches.count(cache_name)) {

		dprintf(D_FULLDEBUG, "cache in memory requests: %s!\n", cache_name.c_str());//##
		// It's in the in memory requests
		cache_ad = m_requested_caches[cache_name];
		// If it's coming from memory, then it's uncommitted
		cache_ad.InsertAttr(ATTR_CACHE_STATE, UNCOMMITTED);
		if (!putClassAd(sock, cache_ad) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			return 1;
		}

	}
	else 
	{

		dprintf(D_FULLDEBUG, "cache requested: %s!\n", cache_name.c_str());//##
		// Brand new request, return that we are now looking into it.
		cache_ad.InsertAttr(ATTR_CACHE_REPLICATION_STATUS, "REQUESTED");
		cache_ad.InsertAttr(ATTR_CACHE_ORIGINATOR_HOST, cached_origin);



		//cache_ad.InsertAttr(ATTR_CACHE_PARENT_CACHED, cached_parent);

		m_requested_caches[cache_name] = cache_ad;
		if (!putClassAd(sock, cache_ad) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			return 1;
		}

		//CheckCacheReplicationStatus(cache_name, cached_origin);
	}
}

int CachedServer::ReceiveLocalReplicationRequest2(int /* cmd */, Stream* sock) 
{

	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveLocalReplicationRequest2\n");

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ReceiveLocalReplicationRequest.\n");
		return 1;
	}
	std::string cached_origin;
	std::string cache_name;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ORIGINATOR_HOST, cached_origin))
	{
		dprintf(D_FULLDEBUG, "Client did not include %s in ReceiveLocalReplicationRequest request\n", ATTR_CACHE_ORIGINATOR_HOST);
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheOriginatorHost attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "Client did not include %s in ReceiveLocalReplicationRequest request\n", ATTR_CACHE_NAME);
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheName attribute");
	}

	CondorError err;

	// Check if we have a record of this URL in the cache log
	compat_classad::ClassAd cache_ad;
	compat_classad::ClassAd* tmp_ad;
	if(GetCacheAd(cache_name, tmp_ad, err))
	{
		dprintf(D_FULLDEBUG, "cache exists: %s!\n", cache_name.c_str());//##
		cache_ad = *tmp_ad;
		cache_ad.InsertAttr(ATTR_CACHE_REPLICATION_STATUS, "CLASSAD_READY");
		if (!putClassAd(sock, cache_ad) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			return 1;
		}
	}
	else 
	{
		dprintf(D_FULLDEBUG, "cache requested: %s!\n", cache_name.c_str());//##
		// Brand new request, return that we are now looking into it.
		cache_ad.InsertAttr(ATTR_CACHE_REPLICATION_STATUS, "REQUESTED");
		cache_ad.InsertAttr(ATTR_CACHE_ORIGINATOR_HOST, cached_origin);
		cache_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
		DoDirectDownload2(cached_origin, cache_ad);

		//cache_ad.InsertAttr(ATTR_CACHE_PARENT_CACHED, cached_parent);

		if (!putClassAd(sock, cache_ad) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			return 1;
		}

		//CheckCacheReplicationStatus(cache_name, cached_origin);
	}
	return 0;
}

int CachedServer::ReceiveProcessDataTask(int /* cmd */, Stream* sock) {

	dprintf(D_FULLDEBUG, "In ReceiveProcessDataTask 1\n");

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveProcessDataTask, failed to read request for ReceiveLocalReplicationRequest.\n");
		return 1;
	}

	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "In ReceiveProcessDataTask request_ad did not include version\n");
		return 1;
	}
	if (version != CondorVersion()) {
		dprintf(D_FULLDEBUG, "In ReceiveProcessDataTask, version mismatches\n");
	}
	compat_classad::ClassAd response_ad;
	long long int storage_cost = 0;
	long long int computation_cost = 0;
	long long int network_cost = 0;
	std::string directory_path;
	// TODO: do process data task and get cost
	response_ad.InsertAttr("StorageCost", storage_cost);
	response_ad.InsertAttr("ComputationCost", computation_cost);
	response_ad.InsertAttr("NetworkCost", network_cost);
	// TODO: change it to the actuall output directory
	response_ad.InsertAttr("DirectoryPath", "/home/centos/test");

	dprintf(D_FULLDEBUG, "In ReceiveProcessDataTask 2\n");
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In ReceiveProcessDataTask, failed to receive response_ad\n");
		delete sock;
		return 1;
	}

	dprintf(D_FULLDEBUG, "In ReceiveProcessDataTask 3\n");
	return 0;
}

int CachedServer::DoProcessDataTask(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad) {

	dprintf(D_FULLDEBUG, "In DoProcessDataTask 1, cached_server = %s\n", cached_server.c_str());//##
	DaemonAllowLocateFull remote_cached(DT_CACHED, cached_server.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In DoProcessDataTask, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In DoProcessDataTask, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock;
	rsock = (ReliSock *)remote_cached.startCommand(
		CACHED_PROCESS_DATA_TASK, Stream::reli_sock, 20 );

	dprintf(D_FULLDEBUG, "In DoProcessDataTask 2\n");//##
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	dprintf(D_FULLDEBUG, "In DoProcessDataTask 3\n");//##
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		delete rsock;
		return 1;
	}

	long long int computation_cost;
	long long int storage_cost;
	long long int network_cost;
	std::string directory_path;
	if (!response_ad.EvaluateAttrInt("ComputationCost", computation_cost))
	{
		dprintf(D_FULLDEBUG, "In DoProcessDataTask response_ad did not include computation_cost\n");
		return 1;
	}
	if (!response_ad.EvaluateAttrInt("StorageCost", storage_cost))
	{
		dprintf(D_FULLDEBUG, "In DoProcessDataTask response_ad did not include storage_cost\n");
		return 1;
	}
	if (!response_ad.EvaluateAttrInt("NetworkCost", network_cost))
	{
		dprintf(D_FULLDEBUG, "In DoProcessDataTask, response_ad did not include network_cost\n");
		return 1;
	}
	if (!response_ad.EvaluateAttrString("DirectoryPath", directory_path))
	{
		dprintf(D_FULLDEBUG, "In DoProcessDataPath response_ad did not include directory_path\n");
		return 1;
	}

	dprintf(D_FULLDEBUG, "In DoProcessDataTask 4\n");//##
	return 0;
}

int CachedServer::ReceiveProbeCachedServer(int /* cmd */, Stream* sock) {
	// Get the URL from the incoming classad

	dprintf(D_FULLDEBUG, "entering CachedServer::ReceiveDistributeReplicas\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, failed to receive request_ad\n");
		return 1;
	}
	std::string version;
	double max_failure_rate;
	int time_to_failure_minutes;
	long long int cache_size;
	std::string location_constraint;
	std::string method_constraint;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, request_ad did not include version\n");
		return 1;
	}
	if(version != CondorVersion()) {
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, version mismatches\n");
	}
	if (!request_ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, request_ad did not include max_failure_rate\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("TimeToFailureMinutes", time_to_failure_minutes))
	{
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, request_ad did not include time_to_failure_minutes\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("CacheSize", cache_size))
	{
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, request_ad did not include cache_size\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("LocationConstraint", location_constraint))
	{
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, request_ad did not include location_constraint\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("MethodConstraint", method_constraint))
	{
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, request_ad did not include method_constraint\n");
		return 1;
	}

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr("CondorVersion", version);
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	response_ad.InsertAttr(ATTR_ERROR_STRING, "SUCCEEDED");

	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, failed to send response_ad\n");
		delete sock;
		return 1;
	}

	return 0;
}

int CachedServer::ProbeCachedServer(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad) {

	// Initiate the transfer

	DaemonAllowLocateFull remote_cached(DT_CACHED, cached_server.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
			CACHED_PROBE_CACHED_SERVER, Stream::reli_sock, 20 );

	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, failed to receive request_ad\n");
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, failed to send response_ad\n");
		delete rsock;
		return 1;
	}
	int rc;
	if (!response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, response_ad does not include ATTR_ERROR_CODE\n");
		return 1;
	}
	if (rc) {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, response_ad return ATTR_ERROR_CODE is 1\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, response_ad return ATTR_ERROR_CODE is 0\n");
	}

	return 0;
}

int CachedServer::EvaluateTask(compat_classad::ClassAd& cost_ad, compat_classad::ClassAd& require_ad) {

	long long int storage_cost;
	long long int computation_cost;
	long long int network_cost;
	if (!cost_ad.EvaluateAttrInt("StorageCost", storage_cost))
	{
		dprintf(D_FULLDEBUG, "In EvaluateTask, cost_ad does not include storage_cost\n");//##
		return 1;
	}
	if (!cost_ad.EvaluateAttrInt("ComputationCost", computation_cost))
	{
		dprintf(D_FULLDEBUG, "In EvaluateTask, cost_ad does not include computation_cost\n");//##
		return 1;
	}
	if (!cost_ad.EvaluateAttrInt("NetworkCost", network_cost))
	{
		dprintf(D_FULLDEBUG, "In EvaluateTask, cost_ad does not include network_cost\n");//##
		return 1;
	}

	// TODO: add real evaluation function here
	
	require_ad.InsertAttr("MaxFailureRate", 0.1);
	require_ad.InsertAttr("TimeToFailureMinutes", 25);
	require_ad.InsertAttr("CacheSize", 102400);

	return 0;
}

int CachedServer::NegotiateCacheflowManager(compat_classad::ClassAd& require_ad, compat_classad::ClassAd& return_ad) {

	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, Querying for local daemons.\n");
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	// Make sure it's a cache server
	query.addANDConstraint("CacheflowManager =?= TRUE");
	ClassAdList cmList;
	QueryResult result = collectors->query(query, cmList, NULL);
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, Got %i ads from query for total CacheDs in cluster\n", cmList.Length());

	if(cmList.Length() == 0) {
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, no cacheflow manager found");
		return 1;
	}

	ClassAd *cm;
	cmList.Open();
	
	cm = cmList.Next();
	Daemon cm_daemon(cm, DT_GENERIC, NULL);
	if(!cm_daemon.locate()) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", cm_daemon.name());
	}

	bool probe_all_done = false;
	std::vector<std::string> cached_final_list;
	std::string location_constraint;
	std::string method_constraint = "Replication";
	compat_classad::ClassAd ad;
	while(!probe_all_done) {

		std::string version = CondorVersion();
		require_ad.InsertAttr("CondorVersion", version);
		require_ad.InsertAttr("LocationConstraint", location_constraint);
		require_ad.InsertAttr("MethodConstraint", method_constraint);
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, sending CACHEFLOW_MANAGER_GET_STORAGE_POLICY to cacheflowmanager\n");//##
		ReliSock *rsock = (ReliSock *)cm_daemon.startCommand(
				CACHEFLOW_MANAGER_GET_STORAGE_POLICY, Stream::reli_sock, 20 );

		if (!putClassAd(rsock, require_ad) || !rsock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, failed to send classad to cacheflowmanager\n");//##
			delete rsock;
			return 1;
		}
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager sent to cacheflowmanager\n");//##
		// Receive the response
		location_constraint.clear();
		rsock->decode();
		CondorError err;
		if (!getClassAd(rsock, ad) || !rsock->end_of_message())
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, failed to receive classad to cacheflowmanager\n");//##
			delete rsock;
			return 1;
		}
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, got classad from cacheflowmanager\n");//##
		std::string cached_string;
		if (!ad.EvaluateAttrString("CachedCandidates", cached_string))
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, cacheflowmanager does not include CachedCandiates\n");//##
			delete rsock;
			return 1;
		}
		std::vector<std::string> cached_candidates;
		if (cached_string.empty())
		{
			delete rsock;
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, CachedCandidates is an empty string\n");//##
			return 1;
		}
		boost::split(cached_candidates, cached_string, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, CachedCandidates = %s\n", cached_string.c_str());//##
		int cached_count = 0;
		for(int i = 0; i < cached_candidates.size(); ++i) {
			if(std::find(cached_final_list.begin(), cached_final_list.end(), cached_candidates[i]) != cached_final_list.end()) {
				continue;
			}
			// Ignore local cached
			if(cached_candidates[i] == m_daemonName) {
				continue;
			}
			compat_classad::ClassAd response_ad;
			int rc = ProbeCachedServer(cached_candidates[i], require_ad, response_ad);
			if(!rc) {
				cached_final_list.push_back(cached_candidates[i]);
				cached_count++;
			}
		}
		// We need to figure out how to alter probe_all_done state, now just keep it to true
		//if(cached_count == cached_candidates.size()) {
		//	probe_all_done = true;
		//}
		probe_all_done = true;//##
		for(int i = 0; i < cached_final_list.size(); ++i) {
			location_constraint += cached_final_list[i];
			location_constraint += ",";
		}
		if(!location_constraint.empty() && location_constraint.back() == ',') {
			location_constraint.pop_back();
		}
	}
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, location_constraint = %s\n", location_constraint.c_str());//##
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, method_constraint = %s\n", method_constraint.c_str());//##
	return_ad.InsertAttr("RedundancyCandidates", location_constraint);
	return_ad.InsertAttr("RedundancyMethod", method_constraint);

//	ad.InsertAttr("RequestingCachedServer", m_daemonName);
	if(method_constraint == "Replication") {
		int data_number = cached_final_list.size();
		int parity_number = 0;
		return_ad.InsertAttr("DataNumber", data_number);
		return_ad.InsertAttr("ParityNumber", parity_number);
	}

	return 0;
}

int CachedServer::ReceiveInitializeCache(int /*cmd*/, Stream *sock)
{
	dprintf(D_FULLDEBUG, "In ReceiveInitializeCache 1");
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, failed to read request_ad\n");
		return 1;
	}

	std::string version;
	time_t lease_expiry;
	std::string cache_name;
	std::string directory_path;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_candidates;
	int data_number;
	int parity_number;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include version\n");
		return 1;
	}
	if(version != CondorVersion()) {
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, version mismatches\n");
	}
	if (!request_ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include lease_expiry\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include cache_name\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("DirectoryPath", directory_path))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include directory_path\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_source\n");
		return 1;
	}
	if(redundancy_source != m_daemonName) {
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, redundancy_source mismatches\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_manager\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include data_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include parity_number\n");
		return 1;
	}
	time_t now = time(NULL);
	time_t lease_lifetime = lease_expiry - now;
	if (lease_lifetime < 0)
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, lease expired already\n");
		return 1;
	}
	time_t max_lease_lifetime = param_integer("MAX_CACHED_LEASE", 86400);
	if (lease_lifetime > max_lease_lifetime)
	{
		lease_expiry = now + max_lease_lifetime;
	}
	dprintf(D_FULLDEBUG, "In ReceiveInitializeCache 2");

	// Insert ad into cache
	// Create a uuid for the cache
	boost::uuids::uuid u = boost::uuids::random_generator()();
	const std::string cache_id_str = boost::lexical_cast<std::string>(u);
	//long long cache_id = m_id++;
	//std::string cache_id_str = boost::lexical_cast<std::string>(cache_id);
	const std::string dirname = cache_name + "+" + cache_id_str;

	if(LinkRedundancyDirectory(directory_path, dirname)) {
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, link cache directory failed\n");
		return 1;
	}

	compat_classad::ClassAd cache_ad;
	cache_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	cache_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	cache_ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
	cache_ad.InsertAttr("RedundancySource", redundancy_source);
	cache_ad.InsertAttr("RedundancyManager", redundancy_manager);
	cache_ad.InsertAttr("RedundancyMethod", redundancy_method);
	cache_ad.InsertAttr("RedundancyCandidates", redundancy_candidates);
	cache_ad.InsertAttr("DataNumber", data_number);
	cache_ad.InsertAttr("ParityNumber", parity_number);
	std::string authenticated_user = ((Sock *)sock)->getFullyQualifiedUser();
	cache_ad.InsertAttr(ATTR_OWNER, authenticated_user);

	// add new attribute RedundancyID, this piece of data is id 0 because it is just initialized
//	cache_ad.InsertAttr("RedundancyID", 0);
	int rc = CommitCache(cache_ad);
	if(rc){
		dprintf(D_FULLDEBUG, "In ReiceiveInitializeCache, commit cache failed\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In ReceiveInitializeCache 3");

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	response_ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
	response_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	response_ad.InsertAttr(ATTR_OWNER, authenticated_user);
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, failed to send response\n");
	}
	dprintf(D_FULLDEBUG, "In ReceiveInitializeCache 4");//##

	return 0;
}

int CachedServer::InitializeCache(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad) {

	// Initiate the transfer
	DaemonAllowLocateFull remote_cached(DT_CACHED, cached_server.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In InitializeCache, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In InitializeCache, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
			CACHED_INITIALIZE_CACHE, Stream::reli_sock, 20 );

	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In InitializeCache, failed to send request_ad\n");
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In InitializeCache, failed to receive response_ad\n");
		delete rsock;
		return 1;
	}

	return 0;
}

int CachedServer::GetRedundancyAd(const std::string& dirname, compat_classad::ClassAd*& ad_ptr)
{
	dprintf(D_FULLDEBUG, "In GetRedundancyAd, entering\n");//##
	if (m_log->table.lookup(dirname.c_str(), ad_ptr) < 0)
	{
		dprintf(D_FULLDEBUG, "In GetRedundancyAd, cache ad not found!\n");//##
		return 0;
	}
	dprintf(D_FULLDEBUG, "In GetRedundancyAd, cache ad did found!\n");//##
	return 1;
}

int CachedServer::DownloadRedundancy(int cmd, Stream * sock)
{
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, failed to read request for DownloadFiles\n");
		return 1;
	}

	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include condor version.\n");
		return 1;
	}
	if(version != CondorVersion()) {
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, condor version mismatches\n");
	}

	time_t lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string cache_owner;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_candidates;
	int data_number;
	int parity_number;
	if (!request_ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include lease_expiry\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include cache_name\n");
		return 1;
	}

	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include cache_id_str\n");
		return 1;
	}

	if (!request_ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include cache_owner\n");
		return 1;
	}

	if (!request_ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_source\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_manager\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include data_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include parity_number\n");
		return 1;
	}

	std::string dirname = cache_name + "+" + cache_id_str;

	compat_classad::ClassAd* response_ad_ptr;
	if (!GetRedundancyAd(dirname, response_ad_ptr))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, GetRedundancyAd failed, dirname = %s\n", dirname.c_str());
		return 1;
	}

	// Return the cache ad.
	response_ad_ptr->InsertAttr("CondorVersion", version);
	response_ad_ptr->InsertAttr(ATTR_ERROR_CODE, 0);

	if (!putClassAd(sock, *response_ad_ptr) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
	}

	compat_classad::ClassAd transfer_ad;

	// Set the files to transfer
	std::string directory = GetTransferRedundancyDirectory(dirname);
	dprintf(D_FULLDEBUG, "In DownloadRedundancy, directory = %s\n", directory.c_str());//##
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, directory.c_str());
	transfer_ad.InsertAttr(ATTR_JOB_IWD, directory.c_str());
	MyString err_str;
	int rc;
	rc = FileTransfer::ExpandInputFileList(&transfer_ad, err_str);
	dprintf(D_FULLDEBUG, "In DownloadRedundancy, printing transfer_ad\n");//##
	dPrintAd(D_FULLDEBUG, transfer_ad);//##
	if (!rc) {
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, failed to expand transfer list %s: %s\n", directory.c_str(), err_str.c_str());
		return 1;
	}

	std::string transfer_files;
	transfer_ad.EvaluateAttrString(ATTR_TRANSFER_INPUT_FILES, transfer_files);
	dprintf(D_FULLDEBUG, "In DownloadRedundancy, expanded file list: %s", transfer_files.c_str());

	// From here on out, this is the file transfer server socket.
	// TODO: Handle user permissions for the files

	// The file transfer object is deleted automatically by the upload file 
	// handler.
	FileTransfer* ft = new FileTransfer();
	ft->SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(sock));
	ft->setPeerVersion(version.c_str());
	ft->UploadFiles();
	return KEEP_STREAM;
}

int CachedServer::LinkRedundancyDirectory(const std::string& source, const std::string& destination) {
	dprintf(D_FULLDEBUG, "In LinkRedundancyDirectory 0, source = %s, destination = %s\n", source.c_str(), destination.c_str());
	std::string caching_dir = GetRedundancyDirectory(destination);
	dprintf(D_FULLDEBUG, "In LinkRedundancyDirectory 1, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	boost::filesystem::path src{source};
	dprintf(D_FULLDEBUG, "In LinkRedundancyDirectory 2, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	boost::filesystem::path dst{destination};
	dprintf(D_FULLDEBUG, "In LinkRedundancyDirectory 3, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	if(symlink(source.c_str(), caching_dir.c_str())) {
		dprintf(D_FULLDEBUG, "In LinkRedundancyDirectory, failed to link files %s to %s\n", source.c_str(), caching_dir.c_str());
		return 1;
	}
	dprintf(D_FULLDEBUG, "In LinkRedundancyDirectory 4, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	return 0;
}

int CachedServer::CreateRedundancyDirectory(const std::string &dirname) {

	// Create the directory
	// 1. Get the caching directory from the condor configuration
	std::string caching_dir = GetRedundancyDirectory(dirname);

	// 3. Create the caching directory
	if ( !mkdir_and_parents_if_needed(caching_dir.c_str(), S_IRWXU, PRIV_CONDOR) ) {
		dprintf( D_FULLDEBUG, "In CreateCacheDirectory, couldn't create caching dir %s\n", caching_dir.c_str());
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In CreateCacheDirectory, creating caching directory %s\n", caching_dir.c_str());
	}
	return 0;
}

std::string CachedServer::GetTransferRedundancyDirectory(const std::string &dirname) {

	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	dprintf(D_FULLDEBUG, "Caching directory is set to: %s\n", caching_dir.c_str());

	// 2. Combine the system configured caching directory with the user specified
	// 	 directory.
	// TODO: sanity check the dirname, ie, no ../...
	if(caching_dir[caching_dir.length()-1] != '/') {
		caching_dir += "/";
	}
	caching_dir += dirname;
	caching_dir += "/";

	return caching_dir;
}

std::string CachedServer::GetRedundancyDirectory(const std::string &dirname) {

	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	dprintf(D_FULLDEBUG, "Caching directory is set to: %s\n", caching_dir.c_str());

	// 2. Combine the system configured caching directory with the user specified
	// 	 directory.
	// TODO: sanity check the dirname, ie, no ../...
	if(caching_dir[caching_dir.length()-1] != '/') {
		caching_dir += "/";
	}
	caching_dir += dirname;

	return caching_dir;
}

int CachedServer::RequestRedundancy(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad) {
	// Initiate the transfer
	DaemonAllowLocateFull remote_cached(DT_CACHED, cached_server.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In RequestRedundancy, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In RequestRedundancy, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
			CACHED_REQUEST_REDUNDANCY, Stream::reli_sock, 20 );

	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In RequestRedundancy, failed to send send_ad to remote cached\n");
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In RequestRedundancy, failed to receive receive_ad from remote cached\n");
		delete rsock;
		return 1;
	}
	return 0;
}

int CachedServer::ReceiveRequestRedundancy(int /* cmd */, Stream* sock) {
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, entering ReceiveRequestRedundancy\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to read request for ReceiveRequestRedundancy.\n");
		return 1;
	}

	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include condor version.\n");
		return 1;
	}
	if(version != CondorVersion()) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, condor version mismatches\n");
	}

	time_t lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string cache_owner;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_candidates;
	int data_number;
	int parity_number;
	if (!request_ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include lease_expiry\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include cache_name\n");
		return 1;
	}

	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include cache_id_str\n");
		return 1;
	}

	if (!request_ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include cache_owner\n");
		return 1;
	}

	if (!request_ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_source\n");
		return 1;
	}
	if(redundancy_source == m_daemonName) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, redundancy_source is daemon itself\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_manager\n");
		return 1;
	}

	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include data_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include parity_number\n");
		return 1;
	}

	// Initiate the transfer
	DaemonAllowLocateFull remote_cached(DT_CACHED, redundancy_source.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
			CACHED_DOWNLOAD_REDUNDANCY, Stream::reli_sock, 20 );

	compat_classad::ClassAd send_ad = request_ad;
	if (!putClassAd(rsock, send_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to send send_ad to remote cached\n");
		delete rsock;
		return 1;
	}

	// Receive the response
	compat_classad::ClassAd receive_ad;
	rsock->decode();
	if (!getClassAd(rsock, receive_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to receive receive_ad from remote cached\n");
		delete rsock;
		return 1;
	}

	std::string dirname = cache_name + "+" + cache_id_str;
	std::string directory = GetRedundancyDirectory(dirname);
	dprintf(D_FULLDEBUG, "directory = %s\n", directory.c_str());
	int rc;
	rc = CreateRedundancyDirectory(dirname);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to create redundancy directory\n");
		return 1;
	}

	// We are the client, act like it.
	FileTransfer* ft = new FileTransfer();
	m_active_transfers.push_back(ft);
	compat_classad::ClassAd* transfer_ad = new compat_classad::ClassAd();
	transfer_ad->InsertAttr(ATTR_JOB_IWD, directory);
	transfer_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, directory);
	dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, directory here is %s\n", directory.c_str());

	// TODO: Enable file ownership checks
	rc = ft->SimpleInit(transfer_ad, false, true, static_cast<ReliSock*>(rsock));
	if (!rc) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed simple init\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, successfully SimpleInit of filetransfer\n");
	}

	ft->setPeerVersion(version.c_str());

	rc = ft->DownloadFiles();
	if (!rc) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed DownloadFiles\n");
		delete rsock;
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, successfully began downloading files\n");
	}

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	response_ad.InsertAttr(ATTR_ERROR_STRING, "SUCCEEDED");
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to send response_ad to remote cached\n");
		return 1;
	}

	return 0;
}

//int CachedServer::DownloadBetweenCached(const std::string cached_source, const std::string cached_destination, const std::string cache_name, const std::string cache_id_str, const std::vector<std::string> transfer_files) {
int CachedServer::DownloadBetweenCached(std::string cached_server, compat_classad::ClassAd& ad) {

	dprintf(D_ALWAYS, "1 In UploadFilesToRemoteCache!\n");//##
	std::string cached_destination = cached_server;

	// Initiate the transfer
	DaemonAllowLocateFull new_daemon(DT_CACHED, cached_destination.c_str());
	if(!new_daemon.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
	}
	dprintf(D_ALWAYS, "2 In UploadFilesToRemoteCache!\n");//##

	ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
				CACHED_REQUEST_REDUNDANCY, Stream::reli_sock, 20 );

	CondorError err;
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "3 In UploadFilesToRemoteCache!\n");//##
/*
	filesize_t transfer_size = 0;
	for (std::vector<std::string>::const_iterator it = transfer_files.begin(); it != transfer_files.end(); it++) {
		if (IsDirectory(it->c_str())) {
			Directory dir(it->c_str(), PRIV_USER);
			transfer_size += dir.GetDirectorySize();
		} else {
			StatInfo info(it->c_str());
			transfer_size += info.GetFileSize();
		}

	}
	
	dprintf(D_FULLDEBUG, "Transfer size = %lli\n", transfer_size);

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr(ATTR_DISK_USAGE, transfer_size);
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
*/
	dprintf(D_ALWAYS, "4 In UploadFilesToRemoteCache!\n");//##
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "5 In UploadFilesToRemoteCache!\n");//##

	compat_classad::ClassAd response_ad;
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}

	dprintf(D_ALWAYS, "6 In UploadFilesToRemoteCache!\n");//##
	int rc;
	if (!response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}

	if (rc)
	{
		std::string error_string;
		if (!response_ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
		delete rsock;
		return rc;
	}
/*
	dprintf(D_ALWAYS, "7 In UploadFilesToRemoteCache!\n");//##

	compat_classad::ClassAd transfer_ad;
	transfer_ad.InsertAttr("CondorVersion", version);

	// Expand the files list and add to the classad
	StringList input_files;
	for (std::vector<std::string>::const_iterator it = transfer_files.begin(); it != transfer_files.end(); it++) {
		input_files.insert((*it).c_str());
	}
	char* filelist = input_files.print_to_string();
	dprintf(D_FULLDEBUG, "Transfer list = %s\n", filelist);
	dprintf(D_ALWAYS, "Transfer list = %s\n", filelist);//##
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, filelist);
	char current_dir[PATH_MAX];
	getcwd(current_dir, PATH_MAX);
	transfer_ad.InsertAttr(ATTR_JOB_IWD, current_dir);
	dprintf(D_FULLDEBUG, "IWD = %s\n", current_dir);
	free(filelist);

	dprintf(D_ALWAYS, "8 In UploadFilesToRemoteCache!\n");//##
	// From here on out, this is the file transfer server socket.
	FileTransfer ft;
  	rc = ft.SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(rsock));
	dprintf(D_ALWAYS, "9 rc = %d\n", rc);//##
	if (!rc) {
		dprintf(D_ALWAYS, "Simple init failed\n");
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "9 In UploadFilesToRemoteCache!\n");//##
	ft.setPeerVersion(version.c_str());
	//UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	//ft.RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
	rc = ft.UploadFiles(true);

	if (!rc) {
		delete rsock;
		dprintf(D_ALWAYS, "Upload files failed.\n");
		return 1;
	}
	
	dprintf(D_ALWAYS, "10 In UploadFilesToRemoteCache!\n");//##
*/
	delete rsock;
	return 0;
}

int CachedServer::CreateRemoteCacheRedundancy(std::string cached_server, compat_classad::ClassAd& ad) {

	// Initiate the transfer
	DaemonAllowLocateFull new_daemon(DT_CACHED, cached_server.c_str());
	if(!new_daemon.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 2;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
	}
	dprintf(D_FULLDEBUG, "2 In CreateRemoteCacheDir!\n");//##

	ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
					CACHED_CREATE_CACHE_DIR2, Stream::reli_sock, 20 );
	if (!rsock)
	{
		dprintf(D_FULLDEBUG, "No expiry defined");
		return 1;
	}
	dprintf(D_FULLDEBUG, "3 In CreateRemoteCacheDir!\n");//##

	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("RequestingCachedServer", m_daemonName);
	dprintf(D_FULLDEBUG, "In CreateRemoteCacheRedundancy, printing ad\n");//##
	dPrintAd(D_FULLDEBUG, ad);//##
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		dprintf(D_FULLDEBUG, "Does not put classad");
		return 1;
	}
	dprintf(D_FULLDEBUG, "4 In CreateRemoteCacheDir!\n");//##

	compat_classad::ClassAd response_ad;
	
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		delete rsock;
		dprintf(D_FULLDEBUG, "Does not get classad");
		return 1;
	}
	dprintf(D_FULLDEBUG, "5 In CreateRemoteCacheDir!\n");//##

	rsock->close();
	delete rsock;

	int rc;
	if (!response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		dprintf(D_FULLDEBUG, "No error code defined");
		return 1;
	}
	dprintf(D_FULLDEBUG, "6 In CreateRemoteCacheDir!\n");//##

	if (rc)
	{
		std::string error_string;
		if (!response_ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			dprintf(D_FULLDEBUG, "No error_string defined");
			return 1;
		}
	}
	dprintf(D_FULLDEBUG, "7 In CreateRemoteCacheDir!\n");//##

	return 0;
}

int CachedServer::AskRemoteCachedDownload(std::string cached_server, compat_classad::ClassAd& ad) {
	int rc = -1;
	dprintf(D_FULLDEBUG, "In AskRemoteCachedDownload, printing ad\n");//##
	dPrintAd(D_FULLDEBUG, ad);//##
	rc = CreateRemoteCacheRedundancy(cached_server, ad);
	if(rc) {
		return 1;
	}
	rc = DownloadBetweenCached(cached_server, ad);
	if(rc) {
		return 1;
	}
	return 0;
}

int CachedServer::DistributeRedundancy(compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad)
{
	std::string redundancy_candidates;
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, class ad does not include redundancy_candidates\n");
		return 1;
	}
	std::vector<std::string> v;
	if (redundancy_candidates.empty())
	{
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, redundancy_candidates is an empty string\n");
		return 1;
	}
	boost::split(v, redundancy_candidates, boost::is_any_of(", "));
	dprintf(D_FULLDEBUG, "In DistributeRedundancy, redundancy_candidates is %s\n", redundancy_candidates.c_str());

	int rc = 0;
	for(int i = 0; i < redundancy_candidates.size(); ++i) {
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, cached server is %s\n", v[i].c_str());
		const std::string cached_server = v[i];
		compat_classad::ClassAd send_ad = request_ad;
		compat_classad::ClassAd receive_ad;
		rc = RequestRedundancy(cached_server, send_ad, receive_ad);
		if(rc) {
			dprintf(D_FULLDEBUG, "In DistributeRedundancy, RequestRedundancy failed for %s\n", cached_server.c_str());
		}
	}
	
	return 0;
}

int CachedServer::CheckRedundancyStatus(compat_classad::ClassAd& ad) {
	int rc = CommitCache(ad);
	if(rc) {
		dprintf(D_FULLDEBUG, "CommitCache failed\n");
		return 1;
	}
	return 0;
}

int CachedServer::CommitCache(compat_classad::ClassAd& ad) {

	long long int lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_candidates;
	int data_number;
	int parity_number;
	std::string cache_owner;
	int redundancy_id;

	if (!ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include lease_expiry");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include cache_name");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include cache_id_str");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_source");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_manager");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_method");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_candidates");
		return 1;
	}
	if (!ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include data_number");
		return 1;
	}
	if (!ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include parity_number");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include cache_owner");
		return 1;
	}
//	if (!ad.EvaluateAttrInt("RedundancyID", redundancy_id))
//	{
//		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_id");
//		return 1;
//	}

	std::string dirname = cache_name + "+" + cache_id_str;
	m_log->BeginTransaction();
	SetAttributeLong(dirname, ATTR_LEASE_EXPIRATION, lease_expiry);
	SetAttributeString(dirname, ATTR_CACHE_NAME, cache_name);
	SetAttributeString(dirname, ATTR_CACHE_ID, cache_id_str);
	SetAttributeString(dirname, "RedundancySource", redundancy_source);
	SetAttributeString(dirname, "RedundancyManager", redundancy_manager);
	SetAttributeString(dirname, "RedundancyMethod", redundancy_method);
	SetAttributeString(dirname, "RedundancyCandidates", redundancy_candidates);
	SetAttributeInt(dirname, "DataNumber", data_number);
	SetAttributeInt(dirname, "ParityNumber", parity_number);
	SetAttributeString(dirname, ATTR_OWNER, cache_owner);
//	SetAttributeInt(dirname, "RedundancyID", redundancy_id);
	if(redundancy_source == m_daemonName) {
		SetAttributeBool(dirname, "IsRedundancySource", true);
	} else {
		SetAttributeBool(dirname, "IsRedundancySource", false);
	}
	if(redundancy_manager == m_daemonName) {
		SetAttributeBool(dirname, "IsRedundancyManager", true);
	} else {
		SetAttributeBool(dirname, "IsRedundancyManager", false);
	}
	int state = COMMITTED;
	SetAttributeInt(dirname, ATTR_CACHE_STATE, state);
	m_log->CommitTransaction();

	return 0;
}

int CachedServer::ProcessTask(int /* cmd */, Stream* sock) 
{
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ProcessTask 1\n");

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ProcessTask.\n");
		return 1;
	}

	std::string task_type;
	std::string cached_server;
	std::string cache_name;
	if (!request_ad.EvaluateAttrString("TaskType", task_type))
	{
		dprintf(D_FULLDEBUG, "Client did not include task_type\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("CachedServerName", cached_server))
	{
		dprintf(D_FULLDEBUG, "Client did not include cached_server\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "Client did not include cache_name\n");
		return 1;
	}

	dprintf(D_FULLDEBUG, "In ProcessTask 2\n");

	std::string version = CondorVersion();

	// Step 1, schedule task, task_ad -> cost_ad
	compat_classad::ClassAd task_ad;
	task_ad.InsertAttr("CondorVersion", version);
	compat_classad::ClassAd cost_ad;
	int rc = -1;
	if(task_type == "DataTask") {
		rc = DoProcessDataTask(cached_server, task_ad, cost_ad);
	}
	if(rc) {
		dprintf(D_FULLDEBUG, "In ProcessTask, DoProcessDataTask failed\n");
		return 1;
	}

	long long int computation_cost;
	long long int storage_cost;
	long long int network_cost;
	std::string directory_path;
	if (!cost_ad.EvaluateAttrInt("ComputationCost", computation_cost))
	{
		dprintf(D_FULLDEBUG, "cost_ad did not include computation_cost\n");
		return 1;
	}
	if (!cost_ad.EvaluateAttrInt("StorageCost", storage_cost))
	{
		dprintf(D_FULLDEBUG, "cost_ad did not include storage_cost\n");
		return 1;
	}
	if (!cost_ad.EvaluateAttrInt("NetworkCost", network_cost))
	{
		dprintf(D_FULLDEBUG, "cost_ad did not include network_cost\n");
		return 1;
	}
	if (!cost_ad.EvaluateAttrString("DirectoryPath", directory_path))
	{
		dprintf(D_FULLDEBUG, "cost_ad did not include directory_path\n");
		return 1;
	}

	// Step2, evaluate cost_ad, local function, cost_ad -> require_ad
	compat_classad::ClassAd require_ad;
	rc = EvaluateTask(cost_ad, require_ad);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ProcessTask, EvaluateTask failed\n");
		return 1;
	}

	double max_failure_rate;
	long long int time_to_failure_minutes;
	long long int cache_size;
	if (!require_ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "require_ad did not include max_failure_rate\n");
		return 1;
	}
	if (!require_ad.EvaluateAttrInt("TimeToFailureMinutes", time_to_failure_minutes))
	{
		dprintf(D_FULLDEBUG, "require_ad did not include time_to_failure_minutes\n");
		return 1;
	}
	if (!require_ad.EvaluateAttrInt("CacheSize", cache_size))
	{
		dprintf(D_FULLDEBUG, "require_ad did not include cache_size\n");
		return 1;
	}

	// Step 3, negotiate cache
	dprintf(D_FULLDEBUG, "In ProcessTask 3\n");
	require_ad.InsertAttr("CondorVersion", version);
	compat_classad::ClassAd policy_ad;
	rc = NegotiateCacheflowManager(require_ad, policy_ad);
	dprintf(D_FULLDEBUG, "In ProcessTask, printing policy_ad\n");//##
	dPrintAd(D_FULLDEBUG, policy_ad);//##
	if(rc) {
		dprintf(D_FULLDEBUG, "In ProcessTask, NegotiateCacheflowManager failed\n");
		return 1;
	}
	std::string redundancy_candidates;
	std::string redundancy_method;
	int data_number;
	int parity_number;
	if (!policy_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include redundancy_candidates\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include redundancy_method\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include data_number\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include parity_number\n");
		return 1;
	}

	// Step 4, initialize cache
	compat_classad::ClassAd cache_request_ad;
	time_t lease_expiry = time(NULL) + time_to_failure_minutes * 60;
	// cache_name,version,directory_path have already gotten before
	cache_request_ad.InsertAttr("CondorVersion", version);
	cache_request_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	cache_request_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	cache_request_ad.InsertAttr("DirectoryPath", directory_path);
	cache_request_ad.InsertAttr("RedundancySource", cached_server);
	cache_request_ad.InsertAttr("RedundancyManager", m_daemonName);
	cache_request_ad.InsertAttr("RedundancyMethod", redundancy_method);
	cache_request_ad.InsertAttr("RedundancyCandidates", redundancy_candidates);
	cache_request_ad.InsertAttr("DataNumber", data_number);
	cache_request_ad.InsertAttr("ParityNumber", parity_number);

	compat_classad::ClassAd cache_response_ad;
	rc = InitializeCache(cached_server, cache_request_ad, cache_response_ad);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ProcessTask, InitializeCache failed\n");
		return 1;
	}
	
	std::string return_cache_name;
	std::string cache_id_str;
	time_t new_lease_expiry;
	std::string cache_owner;
	if (!cache_response_ad.EvaluateAttrString(ATTR_CACHE_NAME, return_cache_name))
	{
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include cache_name\n");
		return 1;
	}
	if (!cache_response_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include cache_id_str\n");
		return 1;
	}
	if (!cache_response_ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, new_lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include new_lease_expiry\n");
		return 1;
	}
	if (!cache_response_ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include cache_owner\n");
		return 1;
	}

	// Step 5, distribute cache to other candidates distribute_ad -> succeeded or failued
	dprintf(D_FULLDEBUG, "In ProcessTask 4\n");
	compat_classad::ClassAd distribute_ad;
	distribute_ad.InsertAttr("CondorVersion", version);
	distribute_ad.InsertAttr(ATTR_LEASE_EXPIRATION, new_lease_expiry);
	distribute_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	distribute_ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
	distribute_ad.InsertAttr(ATTR_OWNER, cache_owner);
	distribute_ad.InsertAttr("RedundancySource", cached_server);
	distribute_ad.InsertAttr("RedundancyManager", m_daemonName);
	distribute_ad.InsertAttr("RedundancyMethod", redundancy_method);
	distribute_ad.InsertAttr("RedundancyCandidates", redundancy_candidates);
	distribute_ad.InsertAttr("DataNumber", data_number);
	distribute_ad.InsertAttr("ParityNumber", parity_number);
	compat_classad::ClassAd cache_info;
	rc = DistributeRedundancy(distribute_ad, cache_info);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ProcessTask, DistributeRedundancy failed\n");
		return 1;
	}

	// Step 6, Check distributing redundancy status
	dprintf(D_FULLDEBUG, "In ProcessTask 5\n");
	rc = CheckRedundancyStatus(cache_info);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ProcessTask, CheckRedundancy failed\n");
		return 1;
	}

	// Step 7, response to the caller
	dprintf(D_FULLDEBUG, "In ProcessTask 6\n");
	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete sock;
		dprintf(D_FULLDEBUG, "In ProcessTask, sending response failed\n");
		return 1;
	}

	delete sock;
	return 0;
}

/**
 *	Check the replication status of nodes, and update the m_requested_caches object
 *
 */

int CachedServer::CheckCacheReplicationStatus(std::string cache_name, std::string cached_origin)
{	

	CondorError err;
	// minimum return
	compat_classad::ClassAd toReturn;
	toReturn.Assign(ATTR_CACHE_ORIGINATOR_HOST, cached_origin);
	toReturn.Assign(ATTR_CACHE_REPLICATION_STATUS, "REQUESTED");

	// TODO: determine who to send the next request to, up the chain
	dprintf(D_FULLDEBUG, "In CheckCacheReplicationStatus\n");

	counted_ptr<DCCached> client;
	std::string cached_parent;

	if (m_parent.has_parent) {
		m_parent.parent_ad->EvalString(ATTR_NAME, NULL, cached_parent);
		dprintf(D_FULLDEBUG, "Connecting to parent %s\n", cached_parent.c_str());
		client = (counted_ptr<DCCached>)(new DCCached(m_parent.parent_ad.get(), NULL));
	} else {
		dprintf(D_FULLDEBUG, "Connecting to origin (no parent): %s\n", cached_origin.c_str());
		client = (counted_ptr<DCCached>)(new DCCached(cached_origin.c_str(), NULL));
		cached_parent = cached_origin;
	}


	compat_classad::ClassAd upstream_response;
	int rc = client->requestLocalCache(cached_origin, cache_name, upstream_response, err);
	if (rc) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to request parent cache %s with return %i: %s\n", cached_parent.c_str(), rc, err.getFullText().c_str());
		counted_ptr<compat_classad::ClassAd> parent;
		// Add this parent to the list of bad parents
		m_failed_parents[cached_parent] = time(NULL);
		FindParentCache(parent);
		return 1;
	}
	std::string upstream_replication_status;
	int upstream_cache_state;

	if(upstream_response.EvaluateAttrString(ATTR_CACHE_REPLICATION_STATUS, upstream_replication_status)) {
		if (upstream_replication_status == "REQUESTED") {
			// If upstream is requested as well, then do nothing
		} 
		else if (upstream_replication_status == "CLASSAD_READY") {
			m_requested_caches[cache_name] = upstream_response;

		}

	} 

	if (upstream_response.EvaluateAttrInt(ATTR_CACHE_STATE, upstream_cache_state)) {

		compat_classad::ClassAd cached_ad = GenerateClassAd();

		// Add CacheRequested so caches can be made to only be downloaded if actually
		// requested
		cached_ad.InsertAttr("CacheRequested", true);
		if(NegotiateCache(upstream_response, cached_ad)) {

			upstream_response.InsertAttr(ATTR_CACHE_REPLICATION_STATUS, "CLASSAD_READY");
			m_requested_caches[cache_name] = upstream_response;
			dprintf(D_FULLDEBUG, "Accepted Cache %s\n", cache_name.c_str());
		} else {
			upstream_response.InsertAttr(ATTR_CACHE_REPLICATION_STATUS, "DENIED");
			std::string cache_req, cached_req;
			upstream_response.EvaluateAttrString(ATTR_REQUIREMENTS, cache_req);
			cached_ad.EvaluateAttrString(ATTR_REQUIREMENTS, cached_req);

			dprintf(D_FULLDEBUG, "Denying replication of cache %s.  cache requirements: %s did not match cached requirements: %s\n", cache_name.c_str(), cache_req.c_str(), cached_req.c_str());
			m_requested_caches[cache_name] = upstream_response;
		}
	}
	return 0;

}


/**
 *	Negotiate with the cache_ad to determine if it should be cached here
 *
 *	returns: 	true - if should be cached
 *						false - if should not be cached
 */
bool CachedServer::NegotiateCache(compat_classad::ClassAd cache_ad, compat_classad::ClassAd cached_ad) {

	classad::MatchClassAd mad;
	bool match = false;

	// Only add cache ads that actually match us
	mad.ReplaceLeftAd(&cached_ad);
	mad.ReplaceRightAd(&cache_ad);
	if (mad.EvaluateAttrBool("symmetricMatch", match) && match) {
		//dprintf(D_FULLDEBUG, "Cache matched cached\n");
		mad.RemoveLeftAd();
		mad.RemoveRightAd();
		return true;

	} else {
		//dprintf(D_FULLDEBUG, "Cache did not match cache\n");
		mad.RemoveLeftAd();
		mad.RemoveRightAd();
		return false;
	}


}


/**
 *	Negotiate the transfer method to use for the cache
 *
 *
 */

std::string CachedServer::NegotiateTransferMethod(compat_classad::ClassAd cache_ad,  std::string my_replication_methods) {


	std::string selected_method, replication_methods;

	if(cache_ad.LookupString(ATTR_CACHE_REPLICATION_METHODS, replication_methods)) {
		// Check the replication methods string for how we should start the replication
		std::vector<std::string> methods;
		boost::split(methods, replication_methods, boost::is_any_of(", "));
		//std::string my_replication_methods;
		//param(my_replication_methods, "CACHE_REPLICATION_METHODS");
		std::vector<std::string> my_methods;
		boost::split(my_methods, my_replication_methods, boost::is_any_of(", "));



		// Loop through my methods, looking for matching methods on the cache
		for (std::vector<std::string>::iterator my_it = my_methods.begin(); my_it != my_methods.end(); my_it++) {
			for (std::vector<std::string>::iterator cache_it = methods.begin(); cache_it != methods.end(); cache_it++) {

				boost::to_upper(*my_it);
				boost::to_upper(*cache_it);

				if ( *my_it == *cache_it ) {
					selected_method = *my_it;
					break;
				}
			}
			if (!selected_method.empty())
				break;
		}


		if (selected_method.empty()) {
			dprintf(D_FAILURE | D_ALWAYS, "Failed to agree upon transfer method, my methods = %s, cache methods = %s\n", 
					my_replication_methods.c_str(), replication_methods.c_str());

		}

		dprintf(D_FULLDEBUG, "Selected %s as replication method\n", selected_method.c_str());
	}
	return selected_method;

}

int CachedServer::DoDirectUpload2(std::string cacheDestination, compat_classad::ClassAd cache_ad) {

//	long long cache_size;
//	cache_ad.EvalInteger(ATTR_DISK_USAGE, NULL, cache_size);
//	dprintf(D_FULLDEBUG, "cache_size = %lld\n", cache_size);//##
	std::string cache_name;
	cache_ad.LookupString(ATTR_CACHE_NAME, cache_name);
	dprintf(D_FULLDEBUG, "cache_name = %s\n", cache_name.c_str());//##

	// Initiate the transfer
	DaemonAllowLocateFull new_daemon(DT_CACHED, cacheDestination.c_str());
	if(!new_daemon.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
	}

	ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
			CACHED_REPLICA_UPLOAD_FILES2, Stream::reli_sock, 20 );

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr(ATTR_CACHE_NAME, cache_name);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}

	// Receive the response
	ad.Clear();
	rsock->decode();
	CondorError err;
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}
	int rc;
	if (!ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}

	if (rc)
	{
		std::string error_string;
		if (!ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
		delete rsock;
		return rc;
	}

	compat_classad::ClassAd transfer_ad;
	// Set the files to transfer
	std::string cache_dir = GetCacheDir(cache_name, err);
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, cache_dir.c_str());
	transfer_ad.InsertAttr(ATTR_JOB_IWD, cache_dir.c_str());
	MyString err_str;

	if (!FileTransfer::ExpandInputFileList(&transfer_ad, err_str)) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to expand transfer list %s: %s\n", cache_dir.c_str(), err_str.c_str());
		//PutErrorAd(sock, 1, "DownloadFiles", err_str.c_str());
	}

	std::string transfer_files;
	transfer_ad.EvaluateAttrString(ATTR_TRANSFER_INPUT_FILES, transfer_files);
	dprintf(D_FULLDEBUG, "Expanded file list: %s", transfer_files.c_str());

	// From here on out, this is the file transfer server socket.
	// TODO: Handle user permissions for the files

	// The file transfer object is deleted automatically by the upload file 
	// handler.
	FileTransfer* ft = new FileTransfer();
	rc = ft->SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(rsock));
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed simple init\n");
	} else {
		dprintf(D_FULLDEBUG, "Successfully SimpleInit of filetransfer\n");
	}

	ft->setPeerVersion(version.c_str());

	rc = ft->UploadFiles();
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed DownloadFiles\n");
		delete rsock;
	} else {
		dprintf(D_FULLDEBUG, "Successfully began downloading files\n");
		SetCacheUploadStatus(cache_name.c_str(), UPLOADING);

	}

	return 0;
}


int CachedServer::DoDirectDownload2(std::string cache_source, compat_classad::ClassAd cache_ad) {

	long long cache_size;
	cache_ad.EvalInteger(ATTR_DISK_USAGE, NULL, cache_size);
	dprintf(D_FULLDEBUG, "cache_size = %lld\n", cache_size);//##
	std::string cache_name;
	cache_ad.LookupString(ATTR_CACHE_NAME, cache_name);
	dprintf(D_FULLDEBUG, "cache_name = %s\n", cache_name.c_str());//##

	CondorError err;
	std::string dest = GetCacheDir(cache_name, err);
	dprintf(D_FULLDEBUG, "dest = %s\n", dest.c_str());//##
	CreateCacheDirectory(cache_name, err);

	// Initiate the transfer
	DaemonAllowLocateFull new_daemon(DT_CACHED, cache_source.c_str());
	if(!new_daemon.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
	}

	ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
			CACHED_REPLICA_DOWNLOAD_FILES2, Stream::reli_sock, 20 );

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr(ATTR_CACHE_NAME, cache_name);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}

	// Receive the response
	ad.Clear();
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}
	int rc;
	if (!ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}

	if (rc)
	{
		std::string error_string;
		if (!ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
		delete rsock;
		return rc;
	}


	// We are the client, act like it.
	FileTransfer* ft = new FileTransfer();
	m_active_transfers.push_back(ft);
	compat_classad::ClassAd* transfer_ad = new compat_classad::ClassAd();
	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	dprintf(D_FULLDEBUG, "caching_dir = %s\n", caching_dir.c_str());//##
	transfer_ad->InsertAttr(ATTR_JOB_IWD, caching_dir);
	transfer_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, caching_dir);
	dprintf(D_FULLDEBUG, "caching_dir here is %s\n", caching_dir.c_str());

	// TODO: Enable file ownership checks
	rc = ft->SimpleInit(transfer_ad, false, true, static_cast<ReliSock*>(rsock));
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed simple init\n");
	} else {
		dprintf(D_FULLDEBUG, "Successfully SimpleInit of filetransfer\n");
	}

	ft->setPeerVersion(version.c_str());

	rc = ft->DownloadFiles();
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed DownloadFiles\n");
		delete rsock;
	} else {
		dprintf(D_FULLDEBUG, "Successfully began downloading files\n");
		SetCacheUploadStatus(cache_name.c_str(), UPLOADING);

	}

	return 0;
}

int CachedServer::DoDirectDownload(std::string cache_source, compat_classad::ClassAd cache_ad) {

	long long cache_size;
	cache_ad.EvalInteger(ATTR_DISK_USAGE, NULL, cache_size);

	std::string cache_name;
	cache_ad.LookupString(ATTR_CACHE_NAME, cache_name);

	CondorError err;
	std::string dest = GetCacheDir(cache_name, err);
	CreateCacheDirectory(cache_name, err);

	// Initiate the transfer
	DaemonAllowLocateFull new_daemon(DT_CACHED, cache_source.c_str());
	if(!new_daemon.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
	}

	ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
			CACHED_REPLICA_DOWNLOAD_FILES, Stream::reli_sock, 20 );

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr(ATTR_CACHE_NAME, cache_name);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}

	// Receive the response
	ad.Clear();
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}
	int rc;
	if (!ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}

	if (rc)
	{
		std::string error_string;
		if (!ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
		delete rsock;
		return rc;
	}


	// We are the client, act like it.
	FileTransfer* ft = new FileTransfer();
	m_active_transfers.push_back(ft);
	compat_classad::ClassAd* transfer_ad = new compat_classad::ClassAd();
	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	transfer_ad->InsertAttr(ATTR_JOB_IWD, caching_dir);
	transfer_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, caching_dir);

	// TODO: Enable file ownership checks
	rc = ft->SimpleInit(transfer_ad, false, true, static_cast<ReliSock*>(rsock));
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed simple init\n");
	} else {
		dprintf(D_FULLDEBUG, "Successfully SimpleInit of filetransfer\n");
	}

	ft->setPeerVersion(version.c_str());
	UploadFilesHandler *handler = new UploadFilesHandler(*this, cache_name);
	ft->RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);

	// Restrict the amount of data that the file transfer will transfer
	dprintf(D_FULLDEBUG, "Setting max download bytes to: %lli\n", cache_size);
	ft->setMaxDownloadBytes((cache_size*1024)+4);


	rc = ft->DownloadFiles();
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed DownloadFiles\n");
		delete rsock;
	} else {
		dprintf(D_FULLDEBUG, "Successfully began downloading files\n");
		SetCacheUploadStatus(cache_name.c_str(), UPLOADING);

	}

	return 0;

}


int CachedServer::DoBittorrentDownload(compat_classad::ClassAd& cache_ad, bool initial_download) {

	std::string magnet_uri;

	if(cache_ad.LookupString(ATTR_CACHE_MAGNET_LINK, magnet_uri)) {
		// Magnet uri exists

		dprintf(D_FULLDEBUG, "Magnet URI detected: %s\n", magnet_uri.c_str());
		dprintf(D_FULLDEBUG, "Downloading through Bittorrent\n");
		std::string caching_dir;
		param(caching_dir, "CACHING_DIR");

		std::string cache_name;
		cache_ad.EvalString(ATTR_CACHE_NAME, NULL, cache_name);
		if(initial_download) {
			SetCacheUploadStatus(cache_name, UPLOADING);
		}
//		DownloadTorrent(magnet_uri, caching_dir, "");
		return 0;

	} else {

		dprintf(D_FAILURE | D_ALWAYS, "BITTORRENT was selected as the transfer method, but magnet link is not in the cache classad.  Bailing on this replication request\n");
		return 1;

	}

}


/**
 *	Return the classad for the cache dirname
 * 	returns: 	0 - not found
 *						1 - found
 */
int CachedServer::GetCacheAd(const std::string &dirname, compat_classad::ClassAd *&cache_ad, CondorError &err)
{
	dprintf(D_FULLDEBUG, "In CachedServer::GetCacheAd\n");//##
	if (m_log->table.lookup(dirname.c_str(), cache_ad) < 0)
	{
		dprintf(D_FULLDEBUG, "In CachedServer::GetCacheAd not found!\n");//##
		err.pushf("CACHED", 3, "Cache ad %s not found", dirname.c_str());
		return 0;
	}
	dprintf(D_FULLDEBUG, "In CachedServer::GetCacheAd did found!\n");//##
	return 1;
}


int CachedServer::SetCacheUploadStatus(const std::string &dirname, CACHE_STATE state)
{
	if (!m_log->AdExistsInTableOrTransaction(dirname.c_str())) { return 0; }
	// TODO: Convert this to a real state.
	m_log->BeginTransaction();
	int insert_state = state;
	SetAttributeInt(dirname.c_str(), ATTR_CACHE_STATE, insert_state);
	m_log->CommitTransaction();
	return 0;
}

/*
 * Get the current upload status
 */
CachedServer::CACHE_STATE CachedServer::GetUploadStatus(const std::string &dirname) {

	CACHE_STATE state;
	m_log->BeginTransaction();

	// Check if the cache directory even exists
	if (!m_log->AdExistsInTableOrTransaction(dirname.c_str())) { state = INVALID; }

	compat_classad::ClassAd *cache_ad;
	CondorError errorad;
	// Check the cache status
	if (GetCacheAd(dirname, cache_ad, errorad) == 0 )
		state = INVALID;

	dprintf(D_FULLDEBUG, "Caching classad:");
	compat_classad::dPrintAd(D_FULLDEBUG, *cache_ad);

	int int_state;
	if (! cache_ad->EvalInteger(ATTR_CACHE_STATE, NULL, int_state)) {
		state = INVALID;
	}
	m_log->CommitTransaction();

	return static_cast<CACHE_STATE>(int_state);
}



std::string CachedServer::ConvertIdtoDirname(const std::string cacheId) {


	std::stringstream stream;
	stream << ATTR_CACHE_ID << " == \"" << cacheId << "\"";
	std::list<compat_classad::ClassAd> caches = QueryCacheLog(stream.str());

	if (caches.size() == 0) {
		return std::string("");

	} else {

		// There should only be 1, hopefully
		compat_classad::ClassAd cache = caches.front();
		std::string cache_dirname;
		cache.LookupString(ATTR_CACHE_NAME, cache_dirname);
		return cache_dirname;

	}

}


std::list<compat_classad::ClassAd> CachedServer::QueryCacheLog(const std::string& requirement) {

	dprintf(D_FULLDEBUG, "Cache Query = %s\n", requirement.c_str());
	classad::ClassAdParser parser;
	ExprTree *tree;
//	ClassAd parse_ad;
	if (!(tree = parser.ParseExpression(requirement.c_str()))) {
		dprintf(D_FULLDEBUG, "In CachedServer::QueryCacheLog, ParseExpression failed\n");
	} else {
		dprintf(D_FULLDEBUG, "In CachedServer::QueryCacheLog, ParseExpression succeeded\n");
	}

	std::list<compat_classad::ClassAd> toReturn;
        std::string HK;
        ClassAd* ad;

        m_log->BeginTransaction();
	m_log->table.startIterations();
	while (m_log->table.iterate(HK, ad)) {
		compat_classad::ClassAd db_ad(*ad);
		if (EvalBool(ad, tree)) {
			dprintf(D_FULLDEBUG, "requirement matches ad(%s)\n", HK.c_str());
			toReturn.push_front(db_ad);
		}
	}
	m_log->CommitTransaction();
	return toReturn;
}

std::string CachedServer::GetCacheDir(const std::string &dirname, CondorError& /* err */) {

	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	dprintf(D_FULLDEBUG, "Caching directory is set to: %s\n", caching_dir.c_str());

	// 2. Combine the system configured caching directory with the user specified
	// 	 directory.
	// TODO: sanity check the dirname, ie, no ../...
	if(caching_dir[caching_dir.length()-1] != '/') {
		caching_dir += "/";
	}
	caching_dir += dirname;

	return caching_dir;

}


/**
 *	Remove the cache dir, both the classad in the log and the directories on disk.
 */
int CachedServer::DoRemoveCacheDir(const std::string &dirname, CondorError &err) {

	// First, remove the classad
	m_log->BeginTransaction();
	DeleteClassAd(dirname.c_str());
	m_log->CommitTransaction();

	// Second, delete the directory
	std::string real_cache_dir = GetCacheDir(dirname, err);
	Directory cache_dir(real_cache_dir.c_str(), PRIV_CONDOR);
	if (!cache_dir.Remove_Full_Path(cache_dir.GetDirectoryPath())) {
		dprintf(D_FAILURE | D_ALWAYS, "DoRemoveCacheDir: Failed to remove cache directory %s\n", real_cache_dir.c_str());
		err.pushf("CACHED", 3, "Failed to remove cache directory: %s", real_cache_dir.c_str());
		return 1;
	}

	return 0;
}

int CachedServer::CreateCacheDirectory(const std::string &dirname, CondorError &err) {

	// Create the directory
	// 1. Get the caching directory from the condor configuration
	std::string caching_dir = GetCacheDir(dirname, err);

	// 3. Create the caching directory
	if ( !mkdir_and_parents_if_needed(caching_dir.c_str(), S_IRWXU, PRIV_CONDOR) ) {
		dprintf( D_FAILURE|D_ALWAYS,
				"couldn't create caching dir %s: %s\n",
				caching_dir.c_str(),
				strerror(errno) );
		err.pushf("CACHED", 3, "couldn't create caching dir %s: %s\n",
				caching_dir.c_str(),
				strerror(errno) );
		return 1;

	} else {
		dprintf(D_FULLDEBUG, "Creating caching directory for %s at %s\n",
				dirname.c_str(),
				caching_dir.c_str() );

	}
	return 0;


}

int CachedServer::LinkCacheDirectory(const std::string &source, const std::string &destination, CondorError &err) {
	dprintf(D_FULLDEBUG, "In LinkCacheDirectory 0, source = %s, destination = %s\n", source.c_str(), destination.c_str());
	std::string caching_dir = GetCacheDir(destination, err);
	dprintf(D_FULLDEBUG, "In LinkCacheDirectory 1, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	boost::filesystem::path src{source};
	dprintf(D_FULLDEBUG, "In LinkCacheDirectory 2, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	boost::filesystem::path dst{destination};
	dprintf(D_FULLDEBUG, "In LinkCacheDirectory 3, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	if(symlink(source.c_str(), caching_dir.c_str())) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to link files %s to %s: %s\n", source.c_str(), caching_dir.c_str(), strerror(errno));
		return 1;
	}
//	boost::filesystem::create_symlink(destination, source);
	dprintf(D_FULLDEBUG, "In LinkCacheDirectory 4, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	return 0;
}

filesize_t CachedServer::CalculateCacheSize(std::string cache_name) {

	CondorError err;

	// Get the directory
	std::string real_cache_dir = GetCacheDir(cache_name, err);
	Directory cache_dir(real_cache_dir.c_str(), PRIV_CONDOR);

	return cache_dir.GetDirectorySize();

}

int CachedServer::SetLogCacheSize(std::string cache_name, filesize_t size) {

	if (!m_log->AdExistsInTableOrTransaction(cache_name.c_str())) { return 0; }

	// TODO: Convert this to a real state.
	m_log->BeginTransaction();
	long long int insert_size = size;
	SetAttributeLong(cache_name.c_str(), ATTR_DISK_USAGE, insert_size);
	m_log->CommitTransaction();
	return 0;
}


int CachedServer::SetTorrentLink(std::string cache_name, std::string magnet_link) {

	if (!m_log->AdExistsInTableOrTransaction(cache_name.c_str())) { return 0; }

	dprintf(D_FULLDEBUG, "Magnet Link: %s\n", magnet_link.c_str());
	magnet_link = "\"" + magnet_link + "\"";

	// TODO: Convert this to a real state.
	m_log->BeginTransaction();
	SetAttributeString(cache_name.c_str(), ATTR_CACHE_MAGNET_LINK, magnet_link.c_str());
	m_log->CommitTransaction();
	return 0;

}


/**
 * Find the parent cache for this cache.  It checks first to find the parent
 * on this localhost.  Then, if it is the parent on the node, finds the parent
 * on the cluster (if it exists).  Then, if this daemon does not have a parent, 
 * returns itself.
 *
 * parent: parent article
 * returns: 1 if found parent, 0 if my own parent
 *
 */
int CachedServer::FindParentCache(counted_ptr<compat_classad::ClassAd> &parent) {

	// First, update the parent
	std::string cached_parent;
	param(cached_parent, "CACHED_PARENT");

	if (!cached_parent.empty()) {
		// Ok, the parent cache
		DCCached parent_daemon(cached_parent.c_str());
		if(parent_daemon.locate(Daemon::LOCATE_FULL)) {
			parent = (counted_ptr<compat_classad::ClassAd>)(new compat_classad::ClassAd(*parent_daemon.daemonAd()));
			return 1;
		} else {
			dprintf(D_FULLDEBUG | D_FAILURE, "Unable to locate the daemon %s.  Reverting to auto-detection of parent\n", cached_parent.c_str());
		}
	}


	// Get my own ad 
	counted_ptr<compat_classad::ClassAd> my_ad(new compat_classad::ClassAd(GenerateClassAd()));
	// Set the cache start time in my own ad:
	my_ad->Assign(ATTR_DAEMON_START_TIME, m_boot_time);

	// First, query the collector for caches on this node (same hostname / machine / ipaddress)
	dprintf(D_FULLDEBUG, "Querying for local daemons.\n");
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	// Make sure it's a cache server
	query.addANDConstraint("CachedServer =?= TRUE");

	// Make sure it's on the same host
	std::string created_constraint = "Machine =?= \"";
	created_constraint += get_local_fqdn().Value();
	created_constraint += "\"";
	QueryResult add_result = query.addANDConstraint(created_constraint.c_str());

	// And make sure that it's not this cache daemon
	created_constraint.clear();
	created_constraint = "Name =!= \"";
	created_constraint += m_daemonName.c_str();
	created_constraint += "\"";
	add_result = query.addANDConstraint(created_constraint.c_str());

	ClassAdList adList;
	QueryResult result = collectors->query(query, adList, NULL);
	dprintf(D_FULLDEBUG, "Got %i ads from query for local nodes\n", adList.Length());
	counted_ptr<compat_classad::ClassAd> current_parent_ad = my_ad;
	compat_classad::ClassAd *ad;

	if (adList.Length() >= 1) {
		// Compare the start time of our daemon with the starttime of them
		adList.Open();
		while ((ad = adList.Next())) {
			long long current_parent_start, remote_start;
			current_parent_start = remote_start = LLONG_MAX;

			// Look if the compared host is in the list of bad parents
			std::string other_name;
			ad->EvalString(ATTR_NAME, NULL, other_name);
			if(m_failed_parents.count(other_name)) {
				continue;
			}


			current_parent_ad->EvalInteger(ATTR_DAEMON_START_TIME, NULL, current_parent_start);
			ad->EvalInteger(ATTR_DAEMON_START_TIME, NULL, remote_start);

			dprintf(D_FULLDEBUG, "Comparing %llu with %llu\n", current_parent_start, remote_start);

			// TODO: handle when the times are equal
			if (remote_start < current_parent_start) {
				// New Parent!
				current_parent_ad = (counted_ptr<compat_classad::ClassAd>)(new compat_classad::ClassAd(*ad));
			} else if (remote_start == current_parent_start) {

				std::string current_parent_name;
				current_parent_ad->EvalString(ATTR_NAME, NULL, current_parent_name);
				if (current_parent_name.compare(other_name) < 0) {
					current_parent_ad = (counted_ptr<compat_classad::ClassAd>)(new compat_classad::ClassAd(*ad));
				}

			}

		}

	} else {
		// We are the only ones from this node
		dprintf(D_FULLDEBUG, "Did not find new parent\n");
		m_parent.has_parent = false;
		m_parent.parent_local = false;
		return NULL;
	}
	parent = current_parent_ad;
	if (parent == my_ad) {
		m_parent.has_parent = false;
		m_parent.parent_local = false;
		dprintf(D_FULLDEBUG, "Did not find new parent\n");
		return NULL;
	} else {
		std::string parent_name;
		parent->EvalString(ATTR_NAME, NULL, parent_name);
		DCCached parentD(parent.get(), NULL);
		parentD.locate(Daemon::LOCATE_FULL);

		m_parent.parent_ad = (counted_ptr<compat_classad::ClassAd>)(new compat_classad::ClassAd(*parent));
		m_parent.has_parent = true;
		m_parent.parent_local = true;

		dprintf(D_FULLDEBUG, "Found new parent: %s\n", parent_name.c_str());
		return 1;
	}

}


int CachedServer::DoHardlinkTransfer(ReliSock* rsock, std::string cache_name) {

	/** 
	 * The protocol is as follows:
	 * 1. Client sends a directory for the server to save a hardlink
	 * 2. Server creates hardlink file with mkstemp in directory from 1, and sends file name to client.
	 * 3. Client acknowledges creation, renames hardlink to dest from client.
	 */
	compat_classad::ClassAd ad;

	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		dprintf(D_FAILURE | D_ALWAYS, "Unable to get initial directory from client\n");
		return 1;
	}

	std::string directory;
	ad.EvaluateAttrString(ATTR_JOB_IWD, directory);

	std::string new_file = directory + "/" + "cached_test.XXXXXX";
	char* buf = strdup(new_file.c_str());
	int tmpfile = mkstemp(buf);
	if(!tmpfile) {
		dprintf(D_FAILURE | D_ALWAYS, "Unable to create temporary directory: %s", new_file.c_str());
		return PutErrorAd(rsock, 2, "DoHardlinkTransfer", "Unable to create temporary directory.");
	}
	new_file = buf;
	free(buf);
	close(tmpfile);

	std::string link_path = new_file + ".lnk";
	CondorError err;
	std::string cache_dir = GetCacheDir(cache_name, err);

	if(symlink(cache_dir.c_str(), link_path.c_str())) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to link files %s to %s: %s\n", cache_dir.c_str(), link_path.c_str(), strerror(errno));
		return PutErrorAd(rsock, 2, "DoHardlinkTransfer", "Unable to create HARDLINK");

	}

	// Remove the temporary file
	remove(new_file.c_str());

	// Send the file link
	rsock->encode();
	ad.Clear();
	ad.InsertAttr(ATTR_FILE_NAME, link_path);
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		dprintf(D_FAILURE | D_ALWAYS, "Failed to send file name to client\n");
		return 1;
	}


	// Get the ACK / NACK
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		dprintf(D_FAILURE | D_ALWAYS, "Did not get ACK / NACK from client\n");
		return 1;
	}

	// Try to remove the temp link file
	if(remove(link_path.c_str())) {

		dprintf(D_FULLDEBUG, "Removal of %s failed, but this is good\n", link_path.c_str());

	} else {
		dprintf(D_ALWAYS | D_FAILURE, "Removal of %s succeeded, client did not change file path\n", link_path.c_str());
	}






}


/**
 *	This function is encoding a directory.
 */

int CachedServer::DoEncodeDir(int /* cmd */, Stream* sock) 
{	
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveLocalReplicationRequest\n");
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir, getting into this function!!!\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ReceiveLocalReplicationRequest.\n");
		return 1;
	}
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir 1\n");//##

	std::string version;
	std::string encode_server;
	std::string encode_directory;
	int encode_data_num;
	int encode_parity_num;
	std::string encode_technique;
	int encode_field_size;
	int encode_packet_size;
	int encode_buffer_size;

	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");
		dprintf(D_ALWAYS, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");//##
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("EncodeServer", encode_server))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeServer in ReceiveLocalReplicationRequest request\n");
		dprintf(D_ALWAYS, "Client did not include EncodeServer in ReceiveLocalReplicationRequest request\n");//##
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheOriginatorHost attribute");
	}
	if (!request_ad.EvaluateAttrString("EncodeDir", encode_directory))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeDir\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeDir attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodeDataNum", encode_data_num))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeDataNum in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeDataNum attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodeParityNum", encode_parity_num))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeParityNum in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeParityNum attribute");
	}
	if (!request_ad.EvaluateAttrString("EncodeCodeTech", encode_technique))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeCodeTech in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeCodeTech attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodeFieldSize", encode_field_size))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeFieldSize in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeFieldSize attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodePacketSize", encode_packet_size))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodePacketSize in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodePacketSize attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodeBufferSize", encode_buffer_size))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeBufferSize in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeBufferSize attribute");
	}
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir 2\n");//##

	CondorError err;

	std::string real_encode_dir = GetCacheDir(encode_directory, err);
	ErasureCoder *coder = new ErasureCoder();
	int rc = 0;
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir, real_encode_dir=%s\n", real_encode_dir.c_str());//##
	rc = coder->JerasureEncodeDir (real_encode_dir, encode_data_num, encode_parity_num, encode_technique, encode_field_size, encode_packet_size, encode_buffer_size);
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir 3\n");//##

	delete coder;
	std::string encode_state = (rc ? "FAILED" : "SUCCEEDED");//##

	// Return the cache ad.
	compat_classad::ClassAd return_ad;
	std::string my_version = CondorVersion();
	return_ad.InsertAttr("CondorVersion", my_version);
	return_ad.InsertAttr("EncodeDirState", encode_state);

	if (!putClassAd(sock, return_ad) || !sock->end_of_message())
	{
		return 1;
	}

	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir 4\n");//##

	DistributeEncodedDir(real_encode_dir, encode_directory, encode_data_num, encode_parity_num);//##

	return rc;
}

// return the filenames of all files that have the specified extension
// in the specified directory and all subdirectories
static void get_all(const fs::path& root, const std::string& substr, std::list<std::string>& ret)
{
	if(!fs::exists(root) || !fs::is_directory(root)) return;

	fs::recursive_directory_iterator it(root);
	fs::recursive_directory_iterator endit;
	boost::regex expr{substr};
	boost::smatch what;
	while(it != endit)
	{
		if(fs::is_regular_file(*it) && boost::regex_search(it->path().filename().string(), what, expr)) ret.push_back(it->path().filename().string());
		++it;
	}

}

void CachedServer::DistributeEncodedDir(std::string &encode_dir, std::string &cache_name, int encode_data_num, int encode_parity_num)
{
	dprintf(D_FULLDEBUG, "encode_dir = %s, cache_name = %s, encode_data_num = %d, encode_parity_num = %d\n", encode_dir.c_str(), cache_name.c_str(), encode_data_num, encode_parity_num);
	dprintf(D_FULLDEBUG, "In CachedServer::DistributeEncodedFiles, Querying for the daemon\n");
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	query.addANDConstraint("CachedServer =?= TRUE");
        std::string created_constraint = "Name =!= \"";
        created_constraint += m_daemonName.c_str();
        created_constraint += "\"";
        query.addANDConstraint(created_constraint.c_str());

	ClassAdList adList;
	QueryResult result = collectors->query(query, adList, NULL);
	dprintf(D_FULLDEBUG, "Got %i ads from query\n", adList.Length());
	adList.Open();
        ClassAd* remote_cached_ad;
        counted_ptr<DCCached> client;
	CondorError err;
	time_t timer;
	int rc;
	for(int i = 2; i < encode_data_num + 1; ++i) {
		remote_cached_ad = adList.Next();
		dPrintAd(D_FULLDEBUG, *remote_cached_ad);
		client = (counted_ptr<DCCached>)(new DCCached(remote_cached_ad, NULL));
		time(&timer);
		timer = timer + 1000;
		rc = client->createCacheDir(cache_name, timer, err);
		std::string reg = "k"+boost::lexical_cast<std::string>(i)+"|meta";
		std::list<std::string> file_list;
		get_all(encode_dir, reg, file_list);
		for (std::list<std::string>::iterator it = file_list.begin(); it != file_list.end(); ++it) {
			dprintf(D_FULLDEBUG, "get_all: filename = %s\n", (*it).c_str());
		}
		rc = client->uploadFiles(cache_name, file_list, err);
	}
	for(int i = 1; i < encode_parity_num + 1; ++i) {
		remote_cached_ad = adList.Next();
		dPrintAd(D_FULLDEBUG, *remote_cached_ad);
		client = (counted_ptr<DCCached>)(new DCCached(remote_cached_ad, NULL));
		time(&timer);
		timer = timer + 1000;
		rc = client->createCacheDir(cache_name, timer, err);
		std::string reg = "m"+boost::lexical_cast<std::string>(i)+"|meta";
		std::list<std::string> file_list;
		get_all(encode_dir, reg, file_list);
		for (std::list<std::string>::iterator it = file_list.begin(); it != file_list.end(); ++it) {
			dprintf(D_FULLDEBUG, "get_all: filename = %s\n", (*it).c_str());
		}
		rc = client->uploadFiles(cache_name, file_list, err);
	}

}

/**
 *	This function is distribute specified cache and files to specified cached servers.
 */

int CachedServer::ReceiveDistributeReplicas(int /* cmd */, Stream* sock)
{	
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "entering CachedServer::ReceiveDistributeReplicas\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ReceiveDistributeReplicas.\n");
		return 1;
	}

	dprintf(D_FULLDEBUG, "before printing request_ad\n");//##
	dPrintAd(D_FULLDEBUG, request_ad);//##
	dprintf(D_FULLDEBUG, "after printing request_ad\n");//##
	std::string version;
	std::string cached_servers;
	std::string cache_name;
	std::string cache_id_str;
	std::string transfer_files;

	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ReceiveDistributeReplicas request\n");
		return PutErrorAd(sock, 1, "ReceiveDistributeReplicas", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("CachedServerNames", cached_servers))
	{
		dprintf(D_FULLDEBUG, "Client did not include CachedServerNames\n");
		return PutErrorAd(sock, 1, "ReceiveDistributeReplicas", "Request missing CachedServerNames attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", cache_name))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheName\n");
		return PutErrorAd(sock, 1, "ReceiveDistributeReplicas", "Request missing CacheName attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "Client did not include CacheID\n");
		return PutErrorAd(sock, 1, "ReceiveDistributeReplicas", "Request missing CacheID attribute");
	}
	if (!request_ad.EvaluateAttrString("TransferFiles", transfer_files))
	{
		dprintf(D_FULLDEBUG, "Client did not include TransferFiles\n");
		return PutErrorAd(sock, 1, "ReceiveDistributeReplicas", "Request missing TransferFiles attribute");
	}

	request_ad.Clear();
	CondorError err;

	std::vector<std::string> servers;
	std::vector<std::string> files;
	boost::split(servers, cached_servers, boost::is_any_of(", "));
	boost::split(files, transfer_files, boost::is_any_of(", "));

	for(int i = 0; i < servers.size(); ++i) {//##
		dprintf(D_FULLDEBUG, "servers = %s\n", servers[i].c_str());//##
	}//##
        for(int i = 0; i < files.size(); ++i) {//##
		dprintf(D_FULLDEBUG, "files = %s\n", files[i].c_str());//##
	}//##

	time_t expiry = int(time(0))+1000; // temporarily set as this value, change it later.
	int rc = DistributeReplicas(servers, cache_name, cache_id_str, expiry, files);
	if(rc) {
		dprintf(D_FULLDEBUG, "DistributeReplicas failed\n");
		return PutErrorAd(sock, 2, "ReceiveDistributeReplicas", "DistributeReplicas fails");
	}

	// Return the cache ad.
	compat_classad::ClassAd return_ad;
	return_ad.Clear();
	std::string my_version = CondorVersion();
	return_ad.InsertAttr("CondorVersion", my_version);
	return_ad.InsertAttr(ATTR_ERROR_CODE, 0);

	if (!putClassAd(sock, return_ad) || !sock->end_of_message())
	{
		return 1;
	}

	dprintf(D_FULLDEBUG, "exiting ReceiveDistributeReplicas\n");

	return 0;
}

/**
 *	This function is encoding a file.
 */

int CachedServer::DoEncodeFile(int /* cmd */, Stream* sock) 
{	
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveLocalReplicationRequest\n");
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile, getting into this function!!!\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ReceiveLocalReplicationRequest.\n");
		return 1;
	}
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile 1\n");//##

	std::string version;
	std::string encode_server;
	std::string encode_directory;
	std::string encode_file;
	int encode_data_num;
	int encode_parity_num;
	std::string encode_technique;
	int encode_field_size;
	int encode_packet_size;
	int encode_buffer_size;

	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");
		dprintf(D_ALWAYS, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");//##
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("EncodeServer", encode_server))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeServer in ReceiveLocalReplicationRequest request\n");
		dprintf(D_ALWAYS, "Client did not include EncodeServer in ReceiveLocalReplicationRequest request\n");//##
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheOriginatorHost attribute");
	}
	if (!request_ad.EvaluateAttrString("EncodeDir", encode_directory))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeDir\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeDir attribute");
	}
	if (!request_ad.EvaluateAttrString("EncodeFile", encode_file))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeFile\n");
		return PutErrorAd(sock, 1, "EncodeFile", "Request missing EncodeFile attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodeDataNum", encode_data_num))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeDataNum in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeDataNum attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodeParityNum", encode_parity_num))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeParityNum in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeParityNum attribute");
	}
	if (!request_ad.EvaluateAttrString("EncodeCodeTech", encode_technique))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeCodeTech in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeCodeTech attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodeFieldSize", encode_field_size))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeFieldSize in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeFieldSize attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodePacketSize", encode_packet_size))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodePacketSize in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodePacketSize attribute");
	}
	if (!request_ad.EvaluateAttrInt("EncodeBufferSize", encode_buffer_size))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeBufferSize in request\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeBufferSize attribute");
	}
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile 2\n");//##
	request_ad.Clear();
	CondorError err;

	std::string real_encode_dir = GetCacheDir(encode_directory, err);
	std::string real_encode_file = real_encode_dir + "/" + encode_file;
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile, real_encode_file=%s\n", real_encode_file.c_str());//##

	int rc;
	std::vector<std::string> return_files;
	std::string encode_state;//##
	/*
	   pid_t childPid;
	   childPid = fork();
	   if(childPid == 0) {
	   ErasureCoder *coder = new ErasureCoder();
	   return_files = coder->JerasureEncodeFile (real_encode_file, encode_data_num, encode_parity_num, encode_technique, encode_field_size, encode_packet_size, encode_buffer_size);
	   dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile 3\n");//##
	   delete coder;
	//		if(return_val) exit(1); // Encoding has some error.
	//		else exit(0); // Encoding succedded.
	} else if (childPid < 0) {
	dprintf(D_ALWAYS, "Fork error!\n");//##
	} else {
	int returnStatus;    
	waitpid(childPid, &returnStatus, 0);  // Parent process waits here for child to terminate.

	if (WEXITSTATUS(returnStatus) == 0)  // Verify child process terminated without error.  
	{
	rc = 0;
	encode_state = "SUCCEEDED";//##
	dprintf(D_ALWAYS, "The child process terminated normally.");//##
	}

	if (WEXITSTATUS(returnStatus) == 1)
	{
	rc = 1;
	encode_state = "FAILED";//##
	dprintf(D_ALWAYS, "The child process terminated with an error!.");//##    
	}
	}
	*/
	ErasureCoder *coder = new ErasureCoder();
	return_files = coder->JerasureEncodeFile (real_encode_file, encode_data_num, encode_parity_num, encode_technique, encode_field_size, encode_packet_size, encode_buffer_size);
	//	dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile and encode_state=%s\n",encode_state.c_str());//##
	if(return_files.empty()) {
		rc = 1;
		encode_state = "FAILED";
	} else {
		rc = 0;
		encode_state = "SUCCEEDED";
	}

	// Return the cache ad.
	compat_classad::ClassAd return_ad;
	return_ad.Clear();
	std::string my_version = CondorVersion();
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile and my_version==%s\n",my_version.c_str());//##
	return_ad.InsertAttr("CondorVersion", my_version);
	return_ad.InsertAttr("EncodeFileState", encode_state);
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile 4\n");//##
	if (!putClassAd(sock, return_ad) || !sock->end_of_message())
	{
		return 1;
	}

	dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile 5\n");//##

	std::vector<std::string>::iterator it;
	for(it = return_files.begin(); it != return_files.end(); ++it){
		dprintf(D_FULLDEBUG, "return encoded files = %s\n", (*it).c_str());//##
	}

	DistributeEncodedFiles(encode_directory, return_files);

	return rc;
}

void CachedServer::DistributeEncodedFiles(std::string cache_name, std::vector<std::string> &encoded_files)
{
	dprintf(D_FULLDEBUG, "In CachedServer::DistributeEncodedFiles, Querying for the daemon\n");
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	query.addANDConstraint("CachedServer =?= TRUE");
	std::string created_constraint = "Name =!= \"";
	created_constraint += m_daemonName.c_str();
	created_constraint += "\"";
	query.addANDConstraint(created_constraint.c_str());

	ClassAdList adList;
	QueryResult result = collectors->query(query, adList, NULL);
	dprintf(D_FULLDEBUG, "Got %i ads from query\n", adList.Length());
	adList.Open();
	ClassAd* remote_cached_ad = adList.Next();
	dPrintAd(D_FULLDEBUG, *remote_cached_ad);

	counted_ptr<DCCached> client;
	client = (counted_ptr<DCCached>)(new DCCached(remote_cached_ad, NULL));

	std::string cached_server;
	std::vector<std::string> transfer_files;
	std::string trans_file = encoded_files[0];
	transfer_files.push_back(trans_file);
	remote_cached_ad->EvaluateAttrString("Name", cached_server);// Cached Server uses attribute Name such as cached-1986@condormaster.unl.edu
	dprintf(D_FULLDEBUG, "Server = %s, CacheName = %s, and trans_file = %s\n", cached_server.c_str(), cache_name.c_str(), trans_file.c_str());//##
	compat_classad::ClassAd upstream_response;
	CondorError err;
	int rc = client->distributeEncodedFiles(cached_server, cache_name, transfer_files, upstream_response, err);
	dprintf(D_FULLDEBUG, "after distributeEncodedFiles and rc = %d\n", rc);//##

	if (rc) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to with return %i: %s\n", rc, err.getFullText().c_str());
	}

}

int CachedServer::ReceiveDistributeEncodedFiles(int /* cmd */, Stream* sock)
{
	dprintf(D_ALWAYS, "In CachedServer::ReceiveDistributeEncodedFiles\n");

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ReceiveLocalReplicationRequest.\n");
		return 1;
	}
	std::string cached_server;
	std::string cache_name;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");
		dprintf(D_ALWAYS, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");//##
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("CachedServerName", cached_server))
	{
		dprintf(D_FULLDEBUG, "Client did not include %s in ReceiveLocalReplicationRequest request\n", cached_server.c_str());
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheOriginatorHost attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", cache_name))
	{
		dprintf(D_FULLDEBUG, "Client did not include %s in ReceiveLocalReplicationRequest request\n", cache_name.c_str());
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheName attribute");
	}

	CondorError err;

	// Check if we have a record of this URL in the cache log
	compat_classad::ClassAd cache_ad;
	compat_classad::ClassAd* tmp_ad;
	dprintf(D_ALWAYS, "cache_name=%s",cache_name.c_str());//##
	if(GetCacheAd(cache_name, tmp_ad, err))
	{
		// found the cache in log
		dprintf(D_ALWAYS, "InsertAttr(ATTR_CACHE_REPLICATION_STATUS CLASSAD_READ)\n");//##
		cache_ad = *tmp_ad;
		cache_ad.InsertAttr("CachedDistributeState", "CLASSAD_READY");
		if (!putClassAd(sock, cache_ad) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			return 1;
		}
	}
	else 
	{
		// not found the cache in log
	}

	compat_classad::ClassAd transfer_ad;

	std::string dest;
	param(dest, "CACHING_DIR");

	dprintf(D_FULLDEBUG, "Download Files Destination = %s\n", dest.c_str());
	transfer_ad.InsertAttr(ATTR_OUTPUT_DESTINATION, dest.c_str());
	char current_dir[PATH_MAX];
	getcwd(current_dir, PATH_MAX);
	transfer_ad.InsertAttr(ATTR_JOB_IWD, current_dir);
	dprintf(D_FULLDEBUG, "IWD = %s\n", current_dir); // here the current working dir is 'log' directory

	FileTransfer* ft = new FileTransfer();
	int rc = 0;
	rc = ft->SimpleInit(&transfer_ad, false, true, static_cast<ReliSock*>(sock));
	if (!rc) {
		dprintf(D_ALWAYS, "Simple init failed\n");
		return 1;
	}
	ft->setPeerVersion(version.c_str());
	rc = ft->DownloadFiles();

	if (!rc) {
		dprintf(D_ALWAYS, "Download files failed.\n");
		return 1;
	}

	return 0;	
}


/**
 *	This function is decoding a file.
 */

int CachedServer::DoDecodeFile(int /* cmd */, Stream* sock) 
{	
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveLocalReplicationRequest\n");
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir, getting into this function!!!\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ReceiveLocalReplicationRequest.\n");
		return 1;
	}
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir 1\n");//##

	std::string version;
	std::string decode_server;
	std::string decode_directory;
	std::string decode_file;

	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");
		dprintf(D_ALWAYS, "Client did not include CondorVersion in ReceiveLocalReplicationRequest request\n");//##
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CondorVersion attribute");
		dprintf(D_ALWAYS, "Client did not include EncodeServer in ReceiveLocalReplicationRequest request\n");//##
		return PutErrorAd(sock, 1, "ReceiveLocalReplicationRequest", "Request missing CacheOriginatorHost attribute");
	}
	if (!request_ad.EvaluateAttrString("DecodeDir", decode_directory))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeDir\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeDir attribute");
	}
	if (!request_ad.EvaluateAttrString("DecodeFile", decode_file))
	{
		dprintf(D_FULLDEBUG, "Client did not include EncodeDir\n");
		return PutErrorAd(sock, 1, "EncodeDir", "Request missing EncodeDir attribute");
	}
	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir 2\n");//##
	request_ad.Clear();
	CondorError err;

	std::string real_decode_dir = GetCacheDir(decode_directory, err);
	std::string real_decode_file = real_decode_dir + "/" + decode_file;

	int rc;
	std::string decode_state;//##
	pid_t childPid;
	childPid = fork();
	if(childPid == 0) {
		int return_val = 0;
		ErasureCoder *coder = new ErasureCoder();
		return_val = coder->JerasureDecodeFile(real_decode_file);
		dprintf(D_ALWAYS, "In CachedServer::DoEncodeFile 3\n");//##
		delete coder;
		if(return_val) exit(1); // Encoding has some error.
		else exit(0); // Encoding succedded.
	} else if (childPid < 0) {
		dprintf(D_ALWAYS, "Fork error!\n");//##
	} else {
		int returnStatus;    
		waitpid(childPid, &returnStatus, 0);  // Parent process waits here for child to terminate.

		if (WEXITSTATUS(returnStatus) == 0)  // Verify child process terminated without error.  
		{
			rc = 0;
			decode_state = "SUCCEEDED";//##
			dprintf(D_ALWAYS, "The child process terminated normally.");//##
		}

		if (WEXITSTATUS(returnStatus) == 1)
		{
			rc = 1;
			decode_state = "FAILED";//##
			dprintf(D_ALWAYS, "The child process terminated with an error!.");//##    
		}
	}

	// Return the cache ad.
	compat_classad::ClassAd return_ad;
	return_ad.Clear();
	std::string my_version = CondorVersion();
	return_ad.InsertAttr("CondorVersion", my_version);
	return_ad.InsertAttr("DecodeState", decode_state);

	if (!putClassAd(sock, return_ad) || !sock->end_of_message())
	{
		return 1;
	}

	dprintf(D_ALWAYS, "In CachedServer::DoEncodeDir 4\n");//##

	return rc;
}

int CachedServer::DistributeReplicas(const std::vector<std::string> cached_servers, const std::string cache_name, const std::string cache_id_str, const time_t expiry, const std::vector<std::string> transfer_files)
{
	std::string redundancy_method = "Replication";
	dprintf(D_FULLDEBUG, "entering DistributeReplicas, cached_servers.size() = %d\n", cached_servers.size());
	int data_number = cached_servers.size();
	int parity_number = 0;
	std::string redundancy_candidates;
	for(int i = 0; i < cached_servers.size(); ++i) {
		redundancy_candidates += cached_servers[i];
		if(i != cached_servers.size()-1) {
			redundancy_candidates += ",";
		}
	}
	CondorError err;
	int rc = 0;
	for(int i = 0; i < cached_servers.size(); ++i) {
		const std::string cached_server = cached_servers[i];
		rc = CreateRemoteCacheDir(cached_server, cache_name, cache_id_str, expiry, redundancy_method, data_number, parity_number, redundancy_candidates);
		if(rc) {
			dprintf(D_FULLDEBUG, "CreateRemoteCacheDir Failed\n");
		}

		rc = UploadFilesToRemoteCache(cached_server, cache_name, cache_id_str, transfer_files);
		if(rc) {
			dprintf(D_FULLDEBUG, "UploadFilesToRemoteCache Failed\n");
		}

	}
	
	std::string dirname = cache_name + "+" + cache_id_str;
	m_log->BeginTransaction();
	SetAttributeString(dirname, ATTR_CACHE_NAME, cache_name);
	SetAttributeString(dirname, ATTR_CACHE_ID, cache_id_str);
	SetAttributeLong(dirname, ATTR_LEASE_EXPIRATION, expiry);
//	SetAttributeString(dirname, ATTR_OWNER, authenticated_user);
	SetAttributeString(dirname, ATTR_CACHE_ORIGINATOR_HOST, m_daemonName);
	SetAttributeString(dirname, ATTR_REQUIREMENTS, "MY.DiskUsage < TARGET.TotalDisk");
	SetAttributeString(dirname, ATTR_CACHE_REPLICATION_METHODS, "DIRECT");
	SetAttributeBool(dirname, ATTR_CACHE_ORIGINATOR, true);
	int state = COMMITTED;
	SetAttributeInt(dirname, ATTR_CACHE_STATE, state);
	SetAttributeString(dirname, "RedundancyManager", m_daemonName);
	SetAttributeString(dirname, "RedundancyMethod", redundancy_method);
	SetAttributeInt(dirname, "DataNumber", data_number);
	SetAttributeInt(dirname, "ParityNumber", parity_number);
	SetAttributeString(dirname, "RedundancyCandidates", redundancy_candidates);
	SetAttributeBool(dirname, "IsRedundancyManager", true);
	m_log->CommitTransaction();
	return 0;
}

int CachedServer::CreateRemoteCacheDir(const std::string cached_destination, const std::string cache_name, const std::string cache_id_str, const time_t expiry, const std::string redundancy_method, int data_number, int parity_number, std::string redundancy_candidates) {
	// Initiate the transfer
	DaemonAllowLocateFull new_daemon(DT_CACHED, cached_destination.c_str());
	if(!new_daemon.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 2;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
	}
	dprintf(D_FULLDEBUG, "2 In CreateRemoteCacheDir!\n");//##

	ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
					CACHED_CREATE_CACHE_DIR2, Stream::reli_sock, 20 );
	CondorError err;
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}
	dprintf(D_FULLDEBUG, "3 In CreateRemoteCacheDir!\n");//##

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("LeaseExpiration", expiry);
	ad.InsertAttr("CacheName", cache_name);
	ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
	ad.InsertAttr("RequestingCachedServer", m_daemonName);
	ad.InsertAttr("RedundancyMethod", redundancy_method);
	ad.InsertAttr("DataNumber", data_number);
	ad.InsertAttr("ParityNumber", parity_number);
	ad.InsertAttr("RedundancyCandidates", redundancy_candidates);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to send request to remote condor_cached");
		return 1;
	}
	dprintf(D_FULLDEBUG, "4 In CreateRemoteCacheDir!\n");//##

	ad.Clear();
	
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}
	dprintf(D_FULLDEBUG, "5 In CreateRemoteCacheDir!\n");//##

	rsock->close();
	delete rsock;

	int rc;
	if (!ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}
	dprintf(D_FULLDEBUG, "6 In CreateRemoteCacheDir!\n");//##

	if (rc)
	{
		std::string error_string;
		if (!ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
		return rc;
	}
	dprintf(D_FULLDEBUG, "7 In CreateRemoteCacheDir!\n");//##

	return 0;
}

int CachedServer::UploadFilesToRemoteCache(const std::string cached_destination, const std::string cache_name, const std::string cache_id_str, const std::vector<std::string> transfer_files) {
	dprintf(D_ALWAYS, "1 In UploadFilesToRemoteCache!\n");//##
	dprintf(D_ALWAYS, "cached_destination = %s\n", cached_destination.c_str());//##

	// Initiate the transfer
	DaemonAllowLocateFull new_daemon(DT_CACHED, cached_destination.c_str());
	if(!new_daemon.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
	}
	dprintf(D_ALWAYS, "2 In UploadFilesToRemoteCache!\n");//##

	ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
					CACHED_UPLOAD_FILES, Stream::reli_sock, 20 );

	CondorError err;
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "3 In UploadFilesToRemoteCache!\n");//##

	filesize_t transfer_size = 0;
	for (std::vector<std::string>::const_iterator it = transfer_files.begin(); it != transfer_files.end(); it++) {
		if (IsDirectory(it->c_str())) {
			Directory dir(it->c_str(), PRIV_USER);
			transfer_size += dir.GetDirectorySize();
		} else {
			StatInfo info(it->c_str());
			transfer_size += info.GetFileSize();
		}

	}
	
	dprintf(D_FULLDEBUG, "Transfer size = %lli\n", transfer_size);

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr(ATTR_DISK_USAGE, transfer_size);
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);

	dprintf(D_ALWAYS, "4 In UploadFilesToRemoteCache!\n");//##
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "5 In UploadFilesToRemoteCache!\n");//##

	ad.Clear();
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}

	dprintf(D_ALWAYS, "6 In UploadFilesToRemoteCache!\n");//##
	int rc;
	if (!ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}

	if (rc)
	{
		std::string error_string;
		if (!ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
		delete rsock;
		return rc;
	}

	dprintf(D_ALWAYS, "7 In UploadFilesToRemoteCache!\n");//##

	compat_classad::ClassAd transfer_ad;
	transfer_ad.InsertAttr("CondorVersion", version);

	// Expand the files list and add to the classad
	StringList input_files;
	for (std::vector<std::string>::const_iterator it = transfer_files.begin(); it != transfer_files.end(); it++) {
		input_files.insert((*it).c_str());
	}
	char* filelist = input_files.print_to_string();
	dprintf(D_FULLDEBUG, "Transfer list = %s\n", filelist);
	dprintf(D_ALWAYS, "Transfer list = %s\n", filelist);//##
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, filelist);
	char current_dir[PATH_MAX];
	getcwd(current_dir, PATH_MAX);
	transfer_ad.InsertAttr(ATTR_JOB_IWD, current_dir);
	dprintf(D_FULLDEBUG, "IWD = %s\n", current_dir);
	free(filelist);

	dprintf(D_ALWAYS, "8 In UploadFilesToRemoteCache!\n");//##
	// From here on out, this is the file transfer server socket.
	FileTransfer ft;
  	rc = ft.SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(rsock));
	dprintf(D_ALWAYS, "9 rc = %d\n", rc);//##
	if (!rc) {
		dprintf(D_ALWAYS, "Simple init failed\n");
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "9 In UploadFilesToRemoteCache!\n");//##
	ft.setPeerVersion(version.c_str());
	//UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	//ft.RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
	rc = ft.UploadFiles(true);

	if (!rc) {
		delete rsock;
		dprintf(D_ALWAYS, "Upload files failed.\n");
		return 1;
	}
	
	dprintf(D_ALWAYS, "10 In UploadFilesToRemoteCache!\n");//##
	delete rsock;
	return 0;
}

/**
 *	Advertise the redundancy of all caches stored on this server
 *
 */
void CachedServer::AdvertiseRedundancy() {

	classad::ClassAdParser	parser;

	dprintf(D_ALWAYS, "In AdvertiseRedundancy 1!\n");//##
	// Create the requirements expression
	char buf[512];
	sprintf(buf, "(%s == %i) && (%s =?= false) && (%s =!= \"%s\")", ATTR_CACHE_STATE, COMMITTED, "IsRedundancyManager", "RedundancyManager", m_daemonName.c_str());
	dprintf(D_FULLDEBUG, "AdvertiseRedundancy: Cache Query = %s\n", buf);

	std::list<compat_classad::ClassAd> caches = QueryCacheLog(buf);

	dprintf(D_ALWAYS, "In AdvertiseRedundancy 2, caches.size() = %d!\n", caches.size());//##
	// If there are no originator caches, then don't do anything
	if (caches.size() == 0) {
		daemonCore->Reset_Timer(m_advertise_redundancy_timer, 60);
		return;
	}

	std::string redundancy_manager;
	std::list<compat_classad::ClassAd>::iterator cache_iterator = caches.begin();
	while ((cache_iterator != caches.end())) {
		compat_classad::ClassAd cache_ad = *cache_iterator;
		cache_ad.EvaluateAttrString("RedundancyManager", redundancy_manager);
		DaemonAllowLocateFull manager_cached(DT_CACHED, redundancy_manager.c_str());
		if(!manager_cached.locate(Daemon::LOCATE_FULL)) {
			dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
			cache_iterator++;
			continue;
		} else {
			dprintf(D_FULLDEBUG, "Located daemon at %s\n", redundancy_manager.c_str());
		}
		dprintf(D_ALWAYS, "In AdvertiseRedundancy 3!\n");//##

		ReliSock *rsock = (ReliSock *)manager_cached.startCommand(
						CACHED_ADVERTISE_REDUNDANCY, Stream::reli_sock, 20 );

		dprintf(D_ALWAYS, "In AdvertiseRedundancy 4!\n");//##

		cache_ad.InsertAttr("CachedServerName", m_daemonName);
		if (!putClassAd(rsock, cache_ad) || !rsock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			delete rsock;
			dprintf(D_FULLDEBUG, "Failed to send cache_ad to remote redundancy manager\n");
			return;
		}
		dprintf(D_ALWAYS, "In AdvertiseRedundancy 5!\n");//##

		cache_ad.Clear();
		rsock->decode();
		if (!getClassAd(rsock, cache_ad) || !rsock->end_of_message())
		{
			// TODO: we have to design recover mechanism if the redundancy manager does not response
			delete rsock;
			dprintf(D_FULLDEBUG, "Failed to get response from remote redundancy manager\n");
			return;
		}

		dprintf(D_ALWAYS, "In AdvertiseRedundancy 6!\n");//##
		std::string ack;
		if (!cache_ad.EvaluateAttrString("RedundancyAcknowledgement", ack))
		{
			dprintf(D_FULLDEBUG, "Remote redundancy manager does not response an acknowledgement\n");
		}

		if(ack == "SUCCESS") {
			dprintf(D_FULLDEBUG, "Redundancy manager return SUCCESS!\n");
		} else {
			//TODO: we need to design recover mechanism here too if the redundancy manager return other messages
			delete rsock;
			dprintf(D_FULLDEBUG, "Redundancy manager does not return SUCCESS\n");
			return;
		}
		delete rsock;
		cache_iterator++;
	}

	dprintf(D_FULLDEBUG, "In AdvertiseRedundancy 7!\n");
	daemonCore->Reset_Timer(m_advertise_redundancy_timer, 60);
}

void CachedServer::CheckRedundancyCacheds()
{
	dprintf(D_FULLDEBUG, "entering CachedServer::CheckRedundancyCacheds\n");
	cache_to_unordered::iterator it_cache = redundancy_host_map.begin();
	while(it_cache != redundancy_host_map.end()) {
		std::string cache_name = it_cache->first;
		dprintf(D_FULLDEBUG, "In CachedServer::CheckRedundancyCacheds, it_cache->name = %s\n", cache_name.c_str());
		string_to_time::iterator it_host = (*(it_cache->second)).begin();
		while(it_host != (*(it_cache->second)).end()) {
			std::string cached_name = it_host->first;
			dprintf(D_FULLDEBUG, "In CachedServer::CheckRedundancyCacheds, it_host->name = %s, it_host->time = %lld\n", cached_name.c_str(), it_host->second);
			it_host++;
		}
		it_cache++;
	}
	dprintf(D_FULLDEBUG, "exiting CachedServer::CheckRedundancyCacheds\n");
	daemonCore->Reset_Timer(m_check_redundancy_cached_timer, 60);
}

//------------------------------------------------------------------
// Get Class Ad
//------------------------------------------------------------------

ClassAd* CachedServer::GetClassAd(const std::string& Key)
{
	ClassAd* ad = NULL;
	m_log->table.lookup(Key.c_str(),ad);
	return ad;
}

//------------------------------------------------------------------
// Delete Class Ad
//------------------------------------------------------------------

bool CachedServer::DeleteClassAd(const std::string& Key)
{
	ClassAd* ad=NULL;
	if (m_log->table.lookup(Key.c_str(),ad)==-1)
		return false;

	LogDestroyClassAd* log=new LogDestroyClassAd(Key.c_str());
	m_log->AppendLog(log);
	return true;
}

//------------------------------------------------------------------
// Set an Integer attribute
//------------------------------------------------------------------

void CachedServer::SetAttributeInt(const std::string& Key, const std::string& AttrName, int AttrValue)
{
	if (m_log->AdExistsInTableOrTransaction(Key.c_str()) == false) {
		LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*","*");
		m_log->AppendLog(log);
	}
	char value[50];
	sprintf(value,"%d",AttrValue);
	LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value);
	m_log->AppendLog(log);
}

//------------------------------------------------------------------
// Set a Float attribute
//------------------------------------------------------------------

void CachedServer::SetAttributeFloat(const std::string& Key, const std::string& AttrName, float AttrValue)
{
	if (m_log->AdExistsInTableOrTransaction(Key.c_str()) == false) {
		LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*","*");
		m_log->AppendLog(log);
	}

	char value[255];
	sprintf(value,"%f",AttrValue);
	LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value);
	m_log->AppendLog(log);
}

//------------------------------------------------------------------
// Set an Double attribute
//------------------------------------------------------------------

void CachedServer::SetAttributeDouble(const std::string& Key, const std::string& AttrName, double AttrValue)
{
	if (m_log->AdExistsInTableOrTransaction(Key.c_str()) == false) {
		LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*","*");
		m_log->AppendLog(log);
	}
	char value[50];
	sprintf(value,"%f",AttrValue);
	LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value);
	m_log->AppendLog(log);
}

//------------------------------------------------------------------
// Set an Long Long Integer attribute
//------------------------------------------------------------------

void CachedServer::SetAttributeLong(const std::string& Key, const std::string& AttrName, long long int AttrValue)
{
	if (m_log->AdExistsInTableOrTransaction(Key.c_str()) == false) {
		LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*","*");
		m_log->AppendLog(log);
	}
	char value[50];
	sprintf(value,"%lld",AttrValue);
	LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value);
	m_log->AppendLog(log);
}

//------------------------------------------------------------------
// Set an Bool attribute
//------------------------------------------------------------------

void CachedServer::SetAttributeBool(const std::string& Key, const std::string& AttrName, bool AttrValue)
{
	if (m_log->AdExistsInTableOrTransaction(Key.c_str()) == false) {
		LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*","*");
		m_log->AppendLog(log);
	}
	std::string value;
	if (AttrValue == true) {
		value = "true";
	} else {
		value = "false";
	}
	LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value.c_str());
	m_log->AppendLog(log);
}

//------------------------------------------------------------------
// Set a String attribute
//------------------------------------------------------------------

void CachedServer::SetAttributeString(const std::string& Key, const std::string& AttrName, const std::string& AttrValue)
{
	if (m_log->AdExistsInTableOrTransaction(Key.c_str()) == false) {
		LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*","*");
		m_log->AppendLog(log);
	}

	std::string value;
	value = "\"" + AttrValue + "\"";
	LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value.c_str());
	m_log->AppendLog(log);
}

//------------------------------------------------------------------
// Retrieve a Integer attribute
//------------------------------------------------------------------

bool CachedServer::GetAttributeInt(const std::string& Key, const std::string& AttrName, int& AttrValue)
{
	ClassAd* ad;
	if (m_log->table.lookup(Key.c_str(),ad)==-1) return false;
	if (ad->EvaluateAttrInt(AttrName.c_str(),AttrValue)==0) return false;
	return true;
}

//------------------------------------------------------------------
// Retrieve a Float attribute
//------------------------------------------------------------------

bool CachedServer::GetAttributeFloat(const std::string& Key, const std::string& AttrName, float& AttrValue)
{
	ClassAd* ad;
	if (m_log->table.lookup(Key.c_str(),ad)==-1) return false;
	double value;
	if (ad->EvaluateAttrReal(AttrName.c_str(),value)==0) return false;
	AttrValue = (float)value;
	return true;
}

//------------------------------------------------------------------
// Retrieve a Double attribute
//------------------------------------------------------------------

bool CachedServer::GetAttributeDouble(const std::string& Key, const std::string& AttrName, double& AttrValue)
{
	ClassAd* ad;
	if (m_log->table.lookup(Key.c_str(),ad)==-1) return false;
	if (ad->EvaluateAttrReal(AttrName.c_str(),AttrValue)==0) return false;
	return true;
}

//------------------------------------------------------------------
// Retrieve a Long Long Integer attribute
//------------------------------------------------------------------

bool CachedServer::GetAttributeLong(const std::string& Key, const std::string& AttrName, long long& AttrValue)
{
	ClassAd* ad;
	if (m_log->table.lookup(Key.c_str(),ad)==-1) return false;
	if (ad->EvaluateAttrInt(AttrName.c_str(),AttrValue)==0) return false;
	return true;
}

//------------------------------------------------------------------
// Retrieve a Bool attribute
//------------------------------------------------------------------

bool CachedServer::GetAttributeBool(const std::string& Key, const std::string& AttrName, bool& AttrValue)
{
	ClassAd* ad;
	if (m_log->table.lookup(Key.c_str(),ad)==-1) return false;
	if (ad->EvaluateAttrBool(AttrName.c_str(),AttrValue)==0) return false;
	return true;
}

//------------------------------------------------------------------
// Retrieve a String attribute
//------------------------------------------------------------------

bool CachedServer::GetAttributeString(const std::string& Key, const std::string& AttrName, std::string& AttrValue)
{
	ClassAd* ad;
	if (m_log->table.lookup(Key.c_str(),ad)==-1) return false;
	if (ad->EvaluateAttrString(AttrName.c_str(),AttrValue)==0) return false;
	return true;
}


