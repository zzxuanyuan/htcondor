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

#include "cacheflow_manager_server.h"
#include "dc_cacheflow_manager.h"
#include "dc_storage_optimizer.h"
#include "storage_optimizer_server.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
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
	m_update_collector_timer = -1;
	m_reaper = -1;

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

	m_update_collector_timer = daemonCore->Register_Timer (
			600,
			(TimerHandlercpp) &CacheflowManagerServer::UpdateCollector,
			"Update Collector",
			(Service*)this );
	// update collector for the first time
	UpdateCollector();

	m_reaper = daemonCore->Register_Reaper("dummy_reaper",
			(ReaperHandler) &CacheflowManagerServer::dummy_reaper,
			"dummy_reaper",NULL);

	ASSERT( m_reaper >= 0 );
	Init();
}

CacheflowManagerServer::~CacheflowManagerServer()
{
}

void CacheflowManagerServer::Init()
{
	// We need to update this function and let cacheflow_manager to pull CacheD's failure probability functions from storage_optimizer.
	// Now we just create bunch of dummy CacheDs as well as their failure probability functions for test purpose.
//	CreateDummyCacheDs(GAUSSIAN);
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
	daemonCore->Reset_Timer(m_update_collector_timer, 600);
	dprintf( D_FULLDEBUG, "exit CacheflowManager::UpdateCollector\n" );
}

int CacheflowManagerServer::Ping() {
	dprintf(D_FULLDEBUG, "entering CacheflowManagerServer::Ping()\n");
	dprintf(D_FULLDEBUG, "exiting CacheflowManagerServer::Ping()\n");
	return 0;
}

int CacheflowManagerServer::GetStoragePolicy(int /*cmd*/, Stream * sock) {

	dprintf(D_FULLDEBUG, "entering CacheflowManagerServer::GetStoragePolicy()\n");
	compat_classad::ClassAd jobAd;
	if (!getClassAd(sock, jobAd) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for GetStoragePolicy.\n");
		return PutErrorAd(sock, 1, "GetStoragePolicy", "Failed to read classad");
	}
	std::string version;
	if (!jobAd.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in GetStoragePolicy request\n");
		return PutErrorAd(sock, 1, "GetStoragePolicy", "Request missing CondorVersion attribute");
	}

	dprintf(D_FULLDEBUG, "In GetStoragePolicy, printing jobAd\n");//##
	dPrintAd(D_FULLDEBUG, jobAd);//##

	int res = GetCachedInfo(jobAd);
	if (res)
	{
		dprintf(D_ALWAYS | D_FAILURE, "GetCachedInfo return invalid code\n");
		return 1;
	}

	compat_classad::ClassAd policyAd;
	dprintf(D_FULLDEBUG, "Before NegotiateStoragePolicy().\n");
	policyAd = NegotiateStoragePolicy(jobAd);
	dPrintAd(D_FULLDEBUG, policyAd);
	dprintf(D_FULLDEBUG, "After NegotiateStoragePolicy().\n");
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
	published_classad.InsertAttr(ATTR_NAME, "cacheflow manager");

	return published_classad;
}

int CacheflowManagerServer::dummy_reaper(Service *, int pid, int) {
	dprintf(D_ALWAYS,"dummy-reaper: pid %d exited; ignored\n",pid);
	return TRUE;
}

// This is core function to assign a job's output cache data to a few CacheDs
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

	compat_classad::ClassAd policyAd;
	std::string redundancy_method = "Replication";
	std::string cached_candidates;

	double accumulate_failure_rate = 1.0;
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::NegotiateStoragePolicy 1, m_cached_info_map.size() = %d, m_cached_info_list.size() = %d\n", m_cached_info_map.size(), m_cached_info_list.size());//##
	// If the server that sent this query has CacheD daemon on it, it should be taken into account as a candidate for this cache.
	if(!cached_server.empty()) {
		CMCachedInfo self_info = *m_cached_info_map[cached_server];
		dprintf(D_FULLDEBUG, "In CacheflowManagerServer::NegotiateStoragePolicy, cached_name = %s, failure_rate = %f, total_disk_space = %lld\n", self_info.cached_name.c_str(), self_info.failure_rate, self_info.total_disk_space);//##
		accumulate_failure_rate = self_info.failure_rate;
		cached_candidates += self_info.cached_name;
		if(m_cached_info_list.size() == 1) {
			policyAd.InsertAttr("RedundancyMethod", redundancy_method);
			policyAd.InsertAttr("CachedCandidates", cached_candidates);
			return policyAd;
		}
		cached_candidates += ",";
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::NegotiateStoragePolicy 2\n");//##
	// Iterate CacheD list and find the first n CacheDs whose total failure rate is less than the required max failure rate.
	int n = m_cached_info_list.size();
	int i = 0;
	for(std::list<CMCachedInfo>::iterator it = m_cached_info_list.begin(); accumulate_failure_rate > max_failure_rate; ++i) {
		CMCachedInfo cached_info = *it;
		if(cached_info.total_disk_space < cache_size) continue;
		dprintf(D_FULLDEBUG, "In CacheflowManagerServer::NegotiateStoragePolicy, before accumulate_failure_rate = %f\n", accumulate_failure_rate);//##
		if((cached_info.cached_name == cached_server) && (i == 0)) continue;
		accumulate_failure_rate *= cached_info.failure_rate;
		cached_candidates += cached_info.cached_name;
		cached_candidates += ",";
		it++;
		dprintf(D_FULLDEBUG, "In CacheflowManagerServer::NegotiateStoragePolicy, after accumulate_failure_rate = %f\n", accumulate_failure_rate);//##
		m_cached_info_list.splice(m_cached_info_list.begin(), m_cached_info_list, it, m_cached_info_list.end());
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::NegotiateStoragePolicy 3\n");//##
	if(!cached_candidates.empty() && cached_candidates.back() == ',') {
		cached_candidates.pop_back();
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::NegotiateStoragePolicy 4\n");//##
	policyAd.InsertAttr("RedundancyMethod", redundancy_method);
	policyAd.InsertAttr("CachedCandidates", cached_candidates);
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::NegotiateStoragePolicy 5\n");//##
	return policyAd;
}

int CacheflowManagerServer::GetCachedInfo(compat_classad::ClassAd& jobAd) {
	// Query the collector for the cached
	CollectorList* collectors = daemonCore->getCollectorList();
	CondorQuery query(ANY_AD);
	query.addANDConstraint("StorageOptimizerServer =?= TRUE");
	ClassAdList soList;
	QueryResult result = collectors->query(query, soList, NULL);
	dprintf(D_FULLDEBUG, "Got %i storage optimizers from query\n", soList.Length());

	if(soList.Length() > 1) {
		dprintf(D_FULLDEBUG, "There are %d \n", soList.Length());
	}

	ClassAd *so;
	soList.Open();
	// Loop through the caches and the cached's and attempt to match.
	so = soList.Next();
	Daemon so_daemon(so, DT_GENERIC, NULL);
	if(!so_daemon.locate()) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Located daemon at %s\n", so_daemon.name());
	}

	dprintf(D_FULLDEBUG, "CacheflowManagerServer::GetCachedInfo, before STORAGE_OPTIMIZER_GET_CACHED_INFO command\n");//##
	ReliSock *rsock = (ReliSock *)so_daemon.startCommand(
					STORAGE_OPTIMIZER_GET_CACHED_INFO, Stream::reli_sock, 20 );
	dprintf(D_FULLDEBUG, "CacheflowManagerServer::GetCachedInfo, after STORAGE_OPTIMIZER_GET_CACHED_INFO command\n");//##
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 1\n");//##
	if (!rsock)
	{
		dprintf(D_FULLDEBUG, "In GetCachedInfo, cannot establish valid rsock\n");
		return 1;
	}

	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 2\n");//##
	if (!putClassAd(rsock, jobAd) || !rsock->end_of_message())
	{
		delete rsock;
		dprintf(D_FULLDEBUG, "In GetCachedInfo, failed to send request to storage optimizer\n");
		return 1;
	}

	compat_classad::ClassAd ad;
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 3\n");//##
	
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		dprintf(D_FULLDEBUG, "In GetCachedInfo, failed to receive response to storage optimizer\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 4\n");//##

	// store failure rates and total disk space information from Storage Optimizer
	std::unordered_map<std::string, double> failure_map;
	std::unordered_map<std::string, long long int> capacity_map;

	// failureRates should be a string coming from Storage Optimizer with format: cached-1@condor1=0.1,cached-2@condor2=0.2,...
	std::string failureRates;
	if (!ad.EvaluateAttrString("FailureRates", failureRates))
	{
		delete rsock;
		dprintf(D_FULLDEBUG, "In GetCachedInfo, failed to receive response to storage optimizer\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 5, FailureRates=%s\n", failureRates.c_str());//##
	std::vector<std::string> cached_servers;
	std::vector<std::string> failure_rates;
	if (failureRates.empty())
	{
		delete rsock;
		dprintf(D_FULLDEBUG, "In GetCachedInfo, failed to receive response to storage optimizer\n");
		return 1;
	}
	boost::split(failure_rates, failureRates, boost::is_any_of(", "));
	std::vector<std::string> pair;
	for(int i = 0; i < failure_rates.size(); ++i) {
		boost::split(pair, failure_rates[i], boost::is_any_of("="));
		cached_servers.push_back(pair[0]);
		failure_map[pair[0]] = boost::lexical_cast<double>(pair[1]);
		pair.clear();
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 6, failure_rates.size() = %d\n", failure_rates.size());//##
	// storageCapacities should be a string coming from Storage Optimizer with format: cached-1@condor1=100000,cached-2@condor2=200000,...
	std::string storageCapacities;
	if (!ad.EvaluateAttrString("StorageCapacities", storageCapacities))
	{
		delete rsock;
		dprintf(D_FULLDEBUG, "In GetCachedInfo, failed to receive response to storage optimizer\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 7\n");//##
	std::vector<std::string> storage_capacities;
	if (storageCapacities.empty())
	{
		delete rsock;
		dprintf(D_FULLDEBUG, "In GetCachedInfo, failed to receive response to storage optimizer\n");
		return 1;
	}
	boost::split(storage_capacities, storageCapacities, boost::is_any_of(", "));
	for(int i = 0; i < storage_capacities.size(); ++i) {
		boost::split(pair, storage_capacities[i], boost::is_any_of("="));
		capacity_map[pair[0]] = boost::lexical_cast<long long int>(pair[1]);
		pair.clear();
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 8, storage_capacities = %d\n", storage_capacities.size());//##

	CMCachedInfo cached_info;
	m_cached_info_list.clear();
	m_cached_info_map.clear();
	for(int i = 0; i < cached_servers.size(); ++i) {
		cached_info.cached_name = cached_servers[i];
		cached_info.failure_rate = failure_map[cached_servers[i]];
		cached_info.total_disk_space = capacity_map[cached_servers[i]];
		m_cached_info_list.push_back(cached_info);
		m_cached_info_map[cached_servers[i]] = prev(m_cached_info_list.end());
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 9\n");//##

	rsock->close();
	delete rsock;
	return 0;
}
