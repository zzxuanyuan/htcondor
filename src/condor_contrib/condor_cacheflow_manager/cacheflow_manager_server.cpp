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

	double max_failure_rate;
	long long int time_to_fail_minutes;
	long long int cache_size;
	std::string method_constraint;
	int data_number_constraint = -1;
	int parity_number_constraint = -1;
	std::string selection_constraint;
	std::string flexibility_constraint;
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
	if (!jobAd.EvaluateAttrString("SelectionConstraint", selection_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include selection_constraint\n");
		//TODO: find a algorithm to choose between Sorted and Random
		selection_constraint = "Sorted";
	}
	if (!jobAd.EvaluateAttrString("FlexibilityConstraint", flexibility_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include flexibility_constraint\n");
		//TODO: find a algorithm to choose between Sorted and Random
		flexibility_constraint = "Dynamic";
	}
	if (!jobAd.EvaluateAttrInt("DataNumberConstraint", data_number_constraint))
	{
		// keep data_number_constraint as -1
		data_number_constraint = -1;
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include data_number_constraint\n");
	}
	if (!jobAd.EvaluateAttrInt("ParityNumberConstraint", parity_number_constraint))
	{
		// keep parity_number_constraint as -1
		parity_number_constraint = -1;
		dprintf(D_FULLDEBUG, "In NegotiateStoragePolicy, jobAd does not include parity_number_constraint\n");
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
	dprintf(D_FULLDEBUG, "DataNumberConstraint = %d\n", data_number_constraint);//##
	dprintf(D_FULLDEBUG, "ParityNumberConstraint = %d\n", parity_number_constraint);//##

	if(method_constraint == "Replication" && selection_constraint == "Sorted") {
		policyAd = SortedReplication(max_failure_rate, time_to_fail_minutes, cache_size, location_constraint, location_blockout, data_number_constraint, parity_number_constraint, flexibility_constraint);
	} else if(method_constraint == "Replication" && selection_constraint == "Random") {
		policyAd = LocalzReplication(max_failure_rate, time_to_fail_minutes, cache_size, location_constraint, location_blockout, data_number_constraint, parity_number_constraint, flexibility_constraint);
	} else if(method_constraint == "ErasureCoding" && selection_constraint == "Sorted") {
		policyAd = SortedErasureCoding(max_failure_rate, time_to_fail_minutes, cache_size, location_constraint, location_blockout, data_number_constraint, parity_number_constraint, flexibility_constraint);
	} else if(method_constraint == "ErasureCoding" && selection_constraint == "Random") {
		policyAd = RandomErasureCoding(max_failure_rate, time_to_fail_minutes, cache_size, location_constraint, location_blockout, data_number_constraint, parity_number_constraint, flexibility_constraint);
	} else {
		dprintf(D_FULLDEBUG, "method_constraint or selection_constraint is not valid!\n");
	}
	return policyAd;
}

compat_classad::ClassAd CacheflowManagerServer::SortedReplication(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint) {

	compat_classad::ClassAd policyAd;

	std::string redundancy_method = "Replication";
	std::string redundancy_selection = "Sorted";
	std::string redundancy_flexibility = flexibility_constraint;
	policyAd.InsertAttr("RedundancyMethod", redundancy_method);
	policyAd.InsertAttr("RedundancySelection", redundancy_selection);
	policyAd.InsertAttr("RedundancyFlexibility", redundancy_flexibility);

	std::vector<std::string> cached_final_list;
	// Step 1: get all existing CacheDs that store replicas.
	std::vector<std::string> v;
	if (location_constraint.empty())
	{
		dprintf(D_FULLDEBUG, "In Replication, location_constraint is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedReplication, location_constraint is an empty string");
		return policyAd;
	}
	if (location_constraint.find(",") == std::string::npos) {
		v.push_back(location_constraint);
		dprintf(D_FULLDEBUG, "In SortedReplication, only have one location_constraint %s\n", location_constraint.c_str());//##
	} else {
		boost::split(v, location_constraint, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In SortedReplication, have multiple location_constraint %s\n", location_constraint.c_str());//##
	}

	// Step 2: calculate accumulated failure rate of restricted locations."found" keeps a record of real existing replicas which are a subset of vector v.
	double accumulate_failure_rate = 1.0;
	dprintf(D_FULLDEBUG, "In SortedReplication 1, m_cached_info_map.size() = %d, m_cached_info_list.size() = %d\n", m_cached_info_map.size(), m_cached_info_list.size());//##
	std::list<CMCachedInfo>::iterator it;
	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
		dprintf(D_FULLDEBUG, "In SortedReplication, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
	}
	int found = 0;
	for(int i = 0; i < v.size(); ++i) {
		if(m_cached_info_map.find(v[i]) == m_cached_info_map.end()) {
			// TODO: should delete the cache on this CacheD
			dprintf(D_FULLDEBUG, "In SortedReplication, StorageOptimizer decided not to include this CacheD, so forget about this CacheD\n");
			continue;
		}
		found++;
		it = m_cached_info_map[v[i]];
		CMCachedInfo self_info = *it;
		dprintf(D_FULLDEBUG, "In SortedReplication, cached_name = %s, failure_rate = %f, total_disk_space = %lld\n", self_info.cached_name.c_str(), self_info.failure_rate, self_info.total_disk_space);//##
		accumulate_failure_rate *= self_info.failure_rate;
		m_cached_info_list.splice(m_cached_info_list.begin(), m_cached_info_list, it);
		cached_final_list.push_back(v[i]);
	}

	// Step 3: check corner cases and calculate how many replicas we still need - "left_number".
	int left_number = -1;
	if(data_number_constraint == -1 && parity_number_constraint != -1) {
		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedReplication, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint != -1 && parity_number_constraint == -1) {
		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedReplication, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint != -1 && parity_number_constraint != 0) {
		// If data_number_constraint has been defined already, parity_number_constraint must be 0. Because we are using replication.
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedReplication, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint == -1 && parity_number_constraint == -1) {
		left_number = INT_MAX;
	} else if(data_number_constraint != -1 && parity_number_constraint != -1 && redundancy_flexibility == "Dynamic") {
		left_number = INT_MAX;
	} else {
		dprintf(D_FULLDEBUG, "SortedReplication, data_number_constraint = %d, parity_number_constraint = %d\n", data_number_constraint, parity_number_constraint);
		left_number = data_number_constraint - found;
	}

	// Step 4: check left_number.
	if(left_number < 0) {
		// the number of existing replicas has been larger than the number of required number of replicas.
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedReplication, there exists more than reuqired number of replicas");
		return policyAd;
	} else if(left_number == 0) {
		// the number of existing replicas exactly matches the the number of required replicas.
		policyAd.InsertAttr("CachedCandidates", location_constraint);
		policyAd.InsertAttr("DataNumber", data_number_constraint);
		policyAd.InsertAttr("ParityNumber", 0);
		policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		return policyAd;
	} else {
		dprintf(D_FULLDEBUG, "In SortedReplication, left_number = %d\n", left_number); 
	}

	// Step 5: calculate blockout vector in LocationBlockout
	std::vector<std::string> b;
	if (location_blockout.empty())
	{
		dprintf(D_FULLDEBUG, "In SortedReplication, location_blockout is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedReplication, location_blockout is an empty string");
		return policyAd;
	}
	if (location_blockout.find(",") == std::string::npos) {
		b.push_back(location_blockout);
		dprintf(D_FULLDEBUG, "In SortedReplication, only have one location_blockout %s\n", location_blockout.c_str());//##
	} else {
		boost::split(b, location_blockout, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In SortedReplication, have multiple location_blockout %s\n", location_blockout.c_str());//##
	}

	dprintf(D_FULLDEBUG, "In SortedReplication 2\n");//##
	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
		dprintf(D_FULLDEBUG, "In SortedReplication 2, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
	}

	// Step 6: iterate CacheD list and find the first n CacheDs
	// a. whose total failure rate is less than the required max failure rate;
	// b. where n reaches the number of left_number.
	it = m_cached_info_list.begin();
	int idx = 0;
	for(advance(it, found); it != m_cached_info_list.end() && idx < left_number; ++it) {
		// if there is no restriction on parity_number, we stop when accumulate_failure_rate goes below max_failure_rate.
		if((accumulate_failure_rate < max_failure_rate) && (left_number == INT_MAX)) break;
		CMCachedInfo cached_info = *it;
		if(cached_info.total_disk_space < cache_size) continue;
		dprintf(D_FULLDEBUG, "In SortedReplication, before accumulate_failure_rate = %f\n", accumulate_failure_rate);//##
		// redundancy manager cached does not store redundancy
		if(find(b.begin(), b.end(), cached_info.cached_name) != b.end()) continue;
		accumulate_failure_rate *= cached_info.failure_rate;
		dprintf(D_FULLDEBUG, "In SortedReplication, after accumulate_failure_rate = %f\n", accumulate_failure_rate);//##
		cached_final_list.push_back(cached_info.cached_name);
		idx++;
	}

	// Step 7: prepare return policy ad.
	if(it == m_cached_info_list.end()) {
		policyAd.InsertAttr("NegotiateStatus", "COMPACTED");
	} else {
		policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
	}
	dprintf(D_FULLDEBUG, "In SortedReplication 3\n");//##
	std::string cached_candidates;
	for(int i = 0; i < cached_final_list.size(); ++i) {
		cached_candidates += cached_final_list[i];
		cached_candidates += ",";
	}
	if(!cached_candidates.empty() && cached_candidates.back() == ',') {
		cached_candidates.pop_back();
	}
	dprintf(D_FULLDEBUG, "In SortedReplication 4\n");//##
	policyAd.InsertAttr("CachedCandidates", cached_candidates);
	int ndata = cached_final_list.size();
	policyAd.InsertAttr("DataNumber", ndata);
	policyAd.InsertAttr("ParityNumber", 0);
	dprintf(D_FULLDEBUG, "In SortedReplication 5\n");//##
	return policyAd;
}

compat_classad::ClassAd CacheflowManagerServer::LocalzReplication(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint) {

  	compat_classad::ClassAd policyAd;

  	dprintf(D_FULLDEBUG, "In LocalzReplication 1\n");//##
 	std::string redundancy_method = "Replication";
 	std::string redundancy_selection = "Random";
 	std::string redundancy_flexibility = flexibility_constraint;
 	policyAd.InsertAttr("RedundancyMethod", redundancy_method);
 	policyAd.InsertAttr("RedundancySelection", redundancy_selection);
 	policyAd.InsertAttr("RedundancyFlexibility", redundancy_flexibility);

  	std::vector<std::string> cached_final_list;

 	std::vector<std::string> v;
 	if (location_constraint.empty())
 	{
 		dprintf(D_FULLDEBUG, "In LocalzReplication, location_constraint is an empty string\n");
 		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
 		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, location_constraint is an empty string");
 		return policyAd;
 	}
 	if (location_constraint.find(",") == std::string::npos) {
 		v.push_back(location_constraint);
 		dprintf(D_FULLDEBUG, "In LocalzReplication, only have one location_constraint %s\n", location_constraint.c_str());//##
 	} else {
 		boost::split(v, location_constraint, boost::is_any_of(", "));
 		dprintf(D_FULLDEBUG, "In LocalzReplication, have multiple location_constraint %s\n", location_constraint.c_str());//##
 	}

 	double accumulate_failure_rate = 1.0;
 	dprintf(D_FULLDEBUG, "In LocalzReplication, m_cached_info_map.size() = %d, m_cached_info_list.size() = %d\n", m_cached_info_map.size(), m_cached_info_list.size());//##
 	std::list<CMCachedInfo>::iterator it;
 	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
 		dprintf(D_FULLDEBUG, "In LocalzReplication, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
 	}
 	int found = 0;
 	int worker1_cnt = 0;
 	int worker2_cnt = 0;
 	int worker3_cnt = 0;
 	int worker4_cnt = 0;
 	std::string worker1 = "condorworker1";
 	std::string worker2 = "condorworker2";
 	std::string worker3 = "condorworker3";
 	std::string worker4 = "condorworker4";
 	for(int i = 0; i < v.size(); ++i) {
 		if(m_cached_info_map.find(v[i]) == m_cached_info_map.end()) {
 			dprintf(D_FULLDEBUG, "In LocalzReplication, StorageOptimizer decided not to include this CacheD, so forget about this CacheD\n");
 			continue;
 		}
 		found++;
 		it = m_cached_info_map[v[i]];
 		CMCachedInfo self_info = *it;
 		if(self_info.cached_name.find(worker1) != std::string::npos) {
 			worker1_cnt++;
 		} else if(self_info.cached_name.find(worker2) != std::string::npos) {
 			worker2_cnt++;
 		} else if(self_info.cached_name.find(worker3) != std::string::npos) {
 			worker3_cnt++;
 		} else if(self_info.cached_name.find(worker4) != std::string::npos) {
 			worker4_cnt++;
 		} else {
 			dprintf(D_FULLDEBUG, "In LocalzReplication, no such worker name\n");
 		}
 		dprintf(D_FULLDEBUG, "In LocalzReplication, cached_name = %s, failure_rate = %f, total_disk_space = %lld\n", self_info.cached_name.c_str(), self_info.failure_rate, self_info.total_disk_space);//##
 		time_t now = time(NULL);
 		network_transfer_fs << now << ", " << "random replication, restrictedcached = " << self_info.cached_name.c_str() << ", " << "failurerate = " << self_info.failure_rate << std::endl;
 		accumulate_failure_rate *= self_info.failure_rate;
 		m_cached_info_list.splice(m_cached_info_list.begin(), m_cached_info_list, it);
 		cached_final_list.push_back(v[i]);
 	}
 	std::vector<std::pair<std::string, int>> pair_vec;
 	std::pair<std::string, int> pair1 = std::make_pair(worker1, worker1_cnt);
 	std::pair<std::string, int> pair2 = std::make_pair(worker2, worker2_cnt);
 	std::pair<std::string, int> pair3 = std::make_pair(worker3, worker3_cnt);
 	std::pair<std::string, int> pair4 = std::make_pair(worker4, worker4_cnt);
 	pair_vec.push_back(pair1);
 	pair_vec.push_back(pair2);
 	pair_vec.push_back(pair3);
 	pair_vec.push_back(pair4);
 	auto comp = [&](std::pair<std::string, int> p1, std::pair<std::string, int> p2) { return p1.second > p2.second; };
 	std::sort(pair_vec.begin(), pair_vec.end(), comp);

 	int left_number = -1;
 	if(data_number_constraint == -1 && parity_number_constraint != -1) {
 		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
 		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
 		policyAd.InsertAttr(ATTR_ERROR_STRING, "In LocalzReplication, data_number_constraint and parity_number_constraint are not a valid pair");
 		return policyAd;
 	} else if(data_number_constraint != -1 && parity_number_constraint == -1) {
 		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
 		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
 		policyAd.InsertAttr(ATTR_ERROR_STRING, "In LocalzReplication, data_number_constraint and parity_number_constraint are not a valid pair");
 		return policyAd;
 	} else if(data_number_constraint != -1 && parity_number_constraint != 0) {
 		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
 		policyAd.InsertAttr(ATTR_ERROR_STRING, "In LocalzReplication, data_number_constraint and parity_number_constraint are not a valid pair");
 		return policyAd;
 	} else if(data_number_constraint == -1 && parity_number_constraint == -1) {
 		left_number = INT_MAX;
 	} else if(data_number_constraint != -1 && parity_number_constraint != -1 && redundancy_flexibility == "Dynamic") {
 		left_number = INT_MAX;
 	} else {
 		dprintf(D_FULLDEBUG, "In LocalzReplication, data_number_constraint = %d, parity_number_constraint = %d\n", data_number_constraint, parity_number_constraint);
 		left_number = data_number_constraint - found;
 	}

 	if(left_number < 0) {
 		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
 		policyAd.InsertAttr(ATTR_ERROR_STRING, "In LocalzReplication, there exists more than reuqired number of replicas");
 		return policyAd;
 	} else if(left_number == 0) {
 		policyAd.InsertAttr("CachedCandidates", location_constraint);
 		policyAd.InsertAttr("DataNumber", data_number_constraint);
 		policyAd.InsertAttr("ParityNumber", 0);
 		policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
 		return policyAd;
 	} else {
 		dprintf(D_FULLDEBUG, "In LocalzReplication, left_number = %d\n", left_number);
 	}

 	std::vector<std::string> b;
 	if (location_blockout.empty())
 	{
 		dprintf(D_FULLDEBUG, "In LocalzReplication, location_blockout is an empty string\n");
 		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
 		policyAd.InsertAttr(ATTR_ERROR_STRING, "In LocalzReplication, location_blockout is an empty string");
 		return policyAd;
 	}
 	if (location_blockout.find(",") == std::string::npos) {
 		b.push_back(location_blockout);
 		dprintf(D_FULLDEBUG, "In LocalzReplication, only have one location_blockout %s\n", location_blockout.c_str());//##
 	} else {
 		boost::split(b, location_blockout, boost::is_any_of(", "));
 		dprintf(D_FULLDEBUG, "In LocalzReplication, have multiple location_blockout %s\n", location_blockout.c_str());//##
 	}

  	dprintf(D_FULLDEBUG, "In LocalzReplication 2\n");//##
 	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
 		dprintf(D_FULLDEBUG, "In LocalzReplication 2, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
 	}

 	it = m_cached_info_list.begin();
 	int valid_count = 0;
 	std::vector<CMCachedInfo> valid_vector;
 	for(advance(it, found); it != m_cached_info_list.end(); ++it) {
 		CMCachedInfo cached_info = *it;
 		if(cached_info.total_disk_space < cache_size) continue;
 		if(find(b.begin(), b.end(), cached_info.cached_name) != b.end()) continue;
 		valid_vector.push_back(cached_info);
 	}

 	int valid1_cnt = 0;
 	int valid2_cnt = 0;
 	int valid3_cnt = 0;
 	int valid4_cnt = 0;
 	std::string valid1 = pair_vec[0].first;
 	std::string valid2 = pair_vec[1].first;
 	std::string valid3 = pair_vec[2].first;
 	std::string valid4 = pair_vec[3].first;
 	for(int i = 0; i < valid_vector.size(); ++i) {
 		if(valid_vector[i].cached_name.find(valid1) != std::string::npos) {
 			valid1_cnt++;
 		} else if(valid_vector[i].cached_name.find(valid2) != std::string::npos) {
 			valid2_cnt++;
 		} else if(valid_vector[i].cached_name.find(valid3) != std::string::npos) {
 			valid3_cnt++;
 		} else if(valid_vector[i].cached_name.find(valid4) != std::string::npos) {
 			valid4_cnt++;
 		} else {
 			dprintf(D_FULLDEBUG, "In LocalzReplication, no such worker name 2\n");
 		}
 	}
 	int idx1 = 0;
 	int idx2 = idx1 + valid1_cnt;
 	int idx3 = idx2 + valid2_cnt;
 	int idx4 = idx3 + valid3_cnt;
 	std::vector<CMCachedInfo> valid_tmp;
 	CMCachedInfo empty_info;
 	empty_info.cached_name = "";
 	empty_info.failure_rate = 0.0;
 	empty_info.total_disk_space = 0;
 	for(int i = 0; i < valid_vector.size(); ++i) {
 		valid_tmp.push_back(empty_info);
 	}
 	for(int i = 0; i < valid_vector.size(); ++i) {
 		if(valid_vector[i].cached_name.find(valid1) != std::string::npos) {
 			valid_tmp[idx1++] = valid_vector[i];
 		} else if(valid_vector[i].cached_name.find(valid2) != std::string::npos) {
 			valid_tmp[idx2++] = valid_vector[i];
 		} else if(valid_vector[i].cached_name.find(valid3) != std::string::npos) {
 			valid_tmp[idx3++] = valid_vector[i];
 		} else if(valid_vector[i].cached_name.find(valid4) != std::string::npos) {
 			valid_tmp[idx4++] = valid_vector[i];
 		} else {
 			dprintf(D_FULLDEBUG, "In LocalzReplication, no such worker name 2\n");
 		}
 	}
 	valid_vector.swap(valid_tmp);

 	if(left_number != INT_MAX) {
 		if(left_number > valid_vector.size()) {
 			dprintf(D_FULLDEBUG, "In LocalzReplication, not enough valid pilot for this cache\n");
 			policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
 			policyAd.InsertAttr(ATTR_ERROR_STRING, "In LocalzReplication, not enough valid pilot for this cache\n");
 			return policyAd;
 		} else if(left_number == valid_vector.size()) {
 			for(int i = 0; i < valid_vector.size(); ++i) {
 				time_t now = time(NULL);
 				network_transfer_fs << now << ", " << "random replication, selectedcached = " << valid_vector[i].cached_name.c_str() << ", " << "failurerate = " << valid_vector[i].failure_rate << std::endl;
 				cached_final_list.push_back(valid_vector[i].cached_name);
 			}
 			policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
 		} else {
 			for(int idx = 0; idx < left_number; ++idx) {
 				time_t now = time(NULL);
 				network_transfer_fs << now << ", " << "random replication, selectedcached = " << valid_vector[idx].cached_name.c_str() << ", " << "failurerate = " << valid_vector[idx].failure_rate << std::endl;
 			}
 			policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
 		}
 	} else {
 		int idx = 0;
 		while((accumulate_failure_rate > max_failure_rate) && (idx < valid_vector.size())) {
 			time_t now = time(NULL);
 			network_transfer_fs << now << ", " << "random replication, selectedcached = " << valid_vector[idx].cached_name.c_str() << ", " << "failurerate = " << valid_vector[idx].failure_rate << std::endl;
 			cached_final_list.push_back(valid_vector[idx].cached_name);
 			accumulate_failure_rate *= valid_vector[idx].failure_rate;
 			idx++;
 		}
 		if(accumulate_failure_rate > max_failure_rate) {
 			policyAd.InsertAttr("NegotiateStatus", "COMPACTED");
 		} else {
 			policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
 		}
 	}

 	dprintf(D_FULLDEBUG, "In LocalzReplication 3\n");//##
 	std::string cached_candidates;
 	for(int i = 0; i < cached_final_list.size(); ++i) {
 		cached_candidates += cached_final_list[i];
 		cached_candidates += ",";
 	}
 	if(!cached_candidates.empty() && cached_candidates.back() == ',') {
 		cached_candidates.pop_back();
 	}
 	dprintf(D_FULLDEBUG, "In LocalzReplication 4\n");//##
 	policyAd.InsertAttr("CachedCandidates", cached_candidates);
 	int ndata = cached_final_list.size();
 	policyAd.InsertAttr("DataNumber", ndata);
 	policyAd.InsertAttr("ParityNumber", 0);
 	dprintf(D_FULLDEBUG, "In LocalzReplication 5\n");//##
 	return policyAd;
}

compat_classad::ClassAd CacheflowManagerServer::RandomReplication(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint) {

	compat_classad::ClassAd policyAd;

	dprintf(D_FULLDEBUG, "In RandomReplication 1\n");//##
	std::string redundancy_method = "Replication";
	std::string redundancy_selection = "Random";
	std::string redundancy_flexibility = flexibility_constraint;
	policyAd.InsertAttr("RedundancyMethod", redundancy_method);
	policyAd.InsertAttr("RedundancySelection", redundancy_selection);
	policyAd.InsertAttr("RedundancyFlexibility", redundancy_flexibility);

	std::vector<std::string> cached_final_list;
	// Step 1: get all existing CacheDs that store replicas.
	std::vector<std::string> v;
	if (location_constraint.empty())
	{
		dprintf(D_FULLDEBUG, "In RandomReplication, location_constraint is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In NegotiateStoragePolicy, location_constraint is an empty string");
		return policyAd;
	}
	if (location_constraint.find(",") == std::string::npos) {
		v.push_back(location_constraint);
		dprintf(D_FULLDEBUG, "In RandomReplication, only have one location_constraint %s\n", location_constraint.c_str());//##
	} else {
		boost::split(v, location_constraint, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In RandomReplication, have multiple location_constraint %s\n", location_constraint.c_str());//##
	}

	// Step 2: calculate accumulated failure rate of restricted locations."found" keeps a record of real existing replicas which are a subset of vector v.
	double accumulate_failure_rate = 1.0;
	dprintf(D_FULLDEBUG, "In RandomReplication, m_cached_info_map.size() = %d, m_cached_info_list.size() = %d\n", m_cached_info_map.size(), m_cached_info_list.size());//##
	std::list<CMCachedInfo>::iterator it;
	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
		dprintf(D_FULLDEBUG, "In RandomReplication, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
	}
	int found = 0;
	for(int i = 0; i < v.size(); ++i) {
		if(m_cached_info_map.find(v[i]) == m_cached_info_map.end()) {
			// TODO: should delete the cache on this CacheD
			dprintf(D_FULLDEBUG, "In RandomReplication, StorageOptimizer decided not to include this CacheD, so forget about this CacheD\n");
			continue;
		}
		found++;
		it = m_cached_info_map[v[i]];
		CMCachedInfo self_info = *it;
		dprintf(D_FULLDEBUG, "In RandomReplication, cached_name = %s, failure_rate = %f, total_disk_space = %lld\n", self_info.cached_name.c_str(), self_info.failure_rate, self_info.total_disk_space);//##
		accumulate_failure_rate *= self_info.failure_rate;
		m_cached_info_list.splice(m_cached_info_list.begin(), m_cached_info_list, it);
		cached_final_list.push_back(v[i]);
	}

	// Step 3: check corner cases and calculate how many replicas we still need - "left_number".
	int left_number = -1;
	if(data_number_constraint == -1 && parity_number_constraint != -1) {
		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomReplication, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint != -1 && parity_number_constraint == -1) {
		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomReplication, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint != -1 && parity_number_constraint != 0) {
		// If data_number_constraint has been defined already, parity_number_constraint must be 0. Because we are using replication.
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomReplication, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint == -1 && parity_number_constraint == -1) {
		left_number = INT_MAX;
	} else if(data_number_constraint != -1 && parity_number_constraint != -1 && redundancy_flexibility == "Dynamic") {
		left_number = INT_MAX;
	} else {
		dprintf(D_FULLDEBUG, "In RandomReplication, data_number_constraint = %d, parity_number_constraint = %d\n", data_number_constraint, parity_number_constraint);
		left_number = data_number_constraint - found;
	}

	// Step 4: check left_number.
	if(left_number < 0) {
		// the number of existing replicas has been larger than the number of required number of replicas.
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomReplication, there exists more than reuqired number of replicas");
		return policyAd;
	} else if(left_number == 0) {
		// the number of existing replicas exactly matches the the number of required replicas.
		policyAd.InsertAttr("CachedCandidates", location_constraint);
		policyAd.InsertAttr("DataNumber", data_number_constraint);
		policyAd.InsertAttr("ParityNumber", 0);
		policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		return policyAd;
	} else {
		dprintf(D_FULLDEBUG, "In RandomReplication, left_number = %d\n", left_number);
	}

	// Step 5: calculate blockout vector in LocationBlockout
	std::vector<std::string> b;
	if (location_blockout.empty())
	{
		dprintf(D_FULLDEBUG, "In RandomReplication, location_blockout is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomReplication, location_blockout is an empty string");
		return policyAd;
	}
	if (location_blockout.find(",") == std::string::npos) {
		b.push_back(location_blockout);
		dprintf(D_FULLDEBUG, "In RandomReplication, only have one location_blockout %s\n", location_blockout.c_str());//##
	} else {
		boost::split(b, location_blockout, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In RandomReplication, have multiple location_blockout %s\n", location_blockout.c_str());//##
	}

	dprintf(D_FULLDEBUG, "In RandomReplication 2\n");//##
	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
		dprintf(D_FULLDEBUG, "In RandomReplication 2, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
	}

	// Step 6: collect all valid pilots
	it = m_cached_info_list.begin();
	int valid_count = 0;
	std::vector<CMCachedInfo> valid_vector;
	for(advance(it, found); it != m_cached_info_list.end(); ++it) {
		CMCachedInfo cached_info = *it;
		if(cached_info.total_disk_space < cache_size) continue;
		if(find(b.begin(), b.end(), cached_info.cached_name) != b.end()) continue;
		valid_vector.push_back(cached_info);
	}

	// Step 7:
	// 	case 1: left_number != INT_MAX:
	// 		a. left_number is larger than available pilots, wrong
	// 		b. left_number is equal to avaiable pilots, select all of the pilots
	//		c. left_number is less than available pilots, random select pilot until reach the required number of pilots
	//	case 2: left_number == INT_MAX:
	//		we randomly choose pilots util accumulated failure rate goes less than required failure rate:
	//		a. accumulate_failure_rate > max_failure_rate, we do not have pilots
	//		b. accumulate_failure_rate <= max_failure_rate, we succedded
	if(left_number != INT_MAX) {
		if(left_number > valid_vector.size()) {
			dprintf(D_FULLDEBUG, "In RandomReplication, not enough valid pilot for this cache\n");
			policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
			policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomReplication, not enough valid pilot for this cache\n");
			return policyAd;
		} else if(left_number == valid_vector.size()) {
			for(int i = 0; i < valid_vector.size(); ++i) {
				cached_final_list.push_back(valid_vector[i].cached_name);
			}
			policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		} else {
			int count = 0;
			while(count < left_number) {
				int n = valid_vector.size() - count;
				int idx = rand()%n;
				cached_final_list.push_back(valid_vector[idx].cached_name);
				valid_vector.erase(valid_vector.begin()+idx);
				count++;
			}
			policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		}
	} else {
		int count = 0;
		while((accumulate_failure_rate > max_failure_rate) && !valid_vector.empty()) {
			int n = valid_vector.size() - count;
			int idx = rand()%n;
			cached_final_list.push_back(valid_vector[idx].cached_name);
			accumulate_failure_rate *= valid_vector[idx].failure_rate;
			valid_vector.erase(valid_vector.begin()+idx);
			count++;
		}
		if(accumulate_failure_rate > max_failure_rate) {
			policyAd.InsertAttr("NegotiateStatus", "COMPACTED");
		} else {
			policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		}
	}
	
	// Step 7: prepare return policy ad.
	dprintf(D_FULLDEBUG, "In RandomReplication 3\n");//##
	std::string cached_candidates;
	for(int i = 0; i < cached_final_list.size(); ++i) {
		cached_candidates += cached_final_list[i];
		cached_candidates += ",";
	}
	if(!cached_candidates.empty() && cached_candidates.back() == ',') {
		cached_candidates.pop_back();
	}
	dprintf(D_FULLDEBUG, "In RandomReplication 4\n");//##
	policyAd.InsertAttr("CachedCandidates", cached_candidates);
	int ndata = cached_final_list.size();
	policyAd.InsertAttr("DataNumber", ndata);
	policyAd.InsertAttr("ParityNumber", 0);
	dprintf(D_FULLDEBUG, "In RandomReplication 5\n");//##
	return policyAd;
}

compat_classad::ClassAd CacheflowManagerServer::SortedErasureCoding(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint) {
	compat_classad::ClassAd policyAd;

	dprintf(D_FULLDEBUG, "In SortedErasureCoding 1\n");//##
	std::string redundancy_method = "ErasureCoding";
	std::string redundancy_selection = "Sorted";
	std::string redundancy_flexibility = flexibility_constraint;
	policyAd.InsertAttr("RedundancyMethod", redundancy_method);
	policyAd.InsertAttr("RedundancySelection", redundancy_selection);
	policyAd.InsertAttr("RedundancyFlexibility", redundancy_flexibility);

	std::vector<std::string> cached_final_list;
	// Step 1: get all existing CacheDs that store replicas.
	std::vector<std::string> v;
	if (location_constraint.empty())
	{
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, location_constraint is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedErasureCoding, location_constraint is an empty string");
		return policyAd;
	}
	if (location_constraint.find(",") == std::string::npos) {
		v.push_back(location_constraint);
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, only have one location_constraint %s\n", location_constraint.c_str());//##
	} else {
		boost::split(v, location_constraint, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, have multiple location_constraint %s\n", location_constraint.c_str());//##
	}

	// Step 2: calculate accumulated failure rate of restricted locations."found" keeps a record of real existing replicas which are a subset of vector v.
	dprintf(D_FULLDEBUG, "In SortedErasureCoding, m_cached_info_map.size() = %d, m_cached_info_list.size() = %d\n", m_cached_info_map.size(), m_cached_info_list.size());//##
	std::list<CMCachedInfo>::iterator it;
	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
	}
	int found = 0;
	for(int i = 0; i < v.size(); ++i) {
		if(m_cached_info_map.find(v[i]) == m_cached_info_map.end()) {
			// TODO: should delete the cache on this CacheD
			dprintf(D_FULLDEBUG, "In SortedErasureCoding, StorageOptimizer decided not to include this CacheD, so forget about this CacheD\n");
			continue;
		}
		found++;
		it = m_cached_info_map[v[i]];
		CMCachedInfo self_info = *it;
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, cached_name = %s, failure_rate = %f, total_disk_space = %lld\n", self_info.cached_name.c_str(), self_info.failure_rate, self_info.total_disk_space);//##
		m_cached_info_list.splice(m_cached_info_list.begin(), m_cached_info_list, it);
		cached_final_list.push_back(v[i]);
	}

	// Step 3: check corner cases and calculate how many replicas we still need - "left_number".
	int left_number = -1;
	if(data_number_constraint == -1 && parity_number_constraint != -1) {
		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedErasureCoding, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint != -1 && parity_number_constraint == -1) {
		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedErasureCoding, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint == -1 && parity_number_constraint == -1) {
		dprintf(D_FULLDEBUG, "We are not supporting dynamic erasure coding\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedErasureCoding, we are not supporting dynamic erasure coding");
		return policyAd;
	} else {
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, data_number_constraint = %d, parity_number_constraint = %d\n", data_number_constraint, parity_number_constraint);
		left_number = data_number_constraint + parity_number_constraint - found;
	}

	// Step 4: check left_number.
	if(left_number < 0) {
		// the number of existing replicas has been larger than the number of required number of replicas.
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedErasureCoding, there exists more than reuqired number of replicas");
		return policyAd;
	} else if(left_number == 0) {
		// the number of existing replicas exactly matches the the number of required replicas.
		policyAd.InsertAttr("CachedCandidates", location_constraint);
		policyAd.InsertAttr("DataNumber", data_number_constraint);
		policyAd.InsertAttr("ParityNumber", parity_number_constraint);
		policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		return policyAd;
	} else {
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, left_number = %d\n", left_number);
	}

	// Step 5: calculate blockout vector in LocationBlockout
	std::vector<std::string> b;
	if (location_blockout.empty())
	{
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, location_blockout is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In SortedErasureCoding, location_blockout is an empty string");
		return policyAd;
	}
	if (location_blockout.find(",") == std::string::npos) {
		b.push_back(location_blockout);
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, only have one location_blockout %s\n", location_blockout.c_str());//##
	} else {
		boost::split(b, location_blockout, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In SortedErasureCoding, have multiple location_blockout %s\n", location_blockout.c_str());//##
	}

	dprintf(D_FULLDEBUG, "In SortedErasureCoding 2\n");//##
	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
		dprintf(D_FULLDEBUG, "In SortedErasureCoding 2, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
	}

	// Step 6: iterate CacheD list and find the first n CacheDs
	it = m_cached_info_list.begin();
	int idx = 0;
	for(advance(it, found); it != m_cached_info_list.end() && idx < left_number; ++it) {
		CMCachedInfo cached_info = *it;
		if(cached_info.total_disk_space < cache_size) continue;
		// redundancy manager cached does not store redundancy
		if(find(b.begin(), b.end(), cached_info.cached_name) != b.end()) continue;
		cached_final_list.push_back(cached_info.cached_name);
		idx++;
	}

	// Step 7: prepare return policy ad.
	if(it == m_cached_info_list.end()) {
		policyAd.InsertAttr("NegotiateStatus", "FAILED");
	} else {
		policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
	}
	dprintf(D_FULLDEBUG, "In SortedErasureCoding 3\n");//##
	std::string cached_candidates;
	for(int i = 0; i < cached_final_list.size(); ++i) {
		cached_candidates += cached_final_list[i];
		cached_candidates += ",";
	}
	if(!cached_candidates.empty() && cached_candidates.back() == ',') {
		cached_candidates.pop_back();
	}
	dprintf(D_FULLDEBUG, "In SortedErasureCoding 4\n");//##
	policyAd.InsertAttr("CachedCandidates", cached_candidates);
	policyAd.InsertAttr("DataNumber", data_number_constraint);
	policyAd.InsertAttr("ParityNumber", parity_number_constraint);
	dprintf(D_FULLDEBUG, "In SortedErasureCoding 5\n");//##
	return policyAd;
}

compat_classad::ClassAd CacheflowManagerServer::RandomErasureCoding(double max_failure_rate, long long int time_to_fail_minutes, long long int cache_size, std::string location_constraint, std::string location_blockout, int data_number_constraint, int parity_number_constraint, std::string flexibility_constraint) {
	compat_classad::ClassAd policyAd;

	dprintf(D_FULLDEBUG, "In RandomErasureCoding 1\n");//##
	std::string redundancy_method = "ErasureCoding";
	std::string redundancy_selection = "Random";
	std::string redundancy_flexibility = flexibility_constraint;
	policyAd.InsertAttr("RedundancyMethod", redundancy_method);
	policyAd.InsertAttr("RedundancySelection", redundancy_selection);
	policyAd.InsertAttr("RedundancyFlexibility", redundancy_flexibility);

	std::vector<std::string> cached_final_list;
	// Step 1: get all existing CacheDs that store replicas.
	std::vector<std::string> v;
	if (location_constraint.empty())
	{
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, location_constraint is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomErasureCoding, location_constraint is an empty string");
		return policyAd;
	}
	if (location_constraint.find(",") == std::string::npos) {
		v.push_back(location_constraint);
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, only have one location_constraint %s\n", location_constraint.c_str());//##
	} else {
		boost::split(v, location_constraint, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, have multiple location_constraint %s\n", location_constraint.c_str());//##
	}

	// Step 2: calculate accumulated failure rate of restricted locations."found" keeps a record of real existing replicas which are a subset of vector v.
	dprintf(D_FULLDEBUG, "In RandomErasureCoding, m_cached_info_map.size() = %d, m_cached_info_list.size() = %d\n", m_cached_info_map.size(), m_cached_info_list.size());//##
	std::list<CMCachedInfo>::iterator it;
	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
	}
	int found = 0;
	for(int i = 0; i < v.size(); ++i) {
		if(m_cached_info_map.find(v[i]) == m_cached_info_map.end()) {
			// TODO: should delete the cache on this CacheD
			dprintf(D_FULLDEBUG, "In RandomErasureCoding, StorageOptimizer decided not to include this CacheD, so forget about this CacheD\n");
			continue;
		}
		found++;
		it = m_cached_info_map[v[i]];
		CMCachedInfo self_info = *it;
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, cached_name = %s, failure_rate = %f, total_disk_space = %lld\n", self_info.cached_name.c_str(), self_info.failure_rate, self_info.total_disk_space);//##
		m_cached_info_list.splice(m_cached_info_list.begin(), m_cached_info_list, it);
		cached_final_list.push_back(v[i]);
	}

	// Step 3: check corner cases and calculate how many replicas we still need - "left_number".
	int left_number = -1;
	if(data_number_constraint == -1 && parity_number_constraint != -1) {
		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomErasureCoding, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint != -1 && parity_number_constraint == -1) {
		dprintf(D_FULLDEBUG, "Not valid pair of data and parity\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomErasureCoding, data_number_constraint and parity_number_constraint are not a valid pair");
		return policyAd;
	} else if(data_number_constraint == -1 && parity_number_constraint == -1) {
		dprintf(D_FULLDEBUG, "we do not support dynamic erasure coding\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomErasureCoding, we do not support dynamic erasure coding");
		return policyAd;
	} else {
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, data_number_constraint = %d, parity_number_constraint = %d\n", data_number_constraint, parity_number_constraint);
		left_number = data_number_constraint + parity_number_constraint - found;
	}

	// Step 4: check left_number.
	if(left_number < 0) {
		// the number of existing replicas has been larger than the number of required number of replicas.
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomErasureCoding, there exists more than reuqired number of replicas");
		return policyAd;
	} else if(left_number == 0) {
		// the number of existing replicas exactly matches the the number of required replicas.
		policyAd.InsertAttr("CachedCandidates", location_constraint);
		policyAd.InsertAttr("DataNumber", data_number_constraint);
		policyAd.InsertAttr("ParityNumber", parity_number_constraint);
		policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		return policyAd;
	} else {
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, left_number = %d\n", left_number);
	}

	// Step 5: calculate blockout vector in LocationBlockout
	std::vector<std::string> b;
	if (location_blockout.empty())
	{
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, location_blockout is an empty string\n");
		policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
		policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomErasureCoding, location_blockout is an empty string");
		return policyAd;
	}
	if (location_blockout.find(",") == std::string::npos) {
		b.push_back(location_blockout);
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, only have one location_blockout %s\n", location_blockout.c_str());//##
	} else {
		boost::split(b, location_blockout, boost::is_any_of(", "));
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, have multiple location_blockout %s\n", location_blockout.c_str());//##
	}

	dprintf(D_FULLDEBUG, "In RandomErasureCoding 2\n");//##
	for(std::list<CMCachedInfo>::iterator tmp = m_cached_info_list.begin(); tmp != m_cached_info_list.end(); ++tmp) {
		dprintf(D_FULLDEBUG, "In RandomErasureCoding 2, tmp->cached_name = %s, tmp->failure_rate = %f, tmp->total_disk_space = %lld\n", tmp->cached_name.c_str(), tmp->failure_rate, tmp->total_disk_space);//##
	}

	// Step 6: collect all valid pilots
	it = m_cached_info_list.begin();
	int valid_count = 0;
	std::vector<CMCachedInfo> valid_vector;
	for(advance(it, found); it != m_cached_info_list.end(); ++it) {
		CMCachedInfo cached_info = *it;
		if(cached_info.total_disk_space < cache_size) continue;
		if(find(b.begin(), b.end(), cached_info.cached_name) != b.end()) continue;
		valid_vector.push_back(cached_info);
	}

	// Step 7:
	//	a. left_number is larger than available pilots, wrong
	//	b. left_number is equal to avaiable pilots, select all of the pilots
	//	c. left_number is less than available pilots, random select pilot until reach the required number of pilots
	if(left_number != INT_MAX) {
		if(left_number > valid_vector.size()) {
			dprintf(D_FULLDEBUG, "In RandomErasureCoding, not enough valid pilot for this cache\n");
			policyAd.InsertAttr(ATTR_ERROR_CODE, 1);
			policyAd.InsertAttr(ATTR_ERROR_STRING, "In RandomErasureCoding, not enough valid pilot for this cache\n");
			return policyAd;
		} else if(left_number == valid_vector.size()) {
			for(int i = 0; i < valid_vector.size(); ++i) {
				cached_final_list.push_back(valid_vector[i].cached_name);
			}
			policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		} else {
			int count = 0;
			while(count < left_number) {
				int n = valid_vector.size() - count;
				int idx = rand()%n;
				cached_final_list.push_back(valid_vector[idx].cached_name);
				valid_vector.erase(valid_vector.begin()+idx);
				count++;
			}
			policyAd.InsertAttr("NegotiateStatus", "SUCCEEDED");
		}
	} else {
		// TODO: implement this logic
		dprintf(D_FULLDEBUG, "In RandomErasureCoding, data_number_constraint and parity_number_constraint must be defined\n");
	}
	
	// Step 7: prepare return policy ad.
	dprintf(D_FULLDEBUG, "In RandomErasureCoding 3\n");//##
	std::string cached_candidates;
	for(int i = 0; i < cached_final_list.size(); ++i) {
		cached_candidates += cached_final_list[i];
		cached_candidates += ",";
	}
	if(!cached_candidates.empty() && cached_candidates.back() == ',') {
		cached_candidates.pop_back();
	}
	dprintf(D_FULLDEBUG, "In RandomErasureCoding 4\n");//##
	policyAd.InsertAttr("CachedCandidates", cached_candidates);
	policyAd.InsertAttr("DataNumber", data_number_constraint);
	policyAd.InsertAttr("ParityNumber", parity_number_constraint);
	dprintf(D_FULLDEBUG, "In RandomErasureCoding 5\n");//##
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
		m_cached_info_map[cached_vec[i].cached_name] = prev(m_cached_info_list.end());
	}
	dprintf(D_FULLDEBUG, "In CacheflowManagerServer::GetCachedInfo 11\n");//##

	rsock->close();
	delete rsock;
	return 0;
}

