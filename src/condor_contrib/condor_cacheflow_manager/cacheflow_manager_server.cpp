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

	compat_classad::ClassAd policyAd;
	std::vector<std::string> cached_final_list;

	double max_failure_rate;
	long long int time_to_fail_minutes;
	long long int cache_size;
	std::string method_constraint;
	std::string location_constraint;
	std::string location_blockout;
	dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, printing jobAd\n");//##
	dPrintAd(D_FULLDEBUG, jobAd);//##
	if (!jobAd.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include max_failure_rate\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, jobAd does not include max_failure_rate");
		return policyAd;
	}
	if (!jobAd.EvaluateAttrInt("TimeToFailureMinutes", time_to_fail_minutes))
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include time_to_fail_minutes\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, jobAd does not include time_to_fail_minutes");
		return policyAd;
	}
	if (!jobAd.EvaluateAttrInt("CacheSize", cache_size))
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include cache_size\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, jobAd does not include cache_size");
		return policyAd;
	}
	if (!jobAd.EvaluateAttrString("MethodConstraint", method_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include method_constraint\n");
		//TODO: find a algorithm to choose between Replication and ErasureCoding
		method_constraint = "Replication";
	}
	if (!jobAd.EvaluateAttrString("LocationConstraint", location_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include location_constraint\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, jobAd does not include location_constraint");
		return policyAd;
	}
	if (!jobAd.EvaluateAttrString("LocationBlockout", location_blockout))
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include location_blockout\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, jobAd does not include location_blockout");
		return policyAd;
	}
	dprintf(D_FULLDEBUG, "MethodConstraint = %s\n", method_constraint.c_str());//##
	dprintf(D_FULLDEBUG, "LocationConstraint = %s\n", location_constraint.c_str());//##
	dprintf(D_FULLDEBUG, "LocationBlockout = %s\n", location_blockout.c_str());//##
	dprintf(D_FULLDEBUG, "MaxFailureRate = %f\n", max_failure_rate);//##
	dprintf(D_FULLDEBUG, "TimeToFailMinutes = %f\n", time_to_fail_minutes);//##
	dprintf(D_FULLDEBUG, "CacheSize = %lld\n", cache_size);//##

	std::vector<std::string> v;
	if (location_constraint.empty())
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, location_constraint is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, location_constraint is an empty string");
		return policyAd;
	}
	if (location_constraint.find(",") == std::string::npos) {
		v.push_back(location_constraint);
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, only have one candidate %s\n", location_constraint.c_str());//##
	} else {
		boost::split(v, location_constraint, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, have multiple candidates %s\n", location_constraint.c_str());//##
	}

	std::string redundancy_method = method_constraint;
	// calculate blockout vector in LocationBlockout
	std::vector<std::string> b;
	if (location_blockout.empty())
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, location_blockout is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, location_blockout is an empty string");
		return policyAd;
	}
	if (location_blockout.find(",") == std::string::npos) {
		b.push_back(location_blockout);
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, only have one candidate %s\n", location_blockout.c_str());//##
	} else {
		boost::split(b, location_blockout, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, have multiple candidates %s\n", location_blockout.c_str());//##
	}

	double accumulate_failure_rate = 1.0;
	dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy 1, m_cached_info_map.size() = %d, m_cached_info_list.size() = %d\n", m_cached_info_map.size(), m_cached_info_list.size());//##

	// calculate accumulated failure rate of restricted locations
	std::list<CMCachedInfo>::iterator it;
	int found = 0;
	for(int i = 0; i < v.size(); ++i) {
		if(m_cached_info_map.find(v[i]) == m_cached_info_map.end()) {
			// TODO: should delete the cache on this CacheD
			dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, StorageOptimizer decided not to include this CacheD, so forget about this CacheD\n");
			continue;
		}
		found++;
		it = m_cached_info_map[v[i]];
		CMCachedInfo self_info = *it;
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, cached_name = %s, failure_rate = %f, total_disk_space = %lld\n", self_info.cached_name.c_str(), self_info.failure_rate, self_info.total_disk_space);//##
		accumulate_failure_rate *= self_info.failure_rate;
		m_cached_info_list.splice(m_cached_info_list.begin(), m_cached_info_list, it);
		cached_final_list.push_back(v[i]);
	}
	dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy 2\n");//##
	// Iterate CacheD list and find the first n CacheDs whose total failure rate is less than the required max failure rate.
	it = m_cached_info_list.begin();
	for(advance(it, found); it != m_cached_info_list.end(); ++it) {
		if(accumulate_failure_rate < max_failure_rate) break;
		CMCachedInfo cached_info = *it;
		if(cached_info.total_disk_space < cache_size) continue;
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, before accumulate_failure_rate = %f\n", accumulate_failure_rate);//##
		// redundancy manager cached does not store redundancy
		if(find(b.begin(), b.end(), cached_info.cached_name) != b.end()) continue;
		accumulate_failure_rate *= cached_info.failure_rate;
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, after accumulate_failure_rate = %f\n", accumulate_failure_rate);//##
		cached_final_list.push_back(cached_info.cached_name);
	}
	if(it == m_cached_info_list.end()) {
		policyAd.InsertAttr("NegotiateStatus", "COMPACTED");
	} else {
		policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
	}
	dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy 3\n");//##
	std::string cached_candidates;
	for(int i = 0; i < cached_final_list.size(); ++i) {
		cached_candidates += cached_final_list[i];
		cached_candidates += ",";
	}
	if(!cached_candidates.empty() && cached_candidates.back() == ',') {
		cached_candidates.pop_back();
	}
	dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy 4\n");//##
	policyAd.InsertAttr("RedundancyMethod", redundancy_method);
	policyAd.InsertAttr("CachedCandidates", cached_candidates);
	dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy 5\n");//##
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
	std::vector<struct CMCachedInfo> cached_vec;
	for(int i = 0; i < cached_servers.size(); ++i) {
		cached_info.cached_name = cached_servers[i];
		cached_info.failure_rate = failure_map[cached_servers[i]];
		cached_info.total_disk_space = capacity_map[cached_servers[i]];
		cached_vec.push_back(cached_info);
	}
	for(int i = 0; i < cached_vec.size(); ++i) {
		dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 9, %s = %f\n", cached_vec[i].cached_name.c_str(), cached_vec[i].failure_rate);//##
	}
	auto comp = [&](struct CMCachedInfo cached1, struct CMCachedInfo cached2) { return cached1.failure_rate < cached2.failure_rate; };
	std::sort(cached_vec.begin(), cached_vec.end(), comp);
	for(int i = 0; i < cached_vec.size(); ++i) {
		dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 10, %s = %f\n", cached_vec[i].cached_name.c_str(), cached_vec[i].failure_rate);//##
	}
	for(int i = 0; i < cached_vec.size(); ++i) {
		m_cached_info_list.push_back(cached_vec[i]);
		m_cached_info_map[cached_servers[i]] = prev(m_cached_info_list.end());
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 11\n");//##

	rsock->close();
	delete rsock;
	return 0;
}

