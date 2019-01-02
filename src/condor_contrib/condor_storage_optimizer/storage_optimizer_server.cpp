#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "get_daemon_name.h"
#include "ipv6_hostname.h"
#include <list>
#include <map>
#include "basename.h"

#include "storage_optimizer_server.h"
#include "dc_storage_optimizer.h"
#include "probability_function.h"

#include <sstream>

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

StorageOptimizerServer::StorageOptimizerServer()
{
	m_update_collector_timer = -1;
	m_reaper = -1;

	int rc = -1;
	rc = daemonCore->Register_Command(
		STORAGE_OPTIMIZER_GET_CACHED_INFO,
		"STORAGE_OPTIMIZER_GET_CACHED_INFO",
		(CommandHandlercpp)&StorageOptimizerServer::GetCachedInfo,
		"StorageOptimizerServer::GetCachedInfo",
		this,
		WRITE,
		D_COMMAND,
		true );
	ASSERT( rc >= 0 );

	rc = daemonCore->Register_Command(
		STORAGE_OPTIMIZER_LIST_STORAGE_OPTIMIZERS,
		"STORAGE_OPTIMIZER_LIST_STORAGE_OPTIMIZERS",
		(CommandHandlercpp)&StorageOptimizerServer::ListStorageOptimizers,
		"StorageOptimizerServer::ListStorageOptimizers",
		this,
		WRITE,
		D_COMMAND,
		true );
	ASSERT( rc >= 0 );

	m_update_collector_timer = daemonCore->Register_Timer (
			600,
			(TimerHandlercpp) &StorageOptimizerServer::UpdateCollector,
			"Update Collector",
			(Service*)this );
	// update collector for the first time
	UpdateCollector();

	m_runtime_pdf_timer = daemonCore->Register_Timer (
			10,
			(TimerHandlercpp) &StorageOptimizerServer::GetRuntimePdf,
			"Get Runtime PDF from Pilots",
			(Service*)this );
	// get runtime pdfs for the first time
	GetRuntimePdf();

	m_reaper = daemonCore->Register_Reaper("dummy_reaper",
			(ReaperHandler) &StorageOptimizerServer::dummy_reaper,
			"dummy_reaper",NULL);

	ASSERT( m_reaper >= 0 );

}

StorageOptimizerServer::~StorageOptimizerServer()
{
}

void StorageOptimizerServer::UpdateCollector() {
	dprintf(D_FULLDEBUG, "enter StorageOptimizer::UpdateCollector()\n");

	// Update the available caches on this server
	compat_classad::ClassAd published_classad = GenerateClassAd();
	dPrintAd(D_FULLDEBUG, published_classad);
	int rc = daemonCore->sendUpdates(UPDATE_AD_GENERIC, &published_classad);
	if (rc == 0) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to send commands to collectors, rc = %i\n", rc);
	} else {
		dprintf(D_FULLDEBUG, "Sent updates to %i collectors\n", rc);
	}

	dprintf( D_FULLDEBUG, "exit StorageOptimizer::UpdateCollector\n" );
	daemonCore->Reset_Timer(m_update_collector_timer, 600);
}

/**
 * Generate the daemon's classad, with all the information
 */

compat_classad::ClassAd StorageOptimizerServer::GenerateClassAd() {

	// Update the available caches on this server
	compat_classad::ClassAd published_classad;

	daemonCore->publish(&published_classad);

	published_classad.InsertAttr("StorageOptimizerServer", true);

	// Advertise the available disk space
//	std::string caching_dir;
//	param(caching_dir, "CACHING_DIR");
//	long long total_disk = sysapi_disk_space(caching_dir.c_str());
//	published_classad.Assign( ATTR_TOTAL_DISK, total_disk );

	published_classad.InsertAttr(ATTR_NAME, "storage optimizer");
/*
	classad::ClassAdParser   parser;
	ExprTree    *tree;
	tree = parser.ParseExpression("MY.TotalDisk > TARGET.DiskUsage");
	published_classad.Insert(ATTR_REQUIREMENTS, tree);
*/
	return published_classad;

	// Update the available caches on this server
//	compat_classad::ClassAd published_classad;
//	daemonCore->publish(&published_classad);
//	published_classad.InsertAttr("StorageOptimizerServer", true);
//	return published_classad;

}

int StorageOptimizerServer::dummy_reaper(Service *, int pid, int) {
	dprintf(D_ALWAYS,"dummy-reaper: pid %d exited; ignored\n",pid);
	return TRUE;
}

// This function collect CacheDs runtime PDFs and disk capacities
void StorageOptimizerServer::GetRuntimePdf() {
	// Query the collector for the cached
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	query.addANDConstraint("CachedServer =?= TRUE");
	ClassAdList cacheds;
	QueryResult result = collectors->query(query, cacheds, NULL);
	dprintf(D_FULLDEBUG, "In StorageOptimizerServer::GetRuntimePdf(), Got %i ads from query\n", cacheds.Length());

	int n = cacheds.Length();
	if (n < 1) {
		dprintf(D_FULLDEBUG, "StorageOptimizerServer::GetRuntimePdf, No CacheD available");
		return;
	}

	cacheds.Open();
	std::string cached_name;
	
	// use a set to store all current cacheds.
	std::set<std::string> cached_set;

	// iterate through current cacheds and compare each of them with m_cached_info_list/map
	for(int i = 0; i < n; ++i) {
		ClassAd* cached = cacheds.Next();
		if (!cached->EvaluateAttrString("Name", cached_name))
		{
			dprintf(D_FULLDEBUG, "StorageOptimizerServer::GetRuntimePdf, Cannot find CacheD Server Name");
			continue;
		}
		
		long long int total_disk = -1;
		if (!cached->EvaluateAttrInt(ATTR_TOTAL_DISK, total_disk))
		{
			dprintf(D_FULLDEBUG, "StorageOptimizerServer::GetRuntimePdf, Cannot find CacheD Disk Capacity");
			continue;
		}
		cached_set.insert(cached_name);
		dPrintAd(D_FULLDEBUG, *cached);

		// if cached exists
		if(m_cached_info_map.find(cached_name) != m_cached_info_map.end()) {
			std::list<SOCachedInfo>::iterator it = m_cached_info_map[cached_name];
			it->total_disk_space = total_disk;
			continue;
		}
		// insert new cached
		SOCachedInfo cached_info;
		cached_info.cached_name = cached_name;
		// let's set the pdf to uniform distribution with duration 60 minutes
		cached_info.probability_function = ProbabilityFunction(UNIFORM, 15);
		cached_info.total_disk_space = total_disk;
		cached_info.start_time = time(NULL);
		m_cached_info_list.push_back(cached_info);
		m_cached_info_map[cached_name] = prev(m_cached_info_list.end());
	}

	// iterate through
	std::vector<std::string> dead_cached_vec;
	for(std::list<struct SOCachedInfo>::iterator it = m_cached_info_list.begin(); it != m_cached_info_list.end(); it++) {
		std::string key = it->cached_name;
		if(cached_set.find(key) == cached_set.end()) {
			dead_cached_vec.push_back(key);
		}
	}
	//delete all dead cacheds
	for(int i = 0; i < dead_cached_vec.size(); ++i) {
		std::string key = dead_cached_vec[i];
		std::list<SOCachedInfo>::iterator it = m_cached_info_map[key];
		m_cached_info_list.erase(it);
		m_cached_info_map.erase(key);
	}
	daemonCore->Reset_Timer(m_runtime_pdf_timer, 10);
}

int StorageOptimizerServer::GetCachedInfo(int /*cmd*/, Stream * sock) {
	dprintf(D_FULLDEBUG, "entering StorageOptimizerServer::GetCachedInfo");//##

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for GetStoragePolicy.\n");
		return 1;
	}
	int time_to_failure_minutes = -1;
	if (!request_ad.EvaluateAttrInt("TimeToFailureMinutes", time_to_failure_minutes))
	{
		dprintf(D_FULLDEBUG, "StorageOptimizerServer::GetCachedInfo, Cannot find time to failure minutes");
		return 1;
	}
	long long int cache_size = -1;
	if (!request_ad.EvaluateAttrInt("CacheSize", cache_size))
	{
		dprintf(D_FULLDEBUG, "StorageOptimizerServer::GetCachedInfo, Cannot find cache size");
		return 1;
	}
	
	std::string failure_rates;
	std::string storage_capacities;

	// Get failure rates and storage capacities from all CacheDs
	dprintf(D_FULLDEBUG, "In StorageOptimizerServer::GetCachedInfo, m_cached_info_list.size() = %d\n", m_cached_info_list.size());
	for(std::list<SOCachedInfo>::iterator it = m_cached_info_list.begin(); it != m_cached_info_list.end(); ++it) {
		SOCachedInfo cached_info = *it;
		dprintf(D_FULLDEBUG, "In StorageOptimizerServer::GetCachedInfo, cached_info.name = %s, cached_info.space = %lld, cached_info.time = %lld, cache_size = %d\n", cached_info.cached_name.c_str(), cached_info.total_disk_space, cached_info.start_time, cache_size);
		if(cached_info.total_disk_space < cache_size) continue;
		time_t start_time = cached_info.start_time;
		time_t current_time = time(NULL);
		double failure_rate = cached_info.probability_function.getProbability(start_time, current_time, time_to_failure_minutes);
		dprintf(D_FULLDEBUG, "StorageOptimizerServer::GetCachedInfo, start_time = %lld, current_time = %lld, time_to_failure_minutes = %d, failure_rate = %f\n", start_time, current_time, time_to_failure_minutes, failure_rate);
		// we need to think about different cases:
		// 1. time_to_failure_seconds > 0, time_to_end_seconds > 0, time_to_failure_seconds < time_to_end_seconds (valid location);
		// 2. time_to_failure_seconds > 0, time_to_end_seconds > 0, time_to_failure_seconds >= time_to_end_seconds (designated to fail);
		// 3. time_to_failure_seconds > 0, time_to_end_seconds <=0, (pass the pdf's expected deadline - should fail);
		// 4. time_to_failure_seconds <=0, time_to_end_seconds > 0, (cache's expiry has been passed, so cache is safe now to be deleted);
		// 5. time_to_failure_seconds <=0, time_to_end_seconds <=0, time_to_failure_seconds < time_to_end_seconds (failure_rate > 1.0 but cache is safe to be deleted now)
		// 6. time_to_failure_seconds <=0, time_to_end_seconds <=0, time_to_failure_seconds >=time_to_end_seconds (0.0 < failure_rate < 1.0 but cache is safe to be deleted now)
		// 4,5,6 should be handled by CacheD code (cached_server.cpp);
		// 2,3 are handled here:
		if(failure_rate <= 0.0 || failure_rate >= 1.0) {
			dprintf(D_FULLDEBUG, "StorageOptimizerServer::GetCachedInfo, failure_rate is wrong, start_time = %lld, current_time = %lld, time_to_failure_minutes = %d, failure_rate = %f\n", start_time, current_time, time_to_failure_minutes, failure_rate);
			continue;
		}
		failure_rates += cached_info.cached_name + "=" + std::to_string(failure_rate);
		storage_capacities += cached_info.cached_name + "=" + std::to_string(cached_info.total_disk_space);
		if(it != prev(m_cached_info_list.end())) {
			failure_rates += ",";
			storage_capacities += ",";
		}
	}

	compat_classad::ClassAd return_ad;
	return_ad.InsertAttr("FailureRates", failure_rates);
	return_ad.InsertAttr("StorageCapacities", storage_capacities);
	if (!putClassAd(sock, return_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_ALWAYS | D_FAILURE, "Failed to write response in StorageOptimizerServer::GetCachedInfo.\n");
	}
	dprintf(D_FULLDEBUG, "In StorageOptimizerServer::GetCachedInfo, FailureRates = %s\n", failure_rates.c_str());//##
	dprintf(D_FULLDEBUG, "In StorageOptimizerServer::GetCachedInfo, StorageCapacities = %s\n", storage_capacities.c_str());//##
	dprintf(D_FULLDEBUG, "exiting StorageOptimizerServer::GetCachedInfo");//##

	return 0;
}

int StorageOptimizerServer::ListStorageOptimizers(int /*cmd*/, Stream *sock)
{
	dprintf(D_FULLDEBUG, "In ListStorageOptimizers\n");
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
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in ListStorageOptimizers request\n");
		return PutErrorAd(sock, 1, "ListStorageOptimizers", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString(ATTR_REQUIREMENTS, requirements))
	{
		dprintf(D_FULLDEBUG, "Client did not include Requirements in ListStorageOptimizers request\n");
		return PutErrorAd(sock, 1, "ListStorageOptimizers", "Request missing Requirements attribute");
	}

	dprintf(D_FULLDEBUG, "Querying for local daemons.\n");
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	// Make sure it's a cache server
	query.addANDConstraint("StorageOptimizerServer =?= TRUE");
	if(!requirements.empty()) {
		query.addANDConstraint(requirements.c_str());
	}

	ClassAdList so_ads;
	QueryResult result = collectors->query(query, so_ads, NULL);
	dprintf(D_FULLDEBUG, "Got %i ads from query for total StorageOptimizers in cluster\n", so_ads.Length());

	compat_classad::ClassAd final_ad;
	final_ad.Assign("FinalAd", true);

	compat_classad::ClassAd *ad;
	so_ads.Open();
	while ((ad = so_ads.Next())) {
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
	dprintf(D_FULLDEBUG, "Finish StorageOptimizerServer::ListStorageOptimizers()\n");

	return 0;
}
