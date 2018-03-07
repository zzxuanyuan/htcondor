#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "classad_hashtable.h"
#include "get_daemon_name.h"
#include "ipv6_hostname.h"
#include <list>
#include <map>
#include "basename.h"

#include "cacheflow_manager_server.h"
#include "dc_cacheflow_manager.h"
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


CacheflowManagerServer::CacheflowManagerServer()
{
	m_update_collector_tid = -1;
	m_reaper_tid = -1;

	// Register the commands
	int rc = daemonCore->Register_Command(
		CACHEFLOW_MANAGER_PING,
		"CACHEFLOW_MANAGER_PING",
		(CommandHandlercpp)&CacheflowManagerServer::Ping,
		"CacheflowManagerServer::Ping",
		this,
		WRITE,
		D_COMMAND,
		true );
	ASSERT( rc >= 0 );

	rc = daemonCore->Register_Command(
		CACHEFLOW_MANAGER_GET_STORAGE_POLICY,
		"CACHEFLOW_MANAGER_GET_STORAGE_POLICY",
		(CommandHandlercpp)&CacheflowManagerServer::GetStoragePolicy,
		"CacheflowManagerServer::GetStoragePolicy",
		this,
		WRITE,
		D_COMMAND,
		true );
	ASSERT( rc >= 0 );

	m_update_collector_tid = daemonCore->Register_Timer (
			60,
			(TimerHandlercpp) &CacheflowManagerServer::UpdateCollector,
			"Update Collector",
			(Service*)this );

	m_reaper_tid = daemonCore->Register_Reaper("dummy_reaper",
			(ReaperHandler) &CacheflowManagerServer::dummy_reaper,
			"dummy_reaper",NULL);

	ASSERT( m_reaper_tid >= 0 );
	Init();
}

CacheflowManagerServer::~CacheflowManagerServer()
{
}

void CacheflowManagerServer::Init()
{
	// We need to update this function and let cacheflow_manager to pull CacheD's failure probability functions from storage_optimizer.
	// Now we just create bunch of dummy CacheDs as well as their failure probability functions for test purpose.
	CreateDummyCacheDs(GAUSSIAN);
}

void CacheflowManagerServer::UpdateCollector() {
	dprintf(D_FULLDEBUG, "enter CacheflowManager::UpdateCollector()\n");

	// Update the available caches on this server
	compat_classad::ClassAd published_classad = GenerateClassAd();
	dPrintAd(D_FULLDEBUG, published_classad);
	int rc = daemonCore->sendUpdates(UPDATE_AD_GENERIC, &published_classad);
	if (rc == 0) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to send commands to collectors, rc = %i\n", rc);
	} else {
		dprintf(D_FULLDEBUG, "Sent updates to %i collectors\n", rc);
	}

	dprintf( D_FULLDEBUG, "exit CacheflowManager::UpdateCollector\n" );
}

int CacheflowManagerServer::Ping() {
	dprintf(D_FULLDEBUG, "entering CacheflowManagerServer::Ping()\n");
	dprintf(D_FULLDEBUG, "exiting CacheflowManagerServer::Ping()\n");
	return 0;
}

int CacheflowManagerServer::GetStoragePolicy(int /*cmd*/, Stream * sock) {

	dprintf(D_FULLDEBUG, "entering CacheflowManagerServer::GetStoragePolicy()\n");
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad))
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for GetStoragePolicy.\n");
		return 1;
	}
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in GetStoragePolicy request\n");
		return PutErrorAd(sock, 1, "GetStoragePolicy", "Request missing CondorVersion attribute");
	}
	compat_classad::ClassAd jobAd;
	if (!getClassAd(sock, jobAd))
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for GetStoragePolicy.\n");
		return 1;
	}
	if (!sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for GetStoragePolicy.\n");
		return 1;
	}

	compat_classad::ClassAd policyAd;
	dprintf(D_FULLDEBUG, "Before NegotiateStoragePolicy().\n");
	policyAd = NegotiateStoragePolicy(jobAd);
	dPrintAd(D_FULLDEBUG, policyAd);
	if (!putClassAd(sock, policyAd) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_ALWAYS | D_FAILURE, "Failed to write response in GetStoragePolicy.\n");
	}

	return 0;
}

/**
 * Generate the daemon's classad, with all the information
 */

compat_classad::ClassAd CacheflowManagerServer::GenerateClassAd() {

	// Update the available caches on this server
	compat_classad::ClassAd published_classad;

	daemonCore->publish(&published_classad);

	published_classad.InsertAttr("CacheflowManager", true);

	return published_classad;
}

int CacheflowManagerServer::dummy_reaper(Service *, int pid, int) {
	dprintf(D_ALWAYS,"dummy-reaper: pid %d exited; ignored\n",pid);
	return TRUE;
}

compat_classad::ClassAd CacheflowManagerServer::NegotiateStoragePolicy(compat_classad::ClassAd& jobAd) {
	double max_failure_rate;
	double time_to_fail_minutes;
	long long cache_size;
	std::string cached_server;
	jobAd.EvaluateAttrString("CachedServer", cached_server);
	jobAd.EvaluateAttrNumber("MaxFailureRate", max_failure_rate);
	jobAd.EvaluateAttrNumber("TimeToFailureMinutes", time_to_fail_minutes);
	jobAd.EvaluateAttrNumber("CacheSize", cache_size);
	dprintf(D_FULLDEBUG, "CachedServer = %s\n", cached_server.c_str());
	dprintf(D_FULLDEBUG, "MaxFailureRate = %f\n", max_failure_rate);
	dprintf(D_FULLDEBUG, "TimeToFailMinutes = %f\n", time_to_fail_minutes);
	dprintf(D_FULLDEBUG, "CacheSize = %lld\n", cache_size);

	std::string cached_candidates;
	double accumulate_failure_rate = 1.0;
	// If the server that sent this query has CacheD daemon on it, it should be taken into account as a candidate for this cache.
	if(!cached_server.empty()) {
		CachedInfo self_info = *m_cached_info_map[cached_server];
		accumulate_failure_rate = self_info.probability_function.getProbability(time_to_fail_minutes);
		cached_candidates += self_info.cached_name;
		cached_candidates += ",";
	}
	// Iterate CacheD list and find the first n CacheDs whose total failure rate is less than the required max failure rate.
	for(std::list<CachedInfo>::iterator it = m_cached_info_list.begin(); it != m_cached_info_list.end(), accumulate_failure_rate > max_failure_rate;) {
		CachedInfo cached_info = *it;
		if(cached_info.total_disk_space < cache_size) continue;
		if(cached_info.cached_name != cached_server) {
			accumulate_failure_rate *= cached_info.probability_function.getProbability(time_to_fail_minutes);
			cached_candidates += cached_info.cached_name;
			cached_candidates += ",";
		}
		std::list<CachedInfo>::iterator move = it;
		it++;
		m_cached_info_list.splice(m_cached_info_list.end(), m_cached_info_list, move);
	}
	if(!cached_candidates.empty() && cached_candidates.back() == ',') {
		cached_candidates.pop_back();
	}
	compat_classad::ClassAd policyAd;
	std::string redundancy_method = "Replication";
	policyAd.InsertAttr("RedundancyMethod", redundancy_method);
	policyAd.InsertAttr("CachedCandidates", cached_candidates);
	return policyAd;
}

void CacheflowManagerServer::CreateDummyCacheDs(DISTRIBUTION_TYPE type, int n /*1000*/) {
	for(int i = 0; i < n; ++i) {
		std::string cached_name = "cached-" + std::to_string(i);
		CachedInfo cached_info;
		cached_info.cached_name = cached_name;
		cached_info.probability_function = ProbabilityFunction(type);
		cached_info.total_disk_space = LLONG_MAX;
		ProbabilityFunction probability_function(type);
		m_cached_info_list.push_back(cached_info);
		m_cached_info_map[cached_name] = prev(m_cached_info_list.end());
	}
}
