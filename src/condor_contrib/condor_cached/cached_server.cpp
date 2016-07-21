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
#include "directory.h"

#include "cached_torrent.h"
#include "dc_cached.h"

#include <sstream>


#define SCHEMA_VERSION 1

const int CachedServer::m_schema_version(SCHEMA_VERSION);
const char *CachedServer::m_header_key("CACHE_ID");

CachedServer::CachedServer():
	m_registered_handlers(false)
{
	
	m_boot_time = time(NULL);
	
	if ( !m_registered_handlers )
	{
		m_registered_handlers = true;

		// Register the commands
		int rc = daemonCore->Register_Command(
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
	m_replication_check = daemonCore->Register_Timer(10,
		(TimerHandlercpp)&CachedServer::CheckReplicationRequests,
		"CachedServer::CheckReplicationRequests",
		(Service*)this );
		
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
				{
					TransactionSentry sentry(m_log);
					cache_ad.InsertAttr(ATTR_CACHE_STATE, UNCOMMITTED);
					cache_ad.InsertAttr(ATTR_CACHE_ORIGINATOR, false);
					cache_ad.InsertAttr(ATTR_CACHE_PARENT_CACHED, parent_name);
					m_log->AppendAd(cache_name, cache_ad, "*", "*");
				}
				
				DoDirectDownload(parent_name, m_requested_caches[cache_name]);
				it = m_requested_caches.erase(it);
				continue;
				
			} else if (transfer_method == "BITTORRENT") {
				
				// Put it in the log
				{
					TransactionSentry sentry(m_log);
					cache_ad.InsertAttr(ATTR_CACHE_STATE, UNCOMMITTED);
					cache_ad.InsertAttr(ATTR_CACHE_ORIGINATOR, false);
					cache_ad.InsertAttr(ATTR_CACHE_PARENT_CACHED, parent_name);
					m_log->AppendAd(cache_name, cache_ad, "*", "*");
				}
				
				DoBittorrentDownload(cache_ad);
				it = m_requested_caches.erase(it);
				continue;
			}
			
		}
		it++;
		
	}
	
	daemonCore->Reset_Timer(m_replication_check, 10);

}


/**
	*	This function will initialize the torrents
	*
	*/

void CachedServer::InitializeBittorrent() {
	

	InitTracker();
	
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
			DownloadTorrent(magnet_uri, caching_dir, "");
			
		} else {
			
			std::string torrent_file = cache_dir + "/.torrent";
			AddTorrentFromFile(torrent_file, cache_dir);
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
	
	HandleAlerts(completed_torrents, errored_torrents);
	
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
		Daemon remote_cached(remote_cached_ad, DT_GENERIC, NULL);
		
		//Daemon remote_cached(DT_GENERIC, remote_daemon_name.c_str());
		if(!remote_cached.locate()) {
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
		if(!cached_daemon.locate()) {
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
	m_log.reset(new ClassAdLog(m_db_fname.c_str()));
	InitializeDB();

}


int
CachedServer::InitializeDB()
{
	
	// Check for all caches that we are the origin and update the originator name.
	std::string cache_query = ATTR_CACHE_ORIGINATOR;
	cache_query += " == true";
	std::list<compat_classad::ClassAd> caches = QueryCacheLog(cache_query);
	
	for (std::list<compat_classad::ClassAd>::iterator it = caches.begin(); it != caches.end(); it++) {
		
		{
			TransactionSentry sentry(m_log);
			std::string cache_name;
			it->EvalString(ATTR_CACHE_NAME, NULL, cache_name);
			if (!m_log->AdExistsInTableOrTransaction(cache_name.c_str())) { continue; }

				// TODO: Convert this to a real state.
			std::string origin_host = "\"" + m_daemonName + "\"";
			LogSetAttribute *attr = new LogSetAttribute(cache_name.c_str(), ATTR_CACHE_ORIGINATOR_HOST, origin_host.c_str());
			m_log->AppendLog(attr);
		}
		
		
	}
	
	
	/*
	if (!m_log->AdExistsInTableOrTransaction(m_header_key))
	{
		TransactionSentry sentry(m_log);
		classad::ClassAd ad;
		m_log->AppendAd(m_header_key, ad, "*", "*");
	}
	compat_classad::ClassAd *ad;
	m_log->table.lookup(m_header_key, ad);
	if (!ad->EvaluateAttrInt(ATTR_NEXT_CACHE_NUM, m_id))
	{
		m_id = 0;
	}
	*/
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
	classad::ClassAd log_ad;
	log_ad.InsertAttr(ATTR_CACHE_NAME, dirname);
	log_ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
	log_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	log_ad.InsertAttr(ATTR_OWNER, authenticated_user);
	log_ad.InsertAttr(ATTR_CACHE_ORIGINATOR_HOST, m_daemonName);
	
	// TODO: Make requirements more dynamic by using ATTR values.
	log_ad.InsertAttr(ATTR_REQUIREMENTS, "MY.DiskUsage < TARGET.TotalDisk");
	log_ad.InsertAttr(ATTR_CACHE_REPLICATION_METHODS, "BITTORRENT, DIRECT");
	log_ad.InsertAttr(ATTR_CACHE_ORIGINATOR, true);
	log_ad.InsertAttr(ATTR_CACHE_STATE, UNCOMMITTED);
	{
	TransactionSentry sentry(m_log);
	m_log->AppendAd(dirname, log_ad, "*", "*");
	}

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
				std::string magnet_link = MakeTorrent(cache_dir, cache_id);
				m_server.SetTorrentLink(m_cacheName, magnet_link);
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
        std::string dirname;
        std::string version;
				filesize_t diskUsage;
        if (!request_ad.EvaluateAttrString("CondorVersion", version))
        {
								dprintf(D_FULLDEBUG, "Client did not include CondorVersion in UploadToServer request\n");
                return PutErrorAd(sock, 1, "UploadFiles", "Request missing CondorVersion attribute");
        }
        if (!request_ad.EvaluateAttrString("CacheName", dirname))
        {
								dprintf(D_FULLDEBUG, "Client did not include CacheName in UploadToServer request\n");
                return PutErrorAd(sock, 1, "UploadFiles", "Request missing CacheName attribute");
        }
				if (!request_ad.LookupInteger(ATTR_DISK_USAGE, diskUsage))
				{
								dprintf(D_FULLDEBUG, "Client did not include %s in UploadToServer request\n", ATTR_DISK_USAGE);
								return PutErrorAd(sock, 1, "UploadFiles", "Request missing DiskUsage attribute");
				}
				
	CondorError err;
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
	UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	ft->RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
	
	// Restrict the amount of data that the file transfer will transfer
	ft->setMaxDownloadBytes(diskUsage);
	

	rc = ft->DownloadFiles(false);
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed DownloadFiles\n");
	} else {
		dprintf(D_FULLDEBUG, "Successfully began downloading files\n");
		SetCacheUploadStatus(dirname.c_str(), UPLOADING);
		
	}
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
	ft->UploadFiles(false);
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
	LogSetAttribute *attr = new LogSetAttribute(dirname.c_str(), ATTR_REQUIREMENTS, replication_policy.c_str());
	{
	TransactionSentry sentry(m_log);
	m_log->AppendLog(attr);
	}
	
	if (replication_methods.size() != 0) {
		
		// Make sure the replication methods are quoted
		if (replication_methods.at(0) != '\"') {
			replication_methods.insert(0, "\"");
		}
		
		if (replication_methods.at(replication_methods.length()-1) != '\"') {
			replication_methods.append("\"");
		}
		
		attr = new LogSetAttribute(dirname.c_str(), ATTR_CACHE_REPLICATION_METHODS, replication_methods.c_str());
		{
		TransactionSentry sentry(m_log);
		m_log->AppendLog(attr);
		}
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
		request_ptr->InsertAttr(ATTR_CACHE_ORIGINATOR, false);
		request_ptr->InsertAttr(ATTR_CACHE_STATE, UNCOMMITTED);
		
		// Put it in the log
		{
			TransactionSentry sentry(m_log);
			m_log->AppendAd(cache_name, *request_ptr, "*", "*");
		}
		
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
		Daemon new_daemon(&peer_ad, DT_GENERIC, "");
		if(!new_daemon.locate()) {
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
		cache_ad = *tmp_ad;
		cache_ad.InsertAttr(ATTR_CACHE_REPLICATION_STATUS, "CLASSAD_READY");
		if (!putClassAd(sock, cache_ad) || !sock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			return 1;
		}
	}
	else if (m_requested_caches.count(cache_name)) {
		
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


int CachedServer::DoDirectDownload(std::string cache_source, compat_classad::ClassAd cache_ad) {
	
	long long cache_size;
	cache_ad.EvalInteger(ATTR_DISK_USAGE, NULL, cache_size);
	
	std::string cache_name;
	cache_ad.LookupString(ATTR_CACHE_NAME, cache_name);
	
	CondorError err;
	std::string dest = GetCacheDir(cache_name, err);
	CreateCacheDirectory(cache_name, err);
	
	// Initiate the transfer
	Daemon new_daemon(DT_CACHED, cache_source.c_str());
	if(!new_daemon.locate()) {
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
	
	
	rc = ft->DownloadFiles(false);
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
		DownloadTorrent(magnet_uri, caching_dir, "");
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
	if (m_log->table.lookup(dirname.c_str(), cache_ad) == -1)
	{
		err.pushf("CACHED", 3, "Cache ad %s not found", dirname.c_str());
		return 0;
	}
	return 1;
}


int CachedServer::SetCacheUploadStatus(const std::string &dirname, CACHE_STATE state)
{
	TransactionSentry sentry(m_log);
	if (!m_log->AdExistsInTableOrTransaction(dirname.c_str())) { return 0; }

		// TODO: Convert this to a real state.
	LogSetAttribute *attr = new LogSetAttribute(dirname.c_str(), ATTR_CACHE_STATE, boost::lexical_cast<std::string>(state).c_str());
	m_log->AppendLog(attr);
	return 0;
}

/*
 * Get the current upload status
 */
CachedServer::CACHE_STATE CachedServer::GetUploadStatus(const std::string &dirname) {
	TransactionSentry sentry(m_log);

	// Check if the cache directory even exists
	if (!m_log->AdExistsInTableOrTransaction(dirname.c_str())) { return INVALID; }

	compat_classad::ClassAd *cache_ad;
	CondorError errorad;
	// Check the cache status
	if (GetCacheAd(dirname, cache_ad, errorad) == 0 )
		return INVALID;
	
	

	dprintf(D_FULLDEBUG, "Caching classad:");
	compat_classad::dPrintAd(D_FULLDEBUG, *cache_ad);
	
	int int_state;
	if (! cache_ad->EvalInteger(ATTR_CACHE_STATE, NULL, int_state)) {
		return INVALID;
	}
	
	
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


std::list<compat_classad::ClassAd> CachedServer::QueryCacheLog(std::string requirement){
	
	classad::ClassAdParser	parser;
	ExprTree	*tree;
	std::list<compat_classad::ClassAd> toReturn;
	
	dprintf(D_FULLDEBUG, "Cache Query = %s\n", requirement.c_str());
	
	if ( !( tree = parser.ParseExpression(requirement.c_str()) )) {
		dprintf(D_ALWAYS | D_FAILURE, "Unable to parse expression %s\n", requirement.c_str());
		return toReturn;
	}
	
	//TransactionSentry sentry(m_log);
	ClassAdLog::filter_iterator it(&m_log->table, tree, 1000);
	ClassAdLog::filter_iterator end(&m_log->table, NULL, 0, true);
	
	while ( it != end ) {
		ClassAd* tmp_ad = *it++;
		if (!tmp_ad) {
			break;
		}
		
		ClassAd newClassad = *tmp_ad;
		toReturn.push_front(newClassad);
		
	}
	
	return toReturn;
}


std::string CachedServer::GetCacheDir(const std::string &dirname, CondorError& /* err */) {

	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	dprintf(D_FULLDEBUG, "Caching directory is set to: %s\n", caching_dir.c_str());

	// 2. Combine the system configured caching directory with the user specified
	// 	 directory.
	// TODO: sanity check the dirname, ie, no ../...
	//caching_dir += "/";
	caching_dir += dirname;

	return caching_dir;

}


/**
	*	Remove the cache dir, both the classad in the log and the directories on disk.
	*/
int CachedServer::DoRemoveCacheDir(const std::string &dirname, CondorError &err) {
	
	// First, remove the classad
	{
		TransactionSentry sentry(m_log);
		LogDestroyClassAd* removelog = new LogDestroyClassAd(dirname.c_str());
		m_log->AppendLog(removelog);
	}
	
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




filesize_t CachedServer::CalculateCacheSize(std::string cache_name) {
	
	CondorError err;
	
	// Get the directory
	std::string real_cache_dir = GetCacheDir(cache_name, err);
	Directory cache_dir(real_cache_dir.c_str(), PRIV_CONDOR);
	
	return cache_dir.GetDirectorySize();
	
}

int CachedServer::SetLogCacheSize(std::string cache_name, filesize_t size) {
	
	TransactionSentry sentry(m_log);
	if (!m_log->AdExistsInTableOrTransaction(cache_name.c_str())) { return 0; }

		// TODO: Convert this to a real state.
	LogSetAttribute *attr = new LogSetAttribute(cache_name.c_str(), ATTR_DISK_USAGE, boost::lexical_cast<std::string>(size).c_str());
	m_log->AppendLog(attr);
	return 0;
	
		
	
}


int CachedServer::SetTorrentLink(std::string cache_name, std::string magnet_link) {
	
	TransactionSentry sentry(m_log);
	if (!m_log->AdExistsInTableOrTransaction(cache_name.c_str())) { return 0; }
	
	dprintf(D_FULLDEBUG, "Magnet Link: %s\n", magnet_link.c_str());
	magnet_link = "\"" + magnet_link + "\"";

		// TODO: Convert this to a real state.
	LogSetAttribute *attr = new LogSetAttribute(cache_name.c_str(), ATTR_CACHE_MAGNET_LINK, magnet_link.c_str());
	m_log->AppendLog(attr);
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
		if(parent_daemon.locate()) {
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
		parentD.locate();
		
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
