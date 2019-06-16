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
#include <fstream>
#include <sstream>
#include <chrono>

namespace fs = ::boost::filesystem;

#define SCHEMA_VERSION 1

#define PROACTIVE_TIME 25

const int CachedServer::m_schema_version(SCHEMA_VERSION);
const char *CachedServer::m_header_key("CACHE_ID");

static int dummy_reaper(Service *, int pid, int) {
	dprintf(D_ALWAYS,"dummy-reaper: pid %d exited; ignored\n",pid);
	return TRUE;
}

static void recursive_copy_directory(const boost::filesystem::path &src, const boost::filesystem::path &dst) {
	if (boost::filesystem::exists(dst)){
		dprintf(D_FULLDEBUG, "In recursive_copy_directory, dst exists\n");
		return;
	}
	if (boost::filesystem::is_directory(src)) {
		boost::filesystem::create_directories(dst);
		for (boost::filesystem::directory_entry& item : boost::filesystem::directory_iterator(src)) {
			recursive_copy_directory(item.path(), dst/item.path().filename());
		}
	} else if (boost::filesystem::is_regular_file(src)) {
		boost::filesystem::copy(src, dst);
	} else {
		dprintf(D_FULLDEBUG, "In recursive_copy_directory, dst is not a directory or file\n");
	}
	dprintf(D_FULLDEBUG, "In recursive_copy_directory 4\n");
	return;
}

static unsigned long long int upload_count = 0;
static std::chrono::time_point<std::chrono::system_clock> upload_start;
static std::chrono::time_point<std::chrono::system_clock> upload_end;
static std::chrono::duration<double> upload_duration(0);
static unsigned long long int total_download_count = 0;
static std::chrono::duration<double> total_download_duration(0);
static unsigned long long int write_download_count = 0;
static std::chrono::time_point<std::chrono::system_clock> write_download_start;
static std::chrono::time_point<std::chrono::system_clock> write_download_end;
static std::chrono::duration<double> write_download_duration(0);
static unsigned long long int recovery_download_count = 0;
static std::chrono::time_point<std::chrono::system_clock> recovery_download_start;
static std::chrono::time_point<std::chrono::system_clock> recovery_download_end;
static std::chrono::duration<double> recovery_download_duration(0);
static std::chrono::time_point<std::chrono::system_clock> ec_start;
static std::chrono::time_point<std::chrono::system_clock> ec_end;
static std::chrono::duration<double> ec_duration(0);

CachedServer::CachedServer():
	m_registered_handlers(false)
{
	m_boot_time = time(NULL);
	if ( !m_registered_handlers )
	{
		m_registered_handlers = true;

		int rc = -1;
		// Register the commands
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
			CACHED_GET_MOST_RELIABLE_CACHED,
			"CACHED_GET_MOST_RELIABLE_CACHED",
			(CommandHandlercpp)&CachedServer::GetMostReliableCacheD,
			"CachedServer::GetMostReliableCacheD",
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
			CACHED_CLEAN_REDUNDANCY_SOURCE,
			"CACHED_CLEAN_REDUNDANCY_SOURCE",
			(CommandHandlercpp)&CachedServer::ReceiveCleanRedundancySource,
			"CachedServer::ReceiveCleanRedundancySource",
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
			CACHED_PROBE_CACHED_CLIENT,
			"CACHED_PROBE_CACHED_CLIENT",
			(CommandHandlercpp)&CachedServer::ProbeCachedClient,
			"CachedServer::ProbeCachedClient",
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

		rc = daemonCore->Register_Command(
			CACHED_REQUEST_RECOVERY,
			"CACHED_REQUEST_RECOVERY",
			(CommandHandlercpp)&CachedServer::ReceiveRequestRecovery,
			"CachedServer::ReceiveRequestRecovery",
			this,
			WRITE,
			D_COMMAND,
			true );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_UPDATE_RECOVERY,
			"CACHED_UPDATE_RECOVERY",
			(CommandHandlercpp)&CachedServer::ReceiveUpdateRecovery,
			"CachedServer::ReceiveUpdateRecovery",
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

	// open file to record negotiate information
	negotiate_fs.open("/home/centos/negotiate.txt", std::fstream::out | std::fstream::app);
	negotiate_fs << "start recording" << std::endl;
	// open file to record redundancy total count over time
	redundancy_count_fs.open("/home/centos/redundancy_count.txt", std::fstream::out | std::fstream::app);
	redundancy_count_fs << "start recording" << std::endl;
	// open file to record network count, traffic and time
	network_perf_fs.open("/home/centos/network_perf.txt", std::fstream::out | std::fstream::app);
	network_perf_fs << "start recording" << std::endl;
	// open file to record recovery information 
	recovery_fs.open("/home/centos/recovery.txt", std::fstream::out | std::fstream::app);
	recovery_fs << "start recording" << std::endl;
	// open file to record redundancy map
	redundancy_map_fs.open("/home/centos/redundancy_map.txt", std::fstream::out | std::fstream::app);
	redundancy_map_fs << "start recording" << std::endl;
	// open file to record network transfer
	network_transfer_fs.open("/home/centos/network_transfer.txt", std::fstream::out | std::fstream::app);
	network_transfer_fs << "start recording" << std::endl;

	m_check_redundancy_cached_timer = daemonCore->Register_Timer(120,
		(TimerHandlercpp)&CachedServer::CheckRedundancyCacheds,
		"CachedServer::CheckRedundancyCacheds",
		(Service*)this );
	CheckRedundancyCacheds();
}

/**
 * Generate the daemon's classad, with all the information
 */

compat_classad::ClassAd CachedServer::GenerateClassAd() {

	// Update the available caches on this server
	compat_classad::ClassAd published_classad;

	daemonCore->publish(&published_classad);

	published_classad.InsertAttr("CachedServer", true);

	// Advertise the available disk space
	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	if(caching_dir[caching_dir.length()-1] != '/') {
		caching_dir += "/";
	}
	caching_dir += m_daemonName;
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

	dprintf(D_FULLDEBUG, "About to send update to collectors...\n");
	int rc = daemonCore->sendUpdates(UPDATE_AD_GENERIC, &published_classad);
	if (rc == 0) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to send commands to collectors, rc = %i\n", rc);
	} else {
		dprintf(D_FULLDEBUG, "Sent updates to %i collectors\n", rc);
	}

	// Reset the timer
	daemonCore->Reset_Timer(m_advertise_cache_daemon_timer, 600);
}

CachedServer::~CachedServer()
{
	time_t now = time(NULL);
	network_transfer_fs << now << ", " << "destruct cached, " << m_daemonName.c_str() << std::endl;
	negotiate_fs.close();
	redundancy_count_fs.close();
	network_perf_fs.close();
	recovery_fs.close();
	redundancy_map_fs.close();
	network_transfer_fs.close();
	// open file to record redundancy total count over time
	std::fstream cache_set_fs;
	cache_set_fs.open("/home/centos/cache_set.txt", std::fstream::out | std::fstream::app);
	cache_set_fs << "start printing" << std::endl;
	cache_set_fs << "initialized_set:" << std::endl;
	for(std::set<std::string>::iterator it = initialized_set.begin(); it != initialized_set.end(); ++it) {
		cache_set_fs << *it;
		if(it != prev(initialized_set.end())) {
			cache_set_fs << ",";
		}
	}
	cache_set_fs << std::endl;
	cache_set_fs << "finished_set:" << std::endl;
	for(std::set<std::string>::iterator it = finished_set.begin(); it != finished_set.end(); ++it) {
		cache_set_fs << *it;
		if(it != prev(finished_set.end())) {
			cache_set_fs << ",";
		}
	}
	cache_set_fs << std::endl;
	cache_set_fs << "died_set:" << std::endl;
	for(std::set<std::string>::iterator it = died_set.begin(); it != died_set.end(); ++it) {
		cache_set_fs << *it;
		if(it != prev(died_set.end())) {
			cache_set_fs << ",";
		}
	}
	cache_set_fs << std::endl;
	cache_set_fs << std::endl;
	cache_set_fs << "existed_set:" << std::endl;
	for(std::set<std::string>::iterator it = existed_set.begin(); it != existed_set.end(); ++it) {
		cache_set_fs << *it;
		if(it != prev(existed_set.end())) {
			cache_set_fs << ",";
		}
	}
	cache_set_fs << std::endl;
	cache_set_fs.close();
}


	void
CachedServer::InitAndReconfig()
{
	std::string caching_dir;
	param(caching_dir, "CACHING_DIR");
	if(caching_dir[caching_dir.length()-1] != '/') {
		caching_dir += "/";
	}
	caching_dir += m_daemonName;
	dprintf(D_FULLDEBUG, "In InitAndReconfig, create daemon cached directory: %s\n", caching_dir.c_str());
	if ( !mkdir_and_parents_if_needed(caching_dir.c_str(), S_IRWXU, PRIV_CONDOR) ) {
		dprintf( D_FULLDEBUG, "In InitAndReconfig, couldn't create caching dir %s\n", caching_dir.c_str());
		return;
	} else {
		dprintf(D_FULLDEBUG, "In InitAndReconfig, creating caching directory %s\n", caching_dir.c_str());
	}

	std::string db_name;
	param(db_name, "CACHED_DATABASE");
	dprintf(D_FULLDEBUG, "In InitAndReconfig, db_name = %s\n", db_name.c_str());
	std::vector<std::string> path_vec;
	boost::split(path_vec, db_name, boost::is_any_of("/"));
	if(path_vec.size() == 1) {
		m_db_fname = m_daemonName + "/" + path_vec[0];
	} else {
		m_db_fname = "";
		// the last item should be "cached.db"
		for(int i = 0; i < path_vec.size()-1; ++i) {
			m_db_fname += path_vec[i] + "/";
		}
		m_db_fname += m_daemonName;
		m_db_fname += "/";
		m_db_fname += path_vec.back();
	}
	boost::filesystem::path db_path{m_db_fname};
	if(boost::filesystem::exists(db_path)) {
		dprintf(D_FULLDEBUG, "In InitAndReconfig, m_db_fname = %s exists\n", m_db_fname.c_str());
	} else {
		boost::filesystem::ofstream(m_db_fname);
	}
	dprintf(D_FULLDEBUG, "In InitAndReconfig, m_db_fname = %s\n", m_db_fname.c_str());
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
	std::string cache_query = "IsRedundancyManager";
	cache_query += " == false";
	std::list<compat_classad::ClassAd> caches = QueryCacheLog(cache_query);
	dprintf(D_FULLDEBUG, "In CachedServer::InitializeDB2(), caches.size = %d\n", caches.size());
	
	for (std::list<compat_classad::ClassAd>::iterator it = caches.begin(); it != caches.end(); it++) {

		std::string cache_name;
		std::string cache_id_str;
		it->EvalString(ATTR_CACHE_NAME, NULL, cache_name);
		it->EvalString(ATTR_CACHE_ID, NULL, cache_id_str);
		std::string dirname = cache_name + "+" + cache_id_str;
		CondorError err;
//		DoRemoveCacheDir(dirname.c_str(), err);
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

int CachedServer::GetMostReliableCacheD(int /*cmd*/, Stream *sock)
{
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for ListCacheDirs.\n");
		return 1;
	}
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "Client did not include CondorVersion in CachedServer::GetMostReliableCacheD request\n");
		return 1;
	}
	int time_to_failure_minutes = -1;
	if (!request_ad.EvaluateAttrInt("TimeToFailureMinutes", time_to_failure_minutes))
	{
		dprintf(D_FULLDEBUG, "CachedServer::GetMostReliableCacheD, Cannot find time to failure minutes");
		return 1;
	}
	long long int cache_size = -1;
	if (!request_ad.EvaluateAttrInt("CacheSize", cache_size))
	{
		dprintf(D_FULLDEBUG, "CachedServer::GetMostReliableCacheD, Cannot find cache size");
		return 1;
	}
	
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

	dprintf(D_FULLDEBUG, "CachedServer::GetMostReliableCacheD, before STORAGE_OPTIMIZER_GET_CACHED_INFO command\n");//##
	ReliSock *rsock = (ReliSock *)so_daemon.startCommand(
					STORAGE_OPTIMIZER_GET_CACHED_INFO, Stream::reli_sock, 20 );
	dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 1\n");//##
	if (!rsock)
	{
		dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD, cannot establish valid rsock\n");
		return 1;
	}

	dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 2\n");//##
	compat_classad::ClassAd jobAd;
	jobAd.InsertAttr("CacheSize", cache_size);
	jobAd.InsertAttr("TimeToFailureMinutes", time_to_failure_minutes);
	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In GetMostReliableCacheD, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, jobAd) || !rsock->end_of_message())
	{
		rsock->close();
		delete rsock;
		dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD, failed to send request to storage optimizer\n");
		return 1;
	}

	compat_classad::ClassAd ad;
	dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 3\n");//##
	
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		rsock->close();
		delete rsock;
		dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD, failed to receive response to storage optimizer\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 4\n");//##

	// store failure rates and total disk space information from Storage Optimizer
	std::vector<std::pair<std::string, double>> failure_vec;

	// failureRates should be a string coming from Storage Optimizer with format: cached-1@condor1=0.1,cached-2@condor2=0.2,...
	std::string failureRates;
	if (!ad.EvaluateAttrString("FailureRates", failureRates))
	{
		rsock->close();
		delete rsock;
		dprintf(D_FULLDEBUG, "In GetCachedInfo, failed to receive response to storage optimizer\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 5, FailureRates=%s\n", failureRates.c_str());//##
	std::vector<std::string> failure_rates;
	if (failureRates.empty())
	{
		rsock->close();
		delete rsock;
		dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD, failed to receive response to storage optimizer\n");
		return 1;
	}
	boost::split(failure_rates, failureRates, boost::is_any_of(", "));
	std::vector<std::string> pair;
	for(int i = 0; i < failure_rates.size(); ++i) {
		boost::split(pair, failure_rates[i], boost::is_any_of("="));
		failure_vec.push_back(std::make_pair(pair[0], boost::lexical_cast<double>(pair[1])));
		pair.clear();
	}
	dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 6, failure_rates.size() = %d\n", failure_rates.size());//##
	for(int i = 0; i < failure_vec.size(); ++i) {
		dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 7, %s = %f\n", failure_vec[i].first.c_str(), failure_vec[i].second);//##
	}
	auto comp = [&](std::pair<std::string, double> cached1, std::pair<std::string, double> cached2) { return cached1.second < cached2.second; };
	std::sort(failure_vec.begin(), failure_vec.end(), comp);
	for(int i = 0; i < failure_vec.size(); ++i) {
		dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 8, %s = %f\n", failure_vec[i].first.c_str(), failure_vec[i].second);//##
	}

	if(failure_vec.empty()) {
		rsock->close();
		delete rsock;
		dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD, failed to receive response to storage optimizer\n");
		return 1;
	}

	// We do not want to return this CacheD server itself because it is running as schedd and we assume this server is not in the cluster.
	std::string cached_server = "NONE";
	double failure_rate = 1.0;
	for(int i = 0; i < failure_vec.size(); ++i) {
		if(failure_vec[i].first == m_daemonName) continue;
		cached_server = failure_vec[i].first;
		failure_rate = failure_vec[i].second;
		break;
	}

	compat_classad::ClassAd return_ad;
	return_ad.InsertAttr("CachedServerName", cached_server);
	return_ad.InsertAttr("FailureRate", failure_rate);
	if (!putClassAd(sock, return_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD, failed to send back reponse\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In CachedServer::GetMostReliableCacheD 9\n");

	rsock->close();
	delete rsock;
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
	std::string cache_id_str;
	std::string cache_machine;
	time_t cached_boot_time;
	if(!advertisement_ad.EvalString(ATTR_CACHE_NAME, NULL, cache_name)) {
		dprintf(D_ALWAYS, "RecieveRedundancyAdvertisement: Failed to read request, no %s in advertisement\n", ATTR_CACHE_NAME);
		return 1;
	}
	if(!advertisement_ad.EvalString(ATTR_CACHE_ID, NULL, cache_id_str)) {
		dprintf(D_ALWAYS, "RecieveRedundancyAdvertisement: Failed to read request, no %s in advertisement\n", ATTR_CACHE_ID);
		return 1;
	}
	if(!advertisement_ad.EvalString("CachedServerName", NULL, cache_machine)) {
		dprintf(D_ALWAYS, "RecieveRedundancyAdvertisement: Failed to read request, no %s in advertisement\n", ATTR_MACHINE);
		return 1;
	}
	if(!advertisement_ad.EvaluateAttrInt("CachedBootTime", cached_boot_time))
	{
		dprintf(D_ALWAYS, "RecieveRedundancyAdvertisement: Failed to read request, no %s in advertisement\n", "CachedBootTime");
		return 1;
	}

	std::string cache_key = cache_name + "+" + cache_id_str;
	if(redundancy_host_map.count(cache_key) == 0) {
		redundancy_host_map[cache_key] = (counted_ptr<string_to_time>)(new string_to_time);
	}

	counted_ptr<string_to_time> host_map;
	host_map = redundancy_host_map[cache_key];

	// TODO: how to get time in unix epoch?
	time_t now = time(NULL);
	(*host_map)[cache_machine] = now;

	// update cached boot time map
	cached_boot_time_map[cache_machine] = cached_boot_time;

	compat_classad::ClassAd return_ad;
	return_ad.InsertAttr("RedundancyAcknowledgement", "SUCCESS");
	if (!putClassAd(sock, return_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
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
	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In DoProcessDataTask, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		rsock->close();
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	dprintf(D_FULLDEBUG, "In DoProcessDataTask 3\n");//##
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		rsock->close();
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
		rsock->close();
		delete rsock;
		return 1;
	}
	if (!response_ad.EvaluateAttrInt("StorageCost", storage_cost))
	{
		dprintf(D_FULLDEBUG, "In DoProcessDataTask response_ad did not include storage_cost\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	if (!response_ad.EvaluateAttrInt("NetworkCost", network_cost))
	{
		dprintf(D_FULLDEBUG, "In DoProcessDataTask, response_ad did not include network_cost\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	if (!response_ad.EvaluateAttrString("DirectoryPath", directory_path))
	{
		dprintf(D_FULLDEBUG, "In DoProcessDataPath response_ad did not include directory_path\n");
		rsock->close();
		delete rsock;
		return 1;
	}

	dprintf(D_FULLDEBUG, "In DoProcessDataTask 4\n");//##
	rsock->close();
	delete rsock;
	return 0;
}

int CachedServer::ReceiveProbeCachedServer(int /* cmd */, Stream* sock) {
	// Get the URL from the incoming classad

	dprintf(D_FULLDEBUG, "entering CachedServer::ReceiveProbeCachedServer\n");//##
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
	if (time_to_failure_minutes <= 0)
	{
		dprintf(D_FULLDEBUG, "In ReceiveProbeCachedServer, time_to_failure_minutes should not less than 0\n");
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
		return 1;
	}

	return 0;
}

int CachedServer::ProbeCachedClient(int /* cmd */, Stream* sock) {

	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ProbeCachedClient, failed to get request_ad\n");
		return 1;
	}
	std::string cached_server;
	if (!request_ad.EvaluateAttrString("CachedServerName", cached_server))
	{
		dprintf(D_FULLDEBUG, "In ProbeCachedClient, request_ad does not include cached_server\n");
		return 1;
	}
	// Do not allow cached probe itself. we should change cacheflow_manager and do not allow caller of cacheflow_manager becomes a cache candidates
	if(cached_server == m_daemonName) {
		dprintf(D_FULLDEBUG, "In ProbeCachedClient, probe itself, return 1\n");
		return 1;
	}

	compat_classad::ClassAd response_ad;
	int rc = ProbeCachedServer(cached_server, request_ad, response_ad);

	if(rc) {
		std::string version = CondorVersion();
		response_ad.InsertAttr("CondorVersion", version);
		response_ad.InsertAttr(ATTR_ERROR_CODE, 1);
		response_ad.InsertAttr(ATTR_ERROR_STRING, "FAILED");
	}

	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In ProbeCachedClient, failed to send response_ad\n");
		return 1;
	}

	return rc;
}


int CachedServer::ProbeCachedServer(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad) {

	// Do not allow cached probe itself. we should change cacheflow_manager and do not allow caller of cacheflow_manager becomes a cache candidates
	if(cached_server == m_daemonName) {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, probe itself, return 1\n");
		return 1;
	}

	DaemonAllowLocateFull remote_cached(DT_CACHED, cached_server.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
			CACHED_PROBE_CACHED_SERVER, Stream::reli_sock, 20 );

	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, failed to receive request_ad\n");
		rsock->close();
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, failed to send response_ad\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	int rc;
	if (!response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, response_ad does not include ATTR_ERROR_CODE\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	if (rc) {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, response_ad return ATTR_ERROR_CODE is 1\n");
		rsock->close();
		delete rsock;
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ProbeCachedServer, response_ad return ATTR_ERROR_CODE is 0\n");
	}
	rsock->close();
	delete rsock;
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
	// TimeToFailureMinutes should be larger than 0
	require_ad.InsertAttr("TimeToFailureMinutes", 100);
	require_ad.InsertAttr("CacheSize", 1048576);

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
	std::string location_constraint;
	std::string id_constraint;
	int data_number_constraint;
	int parity_number_constraint;
	std::string cache_name;
	// now the location_constraint is one cached - redundancy_source
	if (!require_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include cache_name\n");
	}
	if (!require_ad.EvaluateAttrString("LocationConstraint", location_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include location_constraint\n");
	}
	if (!require_ad.EvaluateAttrString("IDConstraint", id_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include id_constraint\n");
	}
	if (!require_ad.EvaluateAttrInt("DataNumberConstraint", data_number_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include data_number_constraint\n");
	}
	if (!require_ad.EvaluateAttrInt("ParityNumberConstraint", parity_number_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include parity_number_constraint\n");
	}
	std::vector<std::string> location_vec;
	std::vector<std::string> id_vec;
	std::unordered_map<std::string, std::string> location_id_map;
	std::unordered_map<std::string, std::string> id_location_map;
	if (location_constraint.find(",") == std::string::npos && id_constraint.find(",") == std::string::npos) {
		location_vec.push_back(location_constraint);
		id_vec.push_back(id_constraint);
		location_id_map[location_constraint] = id_constraint;
		id_location_map[id_constraint] = location_constraint;
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, only have one location constraint %s with id = %s\n", location_constraint.c_str(), id_constraint.c_str());//##
	} else if (location_constraint.find(",") != std::string::npos && id_constraint.find(",") != std::string::npos) {
		boost::split(location_vec, location_constraint, boost::is_any_of(","));
		boost::split(id_vec, id_constraint, boost::is_any_of(","));
		if(location_vec.size() != id_vec.size()) {
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, redundancy_candidates and redundancy_ids are with different size\n");
			return 1;
		} else {
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, redundancy_candidates and redundancy_ids are good\n");
			for(int i = 0; i < location_vec.size(); ++i) {
				location_id_map[location_vec[i]] = id_vec[i];
				id_location_map[id_vec[i]] = location_vec[i];
			}
		}
	} else {
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, location_constraint have different size wrong wrong wrong\n");
	}

	std::vector<std::string> cached_final_list = location_vec;

	// assign redundancy_source as location_constraint, and we need to make sure final cached candidates have redundancy_source
	// being assigned with id 1; for recovery case, if redundancy_source failed, we choose the first survivor as redundancy_source but
	// this becomes ignorant in this case
	std::string redundancy_source;
	if(id_location_map.find(std::to_string(1)) != id_location_map.end()) {
		redundancy_source = id_location_map[std::to_string(1)];
	} else if(!id_location_map.empty()) {
		std::unordered_map<std::string, std::string>::iterator it = id_location_map.begin();
		redundancy_source = it->second;
	}
			
	// now the location_blockout is one cached - redundancy_manager
	std::string location_blockout;
	if (!require_ad.EvaluateAttrString("LocationBlockout", location_blockout))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include location_blockout\n");
	}
	std::vector<std::string> blockout_vec;
	if (location_blockout.find(",") == std::string::npos) {
		blockout_vec.push_back(location_blockout);
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, only have one location blockout %s\n", location_blockout.c_str());//##
	} else {
		boost::split(blockout_vec, location_blockout, boost::is_any_of(","));
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, location_blockout different size wrong wrong wrong\n");
	}

	std::string method_constraint;
	if (!require_ad.EvaluateAttrString("MethodConstraint", method_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include method_constraint\n");
	}
	std::string selection_constraint;
	if (!require_ad.EvaluateAttrString("SelectionConstraint", selection_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include selection_constraint\n");
	}
	std::string flexibility_constraint;
	if (!require_ad.EvaluateAttrString("FlexibilityConstraint", flexibility_constraint))
	{
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, require_ad does not include flexibility_constraint\n");
	}

	compat_classad::ClassAd ad;
	std::string negotiate_status;
	time_t now = time(NULL);
	negotiate_fs << now << ", " << "CACHE: " << cache_name << std::endl;
	while(!probe_all_done) {

		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, sending CACHEFLOW_MANAGER_GET_STORAGE_POLICY to cacheflowmanager\n");//##
		ReliSock *rsock = (ReliSock *)cm_daemon.startCommand(
				CACHEFLOW_MANAGER_GET_STORAGE_POLICY, Stream::reli_sock, 20 );

		if (!putClassAd(rsock, require_ad) || !rsock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, failed to send classad to cacheflowmanager\n");//##
			rsock->close();
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
			rsock->close();
			delete rsock;
			return 1;
		}
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, got classad from cacheflowmanager\n");//##
		std::string cached_string;
		if (!ad.EvaluateAttrString("CachedCandidates", cached_string))
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, cacheflowmanager does not include CachedCandiates\n");//##
			rsock->close();
			delete rsock;
			return 1;
		}
		if (!ad.EvaluateAttrString("NegotiateStatus", negotiate_status))
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, cacheflowmanager does not include NegotiateStatus\n");//##
			rsock->close();
			delete rsock;
			return 1;
		}
		std::vector<std::string> cached_candidates;
		if (cached_string.empty())
		{
			rsock->close();
			delete rsock;
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, CachedCandidates is an empty string\n");//##
			return 1;
		}
		if (cached_string.find(",") == std::string::npos) {
			cached_candidates.push_back(cached_string);
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, only have one candidate %s\n", cached_string.c_str());//##
		} else {
			boost::split(cached_candidates, cached_string, boost::is_any_of(", "));
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, have multiple candidates %s\n", cached_string.c_str());//##
		}
		dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, CachedCandidates = %s\n", cached_string.c_str());//##
		negotiate_fs << "CANDIDATES: " << cached_string << std::endl;
		int cached_count = 0;
		for(int i = 0; i < cached_candidates.size(); ++i) {
			if(std::find(cached_final_list.begin(), cached_final_list.end(), cached_candidates[i]) != cached_final_list.end()) {
				cached_count++;
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
			} else {
				blockout_vec.push_back(cached_candidates[i]);
			}

		}
		location_constraint = "";
		for(int i = 0; i < cached_final_list.size(); ++i) {
			location_constraint += cached_final_list[i];
			location_constraint += ",";
		}
		if(!location_constraint.empty() && location_constraint.back() == ',') {
			location_constraint.pop_back();
		}
		location_blockout = "";
		for(int i = 0; i < blockout_vec.size(); ++i) {
			location_blockout += blockout_vec[i];
			location_blockout += ",";
		}
		if(!location_blockout.empty() && location_blockout.back() == ',') {
			location_blockout.pop_back();
		}
		require_ad.InsertAttr("LocationConstraint", location_constraint);
		require_ad.InsertAttr("LocationBlockout", location_blockout);

		// we need to assign method_constraint and selection_constraint when two cases happens:
		// 1. At the beginning of CachedServer::NegotiateCacheflowManager function, method_constraint and selection_constraint
		// are not defined. Thus, after the first round of CACHEFLOW_MANAGER_GET_STORAGE_POLICY function, we should get
		// RedundancyMethod and RedundancySection from the CacheflowManager. If this while loop goes to the second round 
		// of CACHEFLOW_MANAGER_GET_STORAGE_POLICY function, we need to assign RedundancyMethod and RedundancySelection as
		// constraints.
		// 2. If at the beginning of CachedServer::NegotiateCacheflowManager function, method_constraint and selection_constraint
		// are already defined, we need to consistantly put method_constraint and selection_constriant to
		// CACHEFLOW_MANAGER_GET_STORAGE_POLICY function.
		if (!ad.EvaluateAttrString("RedundancyMethod", method_constraint))
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, cacheflowmanager does not include RedundancyMethod\n");//##
			rsock->close();
			delete rsock;
			return 1;
		}
		if (!ad.EvaluateAttrString("RedundancySelection", selection_constraint))
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, cacheflowmanager does not include RedundancySelection\n");//##
			rsock->close();
			delete rsock;
			return 1;
		}
		if (!ad.EvaluateAttrString("RedundancyFlexibility", flexibility_constraint))
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, cacheflowmanager does not include RedundancyFlexibility\n");//##
			rsock->close();
			delete rsock;
			return 1;
		}
		if (!ad.EvaluateAttrInt("DataNumber", data_number_constraint))
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, cacheflowmanager does not include DataNumber\n");//##
			rsock->close();
			delete rsock;
			return 1;
		}
		if (!ad.EvaluateAttrInt("ParityNumber", parity_number_constraint))
		{
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, cacheflowmanager does not include ParityNumber\n");//##
			rsock->close();
			delete rsock;
			return 1;
		}
		require_ad.InsertAttr("MethodConstraint", method_constraint);
		require_ad.InsertAttr("SelectionConstraint", selection_constraint);
		require_ad.InsertAttr("FlexibilityConstraint", flexibility_constraint);
		require_ad.InsertAttr("DataNumberConstraint", data_number_constraint);
		require_ad.InsertAttr("ParityNumberConstraint", parity_number_constraint);

		// if COMPACTED was returned, no more cached is available; if SUCCEEDED was returned, we need to check if there was any candidate cached
		// that was failed to be probed.
		if(negotiate_status == "COMPACTED") {
			probe_all_done = true;
		} else if(negotiate_status == "SUCCEEDED" && cached_count == cached_candidates.size()) {
			// all cached_candidates that returned from CacheflowManager are available at this moment
			probe_all_done = true;
		} else {
			probe_all_done = false;
		}
		negotiate_fs << "LOCATIONS: " << location_constraint << std::endl;
		rsock->close();
		delete rsock;
	}
	negotiate_fs << "STATUS: " << negotiate_status << std::endl;
	negotiate_fs << "END CACHE" << std::endl;
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, location_constraint = %s\n", location_constraint.c_str());//##
	// process final cached candidate list, redundancy_manager needs this to assign redundancy_id to different candidates,
	// we want to assure redundancy_source is assigned with id of 1.
	std::string redundancy_candidates;
	std::string redundancy_ids;

	if(method_constraint == "ErasureCoding") {
		// calculate which ids are not assigned yet
		std::vector<std::string> vacancy_id_vec;
		for(int i = 0; i < cached_final_list.size(); ++i) {
			if(id_location_map.find(std::to_string(i+1)) == id_location_map.end()) {
				dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, does not find ID = %d\n", i+1);//##
				vacancy_id_vec.push_back(std::to_string(i+1));
			} else {
				dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, found ID = %d : %s\n", i+1, id_location_map[std::to_string(i+1)].c_str());//##
			}
		}
		// assign vacancy ids to new cached locations
		int vacancy_idx = 0;
		for(int i = 0; i < cached_final_list.size(); ++i) {
			if(location_id_map.find(cached_final_list[i]) == location_id_map.end()) {
				dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, does not find location = %d\n", cached_final_list[i].c_str());//##
				location_id_map[cached_final_list[i]] = vacancy_id_vec[vacancy_idx];
				id_location_map[vacancy_id_vec[vacancy_idx]] = cached_final_list[i];
				vacancy_idx++;
			} else {
				dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, found location = %s, ID = %s\n", cached_final_list[i].c_str(), location_id_map[cached_final_list[i]].c_str());//##
			}
		}
		if(vacancy_idx+1 != vacancy_id_vec.size()) {
			dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, vacancy idx does not match its size\n");
		}
		// create redundancy_candidates and ids from 1,2,3,... in order.
		for(int i = 0; i < cached_final_list.size(); ++i) {
			redundancy_candidates += id_location_map[std::to_string(i+1)];
			redundancy_candidates += ",";
			redundancy_ids += std::to_string(i+1);
			redundancy_ids += ",";
		}
	} else if(method_constraint == "Replication") {
		// create redundancy_candidates and ids from 1,2,3,... in order.
		for(int i = 0; i < cached_final_list.size(); ++i) {
			redundancy_candidates += cached_final_list[i];
			redundancy_candidates += ",";
			redundancy_ids += std::to_string(i+1);
			redundancy_ids += ",";
		}
	}
	if(!redundancy_candidates.empty() && redundancy_candidates.back() == ',') {
		redundancy_candidates.pop_back();
	}
	if(!redundancy_ids.empty() && redundancy_ids.back() == ',') {
		redundancy_ids.pop_back();
	}
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, redundancy_candidates = %s\n", redundancy_candidates.c_str());//##
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, redundancy_ids = %s\n", redundancy_ids.c_str());//##
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, method_constraint = %s\n", method_constraint.c_str());//##
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, selection_constraint = %s\n", selection_constraint.c_str());//##
	dprintf(D_FULLDEBUG, "In NegotiateCacheflowManager, flexibility_constraint = %s\n", flexibility_constraint.c_str());//##
	return_ad.InsertAttr("RedundancyCandidates", redundancy_candidates);
	return_ad.InsertAttr("RedundancyMap", redundancy_ids);
	return_ad.InsertAttr("RedundancyMethod", method_constraint);
	return_ad.InsertAttr("RedundancySelection", selection_constraint);
	return_ad.InsertAttr("RedundancyFlexibility", flexibility_constraint);

	if(method_constraint == "Replication") {
		int data_number = cached_final_list.size();
		int parity_number = 0;
		return_ad.InsertAttr("DataNumber", data_number);
		return_ad.InsertAttr("ParityNumber", parity_number);
	} else if(method_constraint == "ErasureCoding") {
		// TODO: we need to figure out how to assign data number and parity number given certain number of candidates
		// now we only support predefined number of data and parity.
		return_ad.InsertAttr("DataNumber", data_number_constraint);
		return_ad.InsertAttr("ParityNumber", parity_number_constraint);
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
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	int redundancy_id;
	double max_failure_rate;
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
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_ids\n");
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
	if (!request_ad.EvaluateAttrInt("RedundancyID", redundancy_id))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include redundancy_id\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, request_ad does not include max_failure_rate\n");
		return 1;
	}
	// This cached is redundancy_source, its redundancy_id should be always 1
	if (redundancy_id != 1) {
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, redundancy_id is not 1 in redundancy_source cached\n");
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

	if(redundancy_method == "Replication") {
		if(LinkRedundancyDirectory(directory_path, dirname)) {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, link cache directory failed\n");
			return 1;
		}
	} else if(redundancy_method == "ErasureCoding") {
		if(CopyRedundancyDirectory(directory_path, dirname)) {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, copy cache directory failed\n");
			return 1;
		}
	}

	int rc = -1;

	// get transferring files
	std::string transfer_files;
	std::string transfer_redundancy_files;

	// keep a record for transfer_files and CleanRedundancySource() will delete all these files
	compat_classad::ClassAd transfer_ad;
	std::string directory = GetTransferRedundancyDirectory(dirname);
	dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, directory = %s\n", directory.c_str());//##
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, directory.c_str());
	transfer_ad.InsertAttr(ATTR_JOB_IWD, directory.c_str());
	MyString err_str;
	rc = FileTransfer::ExpandInputFileList(&transfer_ad, err_str);
	if (!rc) {
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, failed to expand transfer list %s: %s\n", directory.c_str(), err_str.c_str());
		return 1;
	}
	transfer_ad.EvaluateAttrString(ATTR_TRANSFER_INPUT_FILES, transfer_files);
	dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, expanded file list: %s", transfer_files.c_str());

	// file_vector stores file names in the current cache directory
	std::vector<std::string> file_vector;
	boost::split(file_vector, transfer_files, boost::is_any_of(","));
	// transfer_vector stores file names which are not absolute pathes
	std::vector<std::string> transfer_vector;
	for(int j = 0; j < file_vector.size(); ++j) {
		boost::filesystem::path p{file_vector[j]};
		if(!boost::filesystem::is_directory(p)) {
			boost::filesystem::path f = p.filename();
			transfer_vector.push_back(f.string());
		}
	}
	// transfer_redundancy_files will be insert to response_ad at the end of this function
	for(int k = 0; k < transfer_vector.size(); ++k) {
		transfer_redundancy_files += transfer_vector[k];
		transfer_redundancy_files += ",";
	}
	if(!transfer_redundancy_files.empty() && transfer_redundancy_files.back() == ',') {
		transfer_redundancy_files.pop_back();
	}

	// encode directory if RedundancyMethod is ErasureCoding
	std::string encode_technique;
	int encode_field_size;
	int encode_packet_size;
	int encode_buffer_size;
	if(redundancy_method == "ErasureCoding") {
		std::string encode_directory = GetRedundancyDirectory(dirname);
		int encode_data_num = data_number;
		int encode_parity_num = parity_number;
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, encode_directory = %s\n", encode_directory.c_str());
		ErasureCoder *coder = new ErasureCoder();
		if (!request_ad.EvaluateAttrString("EncodeCodeTech", encode_technique)) {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, client did not include EncodeCodeTech in request\n");
			encode_technique = "reed_sol_van";
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, client has EncodeCodeTech set as %s\n", encode_technique.c_str());
		}
		if (!request_ad.EvaluateAttrInt("EncodeFieldSize", encode_field_size))
		{
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, client did not include EncodeFieldSize in request\n");
			encode_field_size = 8;
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, client has EncodeCodeTech set as %d\n", encode_field_size);
		}
		if (!request_ad.EvaluateAttrInt("EncodePacketSize", encode_packet_size)) {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, client did not include EncodePacketSize in request\n");
			encode_packet_size = 1024;
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, client has EncodePacketSize set as %d\n", encode_packet_size);
		}
		if (!request_ad.EvaluateAttrInt("EncodeBufferSize", encode_buffer_size)) {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, client did not include EncodeBufferSize in request\n");
			encode_buffer_size = 1048576;
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, client has EncodeBufferSize set as %d\n", encode_buffer_size);
		}
		rc = coder->JerasureEncodeDir (encode_directory, encode_data_num, encode_parity_num, encode_technique, encode_field_size, encode_packet_size, encode_buffer_size);
		dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, finishes encoding\n");
		delete coder;

		// if we are using erasure coding, redundancy source needs to copy all *k1* files
		// and meta files from Coding directory to its parent directory
		// find all files that matches redundancy_id and meta file
		for(int i = 0; i < file_vector.size(); ++i) {
			std::vector<std::string> path_pieces;
			boost::split(path_pieces, file_vector[i], boost::is_any_of("/"));
			// delete file named Coding in which actual encoded file are stored
			if(path_pieces.back() == "Coding") continue;
			std::vector<std::string> name_pieces;
			// /home/htcondor/local.worker1/abc.txt -> last_file_name = abc.txt and pop abc.txt out of path_pieces
			std::string last_file_name = path_pieces.back();
			path_pieces.pop_back();
			boost::split(name_pieces, last_file_name, boost::is_any_of("."));
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, file_vector[%d] = %s\n", i, file_vector[i].c_str());
			std::string from_redundancy_name;
			std::string from_meta_name;
			std::string to_redundancy_name;
			std::string to_meta_name;
			// get prefix of full file path except the last one
			std::string prefix;
			for(int p = 0; p < path_pieces.size(); ++p) {
				prefix += path_pieces[p];
				prefix += "/";
			}
			// copy from Coding subdirectory to parent cache directory
			to_redundancy_name += prefix;
			to_meta_name += prefix;
			prefix += "Coding/";
			from_redundancy_name += prefix;
			from_meta_name += prefix;
			// get erasure coded piece file name
			from_redundancy_name += name_pieces[0] + "_k1";
			from_meta_name += name_pieces[0] + "_meta";
			to_redundancy_name += name_pieces[0] + "_k1";
			to_meta_name += name_pieces[0] + "_meta";
			// get suffix of the last one
			std::string suffix;
			for(int j = 1; j < name_pieces.size(); ++j) {
				suffix += ".";
				suffix += name_pieces[j];
			}
			from_redundancy_name += suffix;
			from_meta_name += ".txt";
			to_redundancy_name += suffix;
			to_meta_name += ".txt";
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, from_redundancy_name = %s\n", from_redundancy_name.c_str());
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, from_meta_name = %s\n", from_meta_name.c_str());
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, to_redundancy_name = %s\n", to_redundancy_name.c_str());
			dprintf(D_FULLDEBUG, "In ReceiveInitializeCache, to_meta_name = %s\n", to_meta_name.c_str());
			boost::filesystem::copy_file(from_redundancy_name, to_redundancy_name, boost::filesystem::copy_option::overwrite_if_exists);
			boost::filesystem::copy_file(from_meta_name, to_meta_name, boost::filesystem::copy_option::overwrite_if_exists);
		}
	}

	compat_classad::ClassAd cache_ad;
	cache_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	cache_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	cache_ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
	cache_ad.InsertAttr("RedundancySource", redundancy_source);
	cache_ad.InsertAttr("RedundancyManager", redundancy_manager);
	cache_ad.InsertAttr("RedundancyMethod", redundancy_method);
	cache_ad.InsertAttr("RedundancySelection", redundancy_selection);
	cache_ad.InsertAttr("RedundancyFlexibility", redundancy_flexibility);
	if(redundancy_method == "ErasureCoding") {
		cache_ad.InsertAttr("EncodeCodeTech", encode_technique);
		cache_ad.InsertAttr("EncodeFieldSize", encode_field_size);
		cache_ad.InsertAttr("EncodePacketSize", encode_packet_size);
		cache_ad.InsertAttr("EncodeBufferSize", encode_buffer_size);
	}
	cache_ad.InsertAttr("RedundancyCandidates", redundancy_candidates);
	cache_ad.InsertAttr("RedundancyMap", redundancy_ids);
	cache_ad.InsertAttr("DataNumber", data_number);
	cache_ad.InsertAttr("ParityNumber", parity_number);
	std::string authenticated_user = ((Sock *)sock)->getFullyQualifiedUser();
	cache_ad.InsertAttr(ATTR_OWNER, authenticated_user);
	cache_ad.InsertAttr("RedundancyID", redundancy_id);
	cache_ad.InsertAttr("TransferRedundancyFiles", transfer_redundancy_files);
	cache_ad.InsertAttr("MaxFailureRate", max_failure_rate);

	rc = CommitCache(cache_ad);
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
	response_ad.InsertAttr("TransferRedundancyFiles", transfer_redundancy_files);
	if(redundancy_method == "ErasureCoding") {
 		response_ad.InsertAttr("EncodeCodeTech", encode_technique);
 		response_ad.InsertAttr("EncodeFieldSize", encode_field_size);
 		response_ad.InsertAttr("EncodePacketSize", encode_packet_size);
 		response_ad.InsertAttr("EncodeBufferSize", encode_buffer_size);
 	}
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

	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In InitializeCache, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In InitializeCache, failed to send request_ad\n");
		rsock->close();
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In InitializeCache, failed to receive response_ad\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	rsock->close();
	delete rsock;
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
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	int redundancy_id;
	std::string transfer_redundancy_files;
	// if the request comes from RequestRecovery (a recovery function), the is_recovery will be set to true in following statements.
	bool is_recovery = false;
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
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_ids\n");
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
	if (!request_ad.EvaluateAttrInt("RedundancyID", redundancy_id))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include redundancy_id\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include transfer_redundancy_files\n");
		return 1;
	}
	// set is_recovery = true if request contains this attribute
	if (request_ad.EvaluateAttrBool("IsRecovery", is_recovery))
	{
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does include is_recovery\n");
		is_recovery = true;
	} else {
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, request_ad does not include is_recovery\n");
		is_recovery = false;
	}
	dprintf(D_FULLDEBUG, "In DownloadRedundancy, transfer_redundancy_files = %s\n", transfer_redundancy_files.c_str());

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

	std::string transfer_files;
	compat_classad::ClassAd transfer_ad;
	std::string directory = GetTransferRedundancyDirectory(dirname);
	dprintf(D_FULLDEBUG, "In DownloadRedundancy, directory = %s\n", directory.c_str());//##
	// ATTR_TRANSFER_INPUT_FILES will be modified in erasure coding path
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, directory.c_str());
	transfer_ad.InsertAttr(ATTR_JOB_IWD, directory.c_str());
	// Set the files to transfer
	if(redundancy_method == "Replication") {
		MyString err_str;
		int rc;
		rc = FileTransfer::ExpandInputFileList(&transfer_ad, err_str);
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, printing transfer_ad\n");//##
		if (!rc) {
			dprintf(D_FULLDEBUG, "In DownloadRedundancy, failed to expand transfer list %s: %s\n", directory.c_str(), err_str.c_str());
			return 1;
		}
		transfer_ad.EvaluateAttrString(ATTR_TRANSFER_INPUT_FILES, transfer_files);
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, expanded file list: %s", transfer_files.c_str());
	} else if(redundancy_method == "ErasureCoding") {
		// transfer_final_list stores the final list of files that need to be transferred
		std::vector<std::string> transfer_final_list;
		// file_vector stores file names in the current cache directory
		std::vector<std::string> file_vector;
		// For erasure coding case, transfer_files is transfer_redundancy_files from ProcessTask()
		transfer_files = transfer_redundancy_files;
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, transfer_files = %s\n", transfer_files.c_str());
		boost::split(file_vector, transfer_files, boost::is_any_of(","));
		// find all files that matches redundancy_id and meta file
		for(int i = 0; i < file_vector.size(); ++i) {
			dprintf(D_FULLDEBUG, "In DownloadRedundancy, file_vector[%d] = %s\n", i, file_vector[i].c_str());
			std::string redundancy_name;
			std::string meta_name;
			// get prefix of full file path except the last one
			std::string prefix = directory;
			if(!is_recovery) {
				prefix += "Coding/";
			}
			// abc.txt -> ["abc", "txt"] -> abc_k1.txt
			std::vector<std::string> name_pieces;
			boost::split(name_pieces, file_vector[i], boost::is_any_of("."));
			redundancy_name += prefix;
			meta_name += prefix;
			// get erasure coded piece file name
			if(redundancy_id <= data_number) {
				redundancy_name += name_pieces[0] + "_k" + std::to_string(redundancy_id);
			} else if(redundancy_id > data_number && redundancy_id <= (data_number+parity_number)) {
				redundancy_name += name_pieces[0] + "_m" + std::to_string(redundancy_id-data_number);
			}
			meta_name += name_pieces[0] + "_meta";
			// get suffix of the last one
			std::string suffix;
			for(int j = 1; j < name_pieces.size(); ++j) {
				suffix += ".";
				suffix += name_pieces[j];
			}
			redundancy_name += suffix;
			meta_name += ".txt";
			dprintf(D_FULLDEBUG, "In DownloadRedundancy, redundancy_name = %s\n", redundancy_name.c_str());
			dprintf(D_FULLDEBUG, "In DownloadRedundancy, meta_name = %s\n", meta_name.c_str());
			transfer_final_list.push_back(redundancy_name);
			transfer_final_list.push_back(meta_name);
		}
		// create the final transfer input file list
		std::string transfer_final_string;
		for(int k = 0; k < transfer_final_list.size(); ++k) {
			transfer_final_string += transfer_final_list[k];
			transfer_final_string += ",";
		}
		if(!transfer_final_string.empty() && transfer_final_string.back() == ',') {
			transfer_final_string.pop_back();
		}
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, transfer_final_string = %s\n", transfer_final_string.c_str());
		transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, transfer_final_string);
	}
		
	// From here on out, this is the file transfer server socket.
	// TODO: Handle user permissions for the files

	// The file transfer object is deleted automatically by the upload file 
	// handler.
	FileTransfer* ft = new FileTransfer();
	ft->SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(sock));
	ft->setPeerVersion(version.c_str());
	upload_start = std::chrono::system_clock::now();
	ft->UploadFiles();
	upload_end = std::chrono::system_clock::now();
	upload_duration += (upload_end - upload_start);
	upload_count += 1;
	dprintf(D_FULLDEBUG, "In DownloadRedundancy, upload_duration = %f, upload_count = %llu\n", upload_duration.count(), upload_count);//##
	FileTransfer::FileTransferInfo fi = ft->GetInfo();
        if (fi.success) {
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, succeeded, fi.bytes = %d, fi.duration = %f\n", fi.bytes, fi.duration);
	} else {
		dprintf(D_FULLDEBUG, "In DownloadRedundancy, failed, fi.bytes = %d, fi.duration = %f\n", fi.bytes, fi.duration);
	}

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

int CachedServer::CopyRedundancyDirectory(const std::string& source, const std::string& destination) {
	dprintf(D_FULLDEBUG, "In CopyRedundancyDirectory 0, source = %s, destination = %s\n", source.c_str(), destination.c_str());
	boost::filesystem::path src{source};
	std::string caching_dir = GetRedundancyDirectory(destination);
	dprintf(D_FULLDEBUG, "In CopyRedundancyDirectory 1, source = %s, cache_dir = %s\n", source.c_str(), caching_dir.c_str());
	boost::filesystem::path dst{caching_dir};
	recursive_copy_directory(src, dst);
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
	caching_dir += m_daemonName;
	caching_dir += "/";
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
	caching_dir += m_daemonName;
	caching_dir += "/";
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

	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In RequestRedundancy, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In RequestRedundancy, failed to send send_ad to remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In RequestRedundancy, failed to receive receive_ad from remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	int rc;//##
	if (!response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		dprintf(D_FULLDEBUG, "In RequestRedundancy, response_ad does not include ATTR_ERROR_CODE\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	if(rc) {
		dprintf(D_FULLDEBUG, "In RequestRedundancy, response_ad ATTR_ERROR_CODE is not zero\n");
		rsock->close();
		delete rsock;
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In RequestRedundancy, response_ad ATTR_ERROR_CODE is zero\n");
	}
	dprintf(D_FULLDEBUG, "In RequestRedundancy, return 0 for %s\n", cached_server.c_str());
	rsock->close();
	delete rsock;
	return 0;
}

int CachedServer::ReceiveCleanRedundancySource(int /* cmd */, Stream* sock) {
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, entering ReceiveCleanRedundancySource\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, failed to read request for ReceiveCleanRedundancySource.\n");
		return 1;
	}

	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include condor version.\n");
		return 1;
	}
	if(version != CondorVersion()) {
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, condor version mismatches\n");
	}

	time_t lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string cache_owner;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	int redundancy_id;
	std::string transfer_redundancy_files;
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include cache_name\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include cache_id\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_source\n");
		return 1;
	}
	// if redundancy_source is this daemon itself, since we already InitlaizeCache thus we return SUCCEEDED
	if(redundancy_source != m_daemonName) {
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, redundancy_source is not me cannot delete redundancy source\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include lease_expiry\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include cache_name\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include cache_id_str\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include cache_owner\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_manager\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include redundancy_ids\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include data_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include parity_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, request_ad does not include transfer_redundancy_files\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, transfer_redundancy_files = %s\n", transfer_redundancy_files.c_str());

	// delete all files that exist before encoding. it should include Coding directory
	std::vector<std::string> file_vector;
	boost::split(file_vector, transfer_redundancy_files, boost::is_any_of(","));
	std::string dirname = cache_name + "+" + cache_id_str;
	std::string directory = GetTransferRedundancyDirectory(dirname);
	// delete all original files in erasure coding
	for(int i = 0; i < file_vector.size(); ++i) {
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, file_vector[%d] = %s\n", i, file_vector[i].c_str());//##
		std::string delete_file_name = directory + file_vector[i];
		if(boost::filesystem::exists(delete_file_name)) {
			boost::filesystem::remove_all(delete_file_name);
		}
	}
	// delete Coding subdirectory in erasure coding
	std::string delete_file_name = directory + "Coding";
	if(boost::filesystem::exists(delete_file_name)) {
		boost::filesystem::remove_all(delete_file_name);
	}

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	response_ad.InsertAttr(ATTR_ERROR_STRING, "SUCCEEDED");
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveCleanRedundancySource, failed to send response_ad to remote cached\n");
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
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	int redundancy_id;
	std::string transfer_redundancy_files;
	double max_failure_rate;
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include cache_name\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include cache_id\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_source\n");
		return 1;
	}
	// if redundancy_source is this daemon itself, since we already InitlaizeCache thus we return SUCCEEDED
	if(redundancy_source == m_daemonName) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, redundancy_source is daemon itself\n");

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
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_ids\n");
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
	if (!request_ad.EvaluateAttrInt("RedundancyID", redundancy_id))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include redundancy_id\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include transfer_redundancy_files\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, request_ad does not include max_failure_rate\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, TransferRedundancyFiles = %s\n", transfer_redundancy_files.c_str());//##
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
	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, send_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to send send_ad to remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}

	// Receive the response
	compat_classad::ClassAd receive_ad;
	rsock->decode();
	if (!getClassAd(rsock, receive_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to receive receive_ad from remote cached\n");
		rsock->close();
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
	compat_classad::ClassAd* transfer_ad = new compat_classad::ClassAd();
	transfer_ad->InsertAttr(ATTR_JOB_IWD, directory);
	transfer_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, directory);
	dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, directory here is %s\n", directory.c_str());

	// TODO: Enable file ownership checks
	rc = ft->SimpleInit(transfer_ad, false, true, static_cast<ReliSock*>(rsock));
	if (!rc) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed simple init\n");
		rsock->close();
		delete rsock;
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, successfully SimpleInit of filetransfer\n");
	}

	ft->setPeerVersion(version.c_str());

	write_download_start = std::chrono::system_clock::now();
	rc = ft->DownloadFiles();
	write_download_end = std::chrono::system_clock::now();
	std::chrono::duration<double> write_tmp_duration(0);
	write_tmp_duration = (write_download_end - write_download_start);
	write_download_duration += write_tmp_duration;
	write_download_count += 1;
	time_t now = time(NULL);
	network_transfer_fs << now << ", " << "write_download, " << redundancy_source.c_str() << ", " << m_daemonName.c_str() << ", " << write_tmp_duration.count() << std::endl;
	total_download_duration += write_tmp_duration;
	total_download_count += 1;
	dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, write_download_duration = %f, write_download_count = %llu\n", write_download_duration.count(), write_download_count);//##
	FileTransfer::FileTransferInfo fi = ft->GetInfo();
        if (fi.success) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, succeeded, fi.bytes = %d, fi.duration = %f\n", fi.bytes, fi.duration);
	} else {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed, fi.bytes = %d, fi.duration = %f\n", fi.bytes, fi.duration);
	}

	if (!rc) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed DownloadFiles\n");
		rsock->close();
		delete rsock;
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, successfully began downloading files\n");
	}

	rc = CommitCache(request_ad);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, CommitCache failed\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	response_ad.InsertAttr(ATTR_ERROR_STRING, "SUCCEEDED");
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRedundancy, failed to send response_ad to remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	rsock->close();
	delete rsock;
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
	std::string redundancy_ids;
	if (!request_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, class ad does not include redundancy_ids\n");
		return 1;
	}
	std::vector<std::string> v;
	std::unordered_map<std::string, int> map;
	if (redundancy_candidates.empty())
	{
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, redundancy_candidates is an empty string\n");
		return 1;
	}
	if (redundancy_ids.empty())
	{
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, redundancy_ids is an empty string\n");
		return 1;
	}
	// create map between redundancy_candidates to redundancy_ids
	if (redundancy_candidates.find(",") == std::string::npos && redundancy_ids.find(",") == std::string::npos) {
		v.push_back(redundancy_candidates);
		map[redundancy_candidates] = stoi(redundancy_ids);
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, only have one candidate %s\n", redundancy_candidates.c_str());//##
	} else if (redundancy_candidates.find(",") != std::string::npos && redundancy_ids.find(",") != std::string::npos) {
		boost::split(v, redundancy_candidates, boost::is_any_of(","));
		std::vector<std::string> ids;
		boost::split(ids, redundancy_ids, boost::is_any_of(","));
		if(v.size() != ids.size()) {
			dprintf(D_FULLDEBUG, "In DistributeRedundancy, redundancy_candidates and redundancy_ids are with different size\n");
			return 1;
		} else {
			dprintf(D_FULLDEBUG, "In DistributeRedundancy, redundancy_candidates and redundancy_ids are good\n");
			for(int i = 0; i < v.size(); ++i) {
				map[v[i]] = stoi(ids[i]);
			}
		}
	} else {
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, have different size wrong wrong wrong\n");
	}
	dprintf(D_FULLDEBUG, "In DistributeRedundancy, candidates size is %d, redundancy_candidates is %s\n", v.size(), redundancy_candidates.c_str());
	dprintf(D_FULLDEBUG, "In DistributeRedundancy, redundancy_ids is %s\n", redundancy_ids.c_str());

	int rc = 0;
	for(int i = 0; i < v.size(); ++i) {
		dprintf(D_FULLDEBUG, "In DistributeRedundancy, iteration is %d\n", i);
		const std::string cached_server = v[i];
		compat_classad::ClassAd send_ad = request_ad;
		// don't forget to assign redundancy_id to this cached
		send_ad.InsertAttr("RedundancyID", map[cached_server]);
		compat_classad::ClassAd receive_ad;
		rc = RequestRedundancy(cached_server, send_ad, receive_ad);
		if(rc) {
			dprintf(D_FULLDEBUG, "In DistributeRedundancy, RequestRedundancy failed for %s\n", cached_server.c_str());
		} else {
			dprintf(D_FULLDEBUG, "In DistributeRedundancy, RequestRedundancy succeeded for %s\n", cached_server.c_str());
		}
	}

	// TODO: find a way to create response_ad (cache_info) for CommitCache function
	response_ad = request_ad;
	dprintf(D_FULLDEBUG, "In DistributeRedundancy, return 0\n");
	return 0;
}

int CachedServer::CheckRedundancyStatus(compat_classad::ClassAd& ad) {
	int rc = CommitCache(ad);
	if(rc) {
		dprintf(D_FULLDEBUG, "CommitCache failed\n");
		return 1;
	}
	rc = CleanRedundancySource(ad);
	if(rc) {
		dprintf(D_FULLDEBUG, "CleanRedundancySource failed\n");
		return 1;
	}
	return 0;
}

int CachedServer::CleanRedundancySource(compat_classad::ClassAd& request_ad) {

	std::string redundancy_source;
	std::string redundancy_method;
	std::string redundancy_selection;
	std::string redundancy_flexibility;

	if (!request_ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, classad does not include redundancy_source\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, classad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, classad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, classad does not include redundancy_flexibility\n");
		return 1;
	}

	if(redundancy_method != "ErasureCoding") return 0;

	// only erasure coding go into here
	std::string cached_server = redundancy_source;
	DaemonAllowLocateFull remote_cached(DT_CACHED, cached_server.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
			CACHED_CLEAN_REDUNDANCY_SOURCE, Stream::reli_sock, 20 );

	std::string version = CondorVersion();
	request_ad.InsertAttr("CondorVersion", version);
	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, failed to send send_ad to remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	compat_classad::ClassAd response_ad;
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, failed to receive receive_ad from remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	int rc;
	if (!response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, response_ad does not include ATTR_ERROR_CODE\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	if(rc) {
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, response_ad ATTR_ERROR_CODE is not zero\n");
		rsock->close();
		delete rsock;
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In CleanRedundancySource, response_ad ATTR_ERROR_CODE is zero\n");
	}
	dprintf(D_FULLDEBUG, "In CleanRedundancySource, return 0 for %s\n", cached_server.c_str());
	rsock->close();
	delete rsock;
	return 0;
}


int CachedServer::CommitCache(compat_classad::ClassAd& ad) {

	long long int lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	std::string cache_owner;
	int redundancy_id;
	std::string transfer_redundancy_files;
	double max_failure_rate;
	std::string encode_technique;
 	int encode_field_size;
 	int encode_packet_size;
 	int encode_buffer_size;

	if (!ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include lease_expiry\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include cache_name\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include cache_id_str\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_source\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_manager\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_method\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_selection\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_candidates\n");
		return 1;
	}
	if (!ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include data_number\n");
		return 1;
	}
	if (!ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include parity_number\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include cache_owner\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_ids\n");
		return 1;
	}
	// redundancy_manager does not need this item
	if (redundancy_manager != m_daemonName && !ad.EvaluateAttrInt("RedundancyID", redundancy_id))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include redundancy_id\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include transfer_redundancy_files\n");
		return 1;
	}
	if (!ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include max_failure_rate\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrString("EncodeCodeTech", encode_technique)) {
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include EncodeCodeTech in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodeFieldSize", encode_field_size)) {
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include EncodeFieldSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodePacketSize", encode_packet_size)) {
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include EncodePacketSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodeBufferSize", encode_buffer_size)) {
		dprintf(D_FULLDEBUG, "In CommitCache, classad does not include EncodeBufferSize in request\n");
		return 1;
	}

	std::string dirname = cache_name + "+" + cache_id_str;
	m_log->BeginTransaction();
	SetAttributeLong(dirname, ATTR_LEASE_EXPIRATION, lease_expiry);
	SetAttributeString(dirname, ATTR_CACHE_NAME, cache_name);
	SetAttributeString(dirname, ATTR_CACHE_ID, cache_id_str);
	SetAttributeString(dirname, "RedundancySource", redundancy_source);
	SetAttributeString(dirname, "RedundancyManager", redundancy_manager);
	SetAttributeString(dirname, "RedundancyMethod", redundancy_method);
	SetAttributeString(dirname, "RedundancySelection", redundancy_selection);
	SetAttributeString(dirname, "RedundancyFlexibility", redundancy_flexibility);
	SetAttributeString(dirname, "RedundancyCandidates", redundancy_candidates);
	SetAttributeInt(dirname, "DataNumber", data_number);
	SetAttributeInt(dirname, "ParityNumber", parity_number);
	SetAttributeString(dirname, ATTR_OWNER, cache_owner);
	SetAttributeString(dirname, "RedundancyMap", redundancy_ids);
	SetAttributeString(dirname, "TransferRedundancyFiles", transfer_redundancy_files);
	SetAttributeDouble(dirname, "MaxFailureRate", max_failure_rate);
	if(redundancy_method == "ErasureCoding") {
		SetAttributeString(dirname, "EncodeCodeTech", encode_technique);
		SetAttributeInt(dirname, "EncodeFieldSize", encode_field_size);
		SetAttributeInt(dirname, "EncodePacketSize", encode_packet_size);
		SetAttributeInt(dirname, "EncodeBufferSize", encode_buffer_size);
	}
	// redundancy_manager does not need this attribute
	if(redundancy_manager != m_daemonName) {
		SetAttributeInt(dirname, "RedundancyID", redundancy_id);
	}
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
	std::string redundancy_method;
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	int data_number;
	int parity_number;;
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
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "Client did not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "Client did not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "Client did not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "Client did not include data_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "Client did not include parity_number\n");
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
	if (time_to_failure_minutes <= 0)
	{
		dprintf(D_FULLDEBUG, "require_ad should make sure time_to_failure_minutes is larger than 0\n");
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
	// keep cache name for record negotiate.txt
	require_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	// since cached_server run the job and currently has the output data, thus we want to keep data there
	require_ad.InsertAttr("LocationConstraint", cached_server);
	// we want cached_server has id as 1 when erasure coding is used
	require_ad.InsertAttr("IDConstraint", "1");
	// this CacheD as the redundancy_manager later on cannot store redundancy
	require_ad.InsertAttr("LocationBlockout", m_daemonName);
	// redundancy method should be consulted with CacheflowManager.
	// If method or selection have been defined before, we set them as constraints here.
	if(redundancy_method != "Undefined") {
		require_ad.InsertAttr("MethodConstraint", redundancy_method);
	}
	if(redundancy_selection != "Undefined") {
		require_ad.InsertAttr("SelectionConstraint", redundancy_selection);
	}
	if(redundancy_flexibility != "Undefined") {
		require_ad.InsertAttr("FlexibilityConstraint", redundancy_flexibility);
	}
	// data number and parity number constraints are designed to assure erasure coding pieces match the order of original
	// assigned order for survivors when recovery happens, if data or parity numbers are defined before, we need to check 
	// if recovering is possible or not by verifying the defined numbers match existing number parameters.
	if(data_number != -1) {
		require_ad.InsertAttr("DataNumberConstraint", data_number);
	}
	if(parity_number != -1) {
		require_ad.InsertAttr("ParityNumberConstraint", parity_number);
	}
	compat_classad::ClassAd policy_ad;
	rc = NegotiateCacheflowManager(require_ad, policy_ad);
	dprintf(D_FULLDEBUG, "In ProcessTask, printing policy_ad\n");//##
	if(rc) {
		dprintf(D_FULLDEBUG, "In ProcessTask, NegotiateCacheflowManager failed\n");
		return 1;
	}
	std::string redundancy_candidates;
	std::string redundancy_ids;
	if (!policy_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include redundancy_candidates\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include redundancy_ids\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include redundancy_method\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include redundancy_selection\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "policy_ad did not include redundancy_flexibility\n");
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
	cache_request_ad.InsertAttr("RedundancySelection", redundancy_selection);
	cache_request_ad.InsertAttr("RedundancyFlexibility", redundancy_flexibility);
	cache_request_ad.InsertAttr("RedundancyCandidates", redundancy_candidates);
	cache_request_ad.InsertAttr("RedundancyMap", redundancy_ids);
	cache_request_ad.InsertAttr("DataNumber", data_number);
	cache_request_ad.InsertAttr("ParityNumber", parity_number);
	cache_request_ad.InsertAttr("RedundancyID", 1);
	cache_request_ad.InsertAttr("MaxFailureRate", max_failure_rate);

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
	std::string transfer_redundancy_files;
	std::string encode_technique;
	int encode_field_size;
	int encode_packet_size;
	int encode_buffer_size;
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
	if (!cache_response_ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include transfer_redundancy_files\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !cache_response_ad.EvaluateAttrString("EncodeCodeTech", encode_technique)) {
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include EncodeCodeTech in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !cache_response_ad.EvaluateAttrInt("EncodeFieldSize", encode_field_size)) {
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include EncodeFieldSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !cache_response_ad.EvaluateAttrInt("EncodePacketSize", encode_packet_size)) {
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include EncodePacketSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !cache_response_ad.EvaluateAttrInt("EncodeBufferSize", encode_buffer_size)) {
		dprintf(D_FULLDEBUG, "In ProcessTask, cache_response_ad did not include EncodeBufferSize in request\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In ProcessTask, TransferRedundancyFiles = %s\n", transfer_redundancy_files.c_str());//##
	// Step 5, distribute cache to other candidates distribute_ad -> succeeded or failued
	dprintf(D_FULLDEBUG, "In ProcessTask 4\n");
	compat_classad::ClassAd distribute_ad;
	distribute_ad.InsertAttr("CondorVersion", version);
	distribute_ad.InsertAttr(ATTR_LEASE_EXPIRATION, new_lease_expiry);
	distribute_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	distribute_ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);

	// Keep a record of expiration time for every cache
	std::string dirname = cache_name + "+" + cache_id_str;
	cache_expiry_map[dirname] = new_lease_expiry;

	// Keep a record of initialized cache name
	initialized_set.insert(dirname);

	distribute_ad.InsertAttr(ATTR_OWNER, cache_owner);
	distribute_ad.InsertAttr("RedundancySource", cached_server);
	distribute_ad.InsertAttr("RedundancyManager", m_daemonName);
	distribute_ad.InsertAttr("RedundancyMethod", redundancy_method);
	distribute_ad.InsertAttr("RedundancySelection", redundancy_selection);
	dprintf(D_FULLDEBUG, "In ProcessTask, RedundancySelection = %s\n", redundancy_selection.c_str());//##
	distribute_ad.InsertAttr("RedundancyFlexibility", redundancy_flexibility);
	dprintf(D_FULLDEBUG, "In ProcessTask, RedundancyFlexibility = %s\n", redundancy_flexibility.c_str());//##
	if(redundancy_method == "ErasureCoding") {
		distribute_ad.InsertAttr("EncodeCodeTech", encode_technique);
		distribute_ad.InsertAttr("EncodeFieldSize", encode_field_size);
		distribute_ad.InsertAttr("EncodePacketSize", encode_packet_size);
		distribute_ad.InsertAttr("EncodeBufferSize", encode_buffer_size);
	}
	distribute_ad.InsertAttr("RedundancyCandidates", redundancy_candidates);
	distribute_ad.InsertAttr("RedundancyMap", redundancy_ids);
	distribute_ad.InsertAttr("DataNumber", data_number);
	distribute_ad.InsertAttr("ParityNumber", parity_number);
	distribute_ad.InsertAttr("TransferRedundancyFiles", transfer_redundancy_files);
	distribute_ad.InsertAttr("MaxFailureRate", max_failure_rate);

	compat_classad::ClassAd cache_info;
	rc = DistributeRedundancy(distribute_ad, cache_info);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ProcessTask, DistributeRedundancy failed\n");
		return 1;
	}

	// Step 6, Check distributing redundancy status
	dprintf(D_FULLDEBUG, "In ProcessTask 5\n");
	cache_info.InsertAttr("MaxFailureRate", max_failure_rate);
	cache_info.InsertAttr("TransferRedundancyFiles", transfer_redundancy_files);
	if(redundancy_method == "ErasureCoding") {
		cache_info.InsertAttr("EncodeCodeTech", encode_technique);
		cache_info.InsertAttr("EncodeFieldSize", encode_field_size);
		cache_info.InsertAttr("EncodePacketSize", encode_packet_size);
		cache_info.InsertAttr("EncodeBufferSize", encode_buffer_size);
	}
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
		dprintf(D_FULLDEBUG, "In ProcessTask, sending response failed\n");
		return 1;
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
	caching_dir += m_daemonName;
	caching_dir += "/";
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

/**
 *	Advertise the redundancy of all caches stored on this server
 *
 */
void CachedServer::AdvertiseRedundancy() {

	classad::ClassAdParser	parser;

	dprintf(D_FULLDEBUG, "In AdvertiseRedundancy 1!\n");//##
	// Create the requirements expression
	char buf[512];
	sprintf(buf, "(%s == %i) && (%s =?= false) && (%s =!= \"%s\")", ATTR_CACHE_STATE, COMMITTED, "IsRedundancyManager", "RedundancyManager", m_daemonName.c_str());
	dprintf(D_FULLDEBUG, "AdvertiseRedundancy: Cache Query = %s\n", buf);

	std::list<compat_classad::ClassAd> caches = QueryCacheLog(buf);

	dprintf(D_FULLDEBUG, "In AdvertiseRedundancy 2, caches.size() = %d!\n", caches.size());//##

	std::string redundancy_manager;
	std::list<compat_classad::ClassAd>::iterator cache_iterator = caches.begin();
	while ((cache_iterator != caches.end())) {
		compat_classad::ClassAd cache_ad = *cache_iterator;

		// evaluate expiry time and delete the cache entry if it expired. We allow 100 seconds grace period.
		time_t lease_expiry = -1;
		std::string cache_name;
		std::string cache_id_str;
		cache_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name);
		cache_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str);
		cache_ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry);
		std::string dirname = cache_name + "+" + cache_id_str;
		dprintf(D_FULLDEBUG, "In AdvertiseRedundancy, lease_expiration = %lld, dirname = %s\n", lease_expiry, dirname.c_str());
		time_t now = time(NULL);
		// keep reporting heartbeats for 3 minutes
		long long int time_to_failure_seconds = (lease_expiry + 180 - now);
		if (time_to_failure_seconds <= 0) {
			dprintf(D_FULLDEBUG, "In AdvertiseRedundancy, time_to_failure_secondss is less than 0 for %s\n", dirname.c_str());
			m_log->BeginTransaction();
			// set this cache as OBSOLETE and make sure the next AdvertiseRedundancy() won't query this cache because
			// AdvertiseRedundancy() only queries COMMITTED caches
			int state = OBSOLETE;
			SetAttributeInt(dirname, ATTR_CACHE_STATE, state);
			m_log->CommitTransaction();
			cache_iterator++;
			continue;
		}

		// now report cache redundancy to redundancy mananger
		cache_ad.EvaluateAttrString("RedundancyManager", redundancy_manager);
		DaemonAllowLocateFull manager_cached(DT_CACHED, redundancy_manager.c_str());
		if(!manager_cached.locate(Daemon::LOCATE_FULL)) {
			dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
			cache_iterator++;
			continue;
		} else {
			dprintf(D_FULLDEBUG, "Located daemon at %s\n", redundancy_manager.c_str());
		}
		dprintf(D_FULLDEBUG, "In AdvertiseRedundancy 3!\n");//##

		ReliSock *rsock = (ReliSock *)manager_cached.startCommand(
						CACHED_ADVERTISE_REDUNDANCY, Stream::reli_sock, 20 );

		dprintf(D_FULLDEBUG, "In AdvertiseRedundancy 4!\n");//##

		cache_ad.InsertAttr("CachedServerName", m_daemonName);
		cache_ad.InsertAttr("CachedBootTime", m_boot_time);
		if(!rsock || rsock->is_closed()) {
			dprintf(D_FULLDEBUG, "In AdvertiseRedundancy, rsock failed\n");
			cache_iterator++;
			continue;
		}

		if (!putClassAd(rsock, cache_ad) || !rsock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			rsock->close();
			delete rsock;
			dprintf(D_FULLDEBUG, "Failed to send cache_ad to remote redundancy manager\n");
			continue;
		}
		dprintf(D_FULLDEBUG, "In AdvertiseRedundancy 5!\n");//##

		cache_ad.Clear();
		rsock->decode();
		if (!getClassAd(rsock, cache_ad) || !rsock->end_of_message())
		{
			// TODO: we have to design recover mechanism if the redundancy manager does not response
			rsock->close();
			delete rsock;
			dprintf(D_FULLDEBUG, "Failed to get response from remote redundancy manager\n");
			continue;
		}

		dprintf(D_FULLDEBUG, "In AdvertiseRedundancy 6!\n");//##
		std::string ack;
		if (!cache_ad.EvaluateAttrString("RedundancyAcknowledgement", ack))
		{
			dprintf(D_FULLDEBUG, "Remote redundancy manager does not response an acknowledgement\n");
		}

		if(ack == "SUCCESS") {
			dprintf(D_FULLDEBUG, "Redundancy manager return SUCCESS!\n");
		} else {
			//TODO: we need to design recover mechanism here too if the redundancy manager return other messages
			rsock->close();
			delete rsock;
			dprintf(D_FULLDEBUG, "Redundancy manager does not return SUCCESS\n");
			continue;
		}
		rsock->close();
		delete rsock;
		cache_iterator++;
	}

	dprintf(D_FULLDEBUG, "In AdvertiseRedundancy 7!\n");
	daemonCore->Reset_Timer(m_advertise_redundancy_timer, 60);
}

int CachedServer::ReceiveUpdateRecovery(int /* cmd */, Stream* sock) {
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, entering ReceiveRequestRedundancy\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, failed to read request for ReceiveRequestRedundancy.\n");
		return 1;
	}

	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include condor version.\n");
		return 1;
	}
	if(version != CondorVersion()) {
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, condor version mismatches\n");
	}

	time_t lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string cache_owner;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	int redundancy_id;
	std::string transfer_redundancy_files;
	double max_failure_rate;
	std::string new_redundancy_candidates;
	std::string new_redundancy_ids;
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include cache_name\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include cache_id\n");
		return 1;
	}

	int cache_state = -1;
	if (!request_ad.EvaluateAttrInt(ATTR_CACHE_STATE, cache_state)) {
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include cache_state\n");
		return 1;
	}
	std::string dirname = cache_name + "+" + cache_id_str;
	// Proactive approach has set cache_state to OBSOLETE; otherwise cache_state is COMMITTED.
	if(cache_state == OBSOLETE) {
		m_log->BeginTransaction();
		SetAttributeInt(dirname, ATTR_CACHE_STATE, cache_state);
		m_log->CommitTransaction();

		compat_classad::ClassAd response_ad;
		response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
		response_ad.InsertAttr(ATTR_ERROR_STRING, "SUCCEEDED");
		if (!putClassAd(sock, response_ad) || !sock->end_of_message())
		{
			dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, failed to send response_ad to remote cached\n");
			return 1;
		}

		return 0;
	}

	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include redundancy_source\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include lease_expiry\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include cache_owner\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include redundancy_manager\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include redundancy_ids\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include data_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include parity_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("RedundancyID", redundancy_id))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include redundancy_id\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include transfer_redundancy_files\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", new_redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include new_redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMap", new_redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include new_redundancy_ids\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, request_ad does not include max_failure_rate\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, TransferRedundancyFiles = %s\n", transfer_redundancy_files.c_str());//##

	// update redundancy locations on this cached server
	m_log->BeginTransaction();
	// redundancy id here should be unchanged
	SetAttributeString(dirname, "RedundancyCandidates", new_redundancy_candidates);
	SetAttributeString(dirname, "RedundancyMap", new_redundancy_ids);
	m_log->CommitTransaction();

	compat_classad::ClassAd response_ad;
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	response_ad.InsertAttr(ATTR_ERROR_STRING, "SUCCEEDED");
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveUpdateRecovery, failed to send response_ad to remote cached\n");
		return 1;
	}

	return 0;
}

int CachedServer::UpdateRecovery(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad) {
	// Initiate the transfer
	DaemonAllowLocateFull remote_cached(DT_CACHED, cached_server.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In UpdateRecovery, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In UpdateRecovery, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
			CACHED_UPDATE_RECOVERY, Stream::reli_sock, 20 );

	std::string version = CondorVersion();
	request_ad.InsertAttr("CondorVersion", version);
	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In UpdateRecovery, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In UpdateRecovery, failed to send send_ad to remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In UpdateRecovery, failed to receive receive_ad from remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	int rc;
	if (!response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		dprintf(D_FULLDEBUG, "In UpdateRecovery, response_ad does not include ATTR_ERROR_CODE\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	if(rc) {
		dprintf(D_FULLDEBUG, "In UpdateRecovery, response_ad ATTR_ERROR_CODE is not zero\n");
		rsock->close();
		delete rsock;
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In UpdateRecovery, response_ad ATTR_ERROR_CODE is zero\n");
	}
	dprintf(D_FULLDEBUG, "In UpdateRecovery, return 0 for %s\n", cached_server.c_str());
	rsock->close();
	delete rsock;
	return 0;
}

int CachedServer::ReceiveRequestRecovery(int /* cmd */, Stream* sock) {
	// Get the URL from the incoming classad
	dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, entering ReceiveRequestRedundancy\n");//##
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed to read request for ReceiveRequestRedundancy.\n");
		return 1;
	}

	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include condor version.\n");
		return 1;
	}
	if(version != CondorVersion()) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery condor version mismatches\n");
	}

	time_t lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string cache_owner;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	int redundancy_id;
	std::string transfer_redundancy_files;
	double max_failure_rate;
	std::string recovery_sources;
	std::string recovery_ids;
	std::string encode_technique;
	int encode_field_size;
	int encode_packet_size;
	int encode_buffer_size;

	if (!request_ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include cache_name\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include cache_id\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include redundancy_source\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include lease_expiry\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include cache_owner\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include redundancy_manager\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include redundancy_method\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include redundancy_selection\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include redundancy_candidates\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include redundancy_ids\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include data_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include parity_number\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrInt("RedundancyID", redundancy_id))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include redundancy_id\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include transfer_redundancy_files\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include max_failure_rate\n");
		return 1;
	}
	// get erasure coding information
	if (redundancy_method == "ErasureCoding" && !request_ad.EvaluateAttrString("EncodeCodeTech", encode_technique)) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include EncodeCodeTech in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !request_ad.EvaluateAttrInt("EncodeFieldSize", encode_field_size)) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include EncodeFieldSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !request_ad.EvaluateAttrInt("EncodePacketSize", encode_packet_size)) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include EncodePacketSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !request_ad.EvaluateAttrInt("EncodeBufferSize", encode_buffer_size)) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include EncodeBufferSize in request\n");
		return 1;
	}
	if (!request_ad.EvaluateAttrString("RecoverySources", recovery_sources))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include recovery_sources\n");
		return 1;
	}
	std::vector<std::string> recovery_sources_vec;
	boost::split(recovery_sources_vec, recovery_sources, boost::is_any_of(","));
	if (!request_ad.EvaluateAttrString("RecoveryIDs", recovery_ids))
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, request_ad does not include recovery_ids\n");
		return 1;
	}
	std::vector<std::string> recovery_ids_vec;
	boost::split(recovery_ids_vec, recovery_ids, boost::is_any_of(","));

	dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, recovery_sources = %s, recovery_ids = %s, TransferRedundancyFiles = %s\n", recovery_sources.c_str(), recovery_ids.c_str(), transfer_redundancy_files.c_str());//##

	std::string dirname = cache_name + "+" + cache_id_str;
	std::string directory = GetRedundancyDirectory(dirname);
	dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, directory = %s\n", directory.c_str());
	int rc;
	rc = CreateRedundancyDirectory(dirname);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed to create redundancy directory\n");
		return 1;
	}

	// create Coding subdirectory when erasure coding is used
	if(redundancy_method == "ErasureCoding") {
		if(!directory.empty() && directory.back() == '/') {
			directory.pop_back();
		}
		directory += "/";
		directory += "Coding";
		if ( !mkdir_and_parents_if_needed(directory.c_str(), S_IRWXU, PRIV_CONDOR) ) {
			dprintf( D_FULLDEBUG, "In ReceiveRequestRecovery, couldn't create directory %s\n", directory.c_str());
			return 1;
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, created directory %s\n", directory.c_str());
		}
	}

	// we only need to download single copy from survivors if replication scheme is used; we need to download survivors (# survivors = # data_number)
	// if erasure coding is used, we iterate all cacheds which contain requested redundancy and immediately stop download redundancy once we get enough redundancy pieces.
	int n = -1;
	if(redundancy_method == "Replication") {
		n = recovery_sources_vec.size();
	} else if(redundancy_method == "ErasureCoding") {
		// recovery_ids_vec's size is greater or equal to data_number in RecoveryCacheRedundancy function
		n = recovery_sources_vec.size();
	}
	std::vector<std::string> recovery_success_sources_vec;
	std::vector<std::string> recovery_success_ids_vec;
	int success_cnt = 0;
	for(int i = 0; i < n; ++i) {
		// have already gotten enough redundancy pieces ?
		if(redundancy_method == "Replication" && success_cnt >= 1) break;
		if(redundancy_method == "ErasureCoding" && success_cnt >= data_number) break;
		// Initiate the transfer
		DaemonAllowLocateFull remote_cached(DT_CACHED, recovery_sources_vec[i].c_str());
		if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed to locate daemon...\n");
			return 1;
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, located daemon at %s\n", remote_cached.name());
		}

		ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
				CACHED_DOWNLOAD_REDUNDANCY, Stream::reli_sock, 20 );

		// TODO: we should handle the failure here. We should collect all available redundancy sources and decide to 
		// download redundancy from another redundancy source if some redundancy source is not available
		if(!rsock || rsock->is_closed()) {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, rsock failed\n");
			return 1;
		}
		compat_classad::ClassAd send_ad = request_ad;
		// do not forget to insert redundancy_id to download corresponding files
		send_ad.InsertAttr("RedundancyID", stoi(recovery_ids_vec[i]));
		// do not forget to insert is_recovery as true
		send_ad.InsertAttr("IsRecovery", true);
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, redundancy id = %d\n", stoi(recovery_ids_vec[i]));
		if (!putClassAd(rsock, send_ad) || !rsock->end_of_message())
		{
			// Can't send another response!  Must just hang-up.
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed to send send_ad to remote cached\n");
			rsock->close();
			delete rsock;
			return 1;
		}

		// Receive the response
		compat_classad::ClassAd receive_ad;
		rsock->decode();
		if (!getClassAd(rsock, receive_ad) || !rsock->end_of_message())
		{
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed to receive receive_ad from remote cached\n");
			rsock->close();
			delete rsock;
			return 1;
		}

		// We are the client, act like it.
		FileTransfer* ft = new FileTransfer();
		compat_classad::ClassAd* transfer_ad = new compat_classad::ClassAd();
		transfer_ad->InsertAttr(ATTR_JOB_IWD, directory);
		transfer_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, directory);
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, directory here is %s\n", directory.c_str());

		// TODO: Enable file ownership checks
		rc = ft->SimpleInit(transfer_ad, false, true, static_cast<ReliSock*>(rsock));
		if (!rc) {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed simple init\n");
			rsock->close();
			delete rsock;
			return 1;
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, successfully SimpleInit of filetransfer\n");
		}

		ft->setPeerVersion(version.c_str());

		recovery_download_start = std::chrono::system_clock::now();
		rc = ft->DownloadFiles();
		recovery_download_end = std::chrono::system_clock::now();
		std::chrono::duration<double> recovery_tmp_duration(0);
		recovery_tmp_duration = (recovery_download_end - recovery_download_start);
		recovery_download_duration += recovery_tmp_duration;
		recovery_download_count += 1;
		time_t now = time(NULL);
		network_transfer_fs << now << ", " << "recovery_download, " << recovery_sources_vec[i].c_str() << ", " << m_daemonName.c_str() << ", " << recovery_tmp_duration.count() << std::endl;
		total_download_duration += recovery_tmp_duration;
		total_download_count += 1;
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, recovery_download_duration = %f, recovery_download_count = %llu\n", recovery_download_duration.count(), recovery_download_count);//##
		FileTransfer::FileTransferInfo fi = ft->GetInfo();
		if (fi.success) {
			success_cnt++;
			recovery_success_sources_vec.push_back(recovery_sources_vec[i]);
			recovery_success_ids_vec.push_back(recovery_ids_vec[i]);
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, succeeded, fi.bytes = %d, fi.duration = %f\n", fi.bytes, fi.duration);
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed, fi.bytes = %d, fi.duration = %f\n", fi.bytes, fi.duration);
		}

		if (!rc) {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed DownloadFiles\n");
			rsock->close();
			delete rsock;
			return 1;
		} else {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, successfully began downloading files\n");
		}
		rsock->close();
		delete rsock;
	}

	// re-create recovery_sources and recovery_ids
	recovery_sources = "";
	recovery_ids = "";
	for(int i = 0; i < recovery_success_sources_vec.size(); ++i) {
		recovery_sources += recovery_success_sources_vec[i];
		recovery_sources += ",";
	}
	for(int i = 0; i < recovery_success_ids_vec.size(); ++i) {
		recovery_ids += recovery_success_ids_vec[i];
		recovery_ids += ",";
	}
	if(!recovery_sources.empty() && recovery_sources.back() == ',') {
		recovery_sources.pop_back();
	}
	if(!recovery_ids.empty() && recovery_ids.back() == ',') {
		recovery_ids.pop_back();
	}
	// update RecoverySources and RecoveryIDs in request_ad.
	request_ad.InsertAttr("RecoverySources", recovery_sources);
	request_ad.InsertAttr("RecoveryIDs", recovery_ids);
	dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, recovery_sources = %s, recovery_ids = %s\n", recovery_sources.c_str(), recovery_ids.c_str());

	// decode directory if RedundancyMethod is ErasureCoding
	if(redundancy_method == "ErasureCoding") {
		ec_start = std::chrono::system_clock::now();
		std::string decode_directory = GetTransferRedundancyDirectory(dirname);
		std::vector<std::string> transfer_file_list;
		boost::split(transfer_file_list, transfer_redundancy_files, boost::is_any_of(","));
		ErasureCoder *coder = new ErasureCoder();
		for(int i = 0; i < transfer_file_list.size(); ++i) {
			const std::string absolute_decode_file = decode_directory + transfer_file_list[i];
			// prepare for return parameters
			int return_k = -1;
			int return_m = -1;
			std::string return_code_tech = "";
			int return_w = -1;
			int return_packetsize = -1;
			int return_buffersize = -1;
			int return_val;
			return_val = coder->JerasureDecodeFile(absolute_decode_file, return_k, return_m, return_code_tech, return_w, return_packetsize, return_buffersize);
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, decode files, absolute_decode_file = %s, return_k = %d, return_m = %d, return_code_tech = %s, return_w = %d, return_packetsize = %d, return_buffersize = %d\n", absolute_decode_file.c_str(), return_k, return_m, return_code_tech.c_str(), return_w, return_packetsize, return_buffersize);//##
			if(return_k < 0 || return_m < 0 || return_code_tech.empty() || return_w < 0 || return_packetsize < 0 || return_buffersize < 0) {
				dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, decode parameters are not correct\n");
				return 1;
			}
			boost::filesystem::path p_to{absolute_decode_file};
			// get prefix of full file path except the last one
			std::string from_redundancy_name;
			std::string from_meta_name;
			std::string to_redundancy_name;
			std::string to_meta_name;
			std::string decoded_file_name;
			std::string prefix = decode_directory;
			to_redundancy_name += prefix;
			to_meta_name += prefix;
			prefix += "Coding/";
			// abc.txt -> ["abc", "txt"] -> abc_decoded.txt
			std::vector<std::string> name_pieces;
			boost::split(name_pieces, transfer_file_list[i], boost::is_any_of("."));
			from_redundancy_name += prefix;
			from_meta_name += prefix;
			decoded_file_name += prefix;
			// get erasure coded piece file name
			if(redundancy_id <= data_number) {
				from_redundancy_name += name_pieces[0] + "_k" + std::to_string(redundancy_id);
				to_redundancy_name += name_pieces[0] + "_k" + std::to_string(redundancy_id);
			} else if(redundancy_id > data_number && redundancy_id <= (data_number+parity_number)) {
				from_redundancy_name += name_pieces[0] + "_m" + std::to_string(redundancy_id-data_number);
				to_redundancy_name += name_pieces[0] + "_m" + std::to_string(redundancy_id-data_number);
			}
			from_meta_name += name_pieces[0] + "_meta";
			to_meta_name += name_pieces[0] + "_meta";
			decoded_file_name += name_pieces[0] + "_decoded";
			// get suffix of the last one
			std::string suffix;
			for(int j = 1; j < name_pieces.size(); ++j) {
				suffix += ".";
				suffix += name_pieces[j];
			}
			from_redundancy_name += suffix;
			from_meta_name += ".txt";
			to_redundancy_name += suffix;
			to_meta_name += ".txt";
			decoded_file_name += suffix;
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, decoded_file_name = %s, from_redundancy_name = %s, from_meta_name = %s, to_redundancy_name = %s, to_meta_name = %s\n", decoded_file_name.c_str(), from_redundancy_name.c_str(), from_meta_name.c_str(), to_redundancy_name.c_str(), to_meta_name.c_str());
			boost::filesystem::path p_from{decoded_file_name};
			rename(p_from, p_to);
			std::vector<std::string> return_files = coder->JerasureEncodeFile(absolute_decode_file, return_k, return_m, encode_technique, encode_field_size, encode_packet_size, encode_buffer_size);
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, re-encode files, absolute_decode_file = %s, return_k = %d, return_m = %d, encode_technique = %s, encode_field_size = %d, encode_packet_size = %d, encode_buffer_size = %d\n", absolute_decode_file.c_str(), return_k, return_m, encode_technique.c_str(), encode_field_size, encode_packet_size, encode_buffer_size);//##
			boost::filesystem::path from_redundancy_path{from_redundancy_name};
			boost::filesystem::path to_redundancy_path{to_redundancy_name};
			boost::filesystem::path from_meta_path{from_meta_name};
			boost::filesystem::path to_meta_path{to_meta_name};
			rename(from_redundancy_path, to_redundancy_path);
			rename(from_meta_path, to_meta_path);
		}
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, delete files\n");
		// delete all original files in erasure coding
		for(int i = 0; i < transfer_file_list.size(); ++i) {
			dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, transfer_file_list[%d] = %s\n", i, transfer_file_list[i].c_str());//##
			std::string delete_file_name = decode_directory + transfer_file_list[i];
			if(boost::filesystem::exists(delete_file_name)) {
				boost::filesystem::remove_all(delete_file_name);
			}
		}
		// delete Coding subdirectory in erasure coding
		std::string delete_file_name = decode_directory + "Coding";
		if(boost::filesystem::exists(delete_file_name)) {
			boost::filesystem::remove_all(delete_file_name);
		}

		ec_end = std::chrono::system_clock::now();
		std::chrono::duration<double> ec_duration(0);
		ec_duration = (ec_end - ec_start);
		time_t now = time(NULL);
		network_transfer_fs << now << ", " << "ec, " << m_daemonName.c_str() << ", " << ec_duration.count() << std::endl;
		delete coder;
	}

	rc = CommitCache(request_ad);
	if(rc) {
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, CommitCache failed\n");
		return 1;
	}

	compat_classad::ClassAd response_ad;
	// send RecoverySources and RecoveryIDs back to RequestRecovery since they need this information to update other cache candidates.
	response_ad.InsertAttr("RecoverySources", recovery_sources);
	response_ad.InsertAttr("RecoveryIDs", recovery_ids);
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);
	response_ad.InsertAttr(ATTR_ERROR_STRING, "SUCCEEDED");
	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In ReceiveRequestRecovery, failed to send response_ad to remote cached\n");
		return 1;
	}

	return 0;
}

int CachedServer::RequestRecovery(const std::string& cached_server, compat_classad::ClassAd& request_ad, compat_classad::ClassAd& response_ad) {
	dprintf(D_FULLDEBUG, "In RequestRecovery, entering func cached_server = %s\n", cached_server.c_str());
	DaemonAllowLocateFull remote_cached(DT_CACHED, cached_server.c_str());
	if(!remote_cached.locate(Daemon::LOCATE_FULL)) {
		dprintf(D_FULLDEBUG, "In RequestRecovery, failed to locate daemon...\n");
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In RequestRecovery, located daemon at %s\n", remote_cached.name());
	}

	ReliSock *rsock = (ReliSock *)remote_cached.startCommand(
			CACHED_REQUEST_RECOVERY, Stream::reli_sock, 20 );

	std::string version = CondorVersion();
	request_ad.InsertAttr("CondorVersion", version);
	if(!rsock || rsock->is_closed()) {
		dprintf(D_FULLDEBUG, "In RequestRecovery, rsock failed\n");
		return 1;
	}
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		dprintf(D_FULLDEBUG, "In RequestRecovery, failed to send send_ad to remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}

	// Receive the response
	rsock->decode();
	if (!getClassAd(rsock, response_ad) || !rsock->end_of_message())
	{
		dprintf(D_FULLDEBUG, "In RequestRecovery, failed to receive receive_ad from remote cached\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	int rc;
	if (!response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		dprintf(D_FULLDEBUG, "In RequestRecovery, response_ad does not include ATTR_ERROR_CODE\n");
		rsock->close();
		delete rsock;
		return 1;
	}
	if(rc) {
		dprintf(D_FULLDEBUG, "In RequestRecovery, response_ad ATTR_ERROR_CODE is not zero\n");
		rsock->close();
		delete rsock;
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "In RequestRecovery, response_ad ATTR_ERROR_CODE is zero\n");
	}
	dprintf(D_FULLDEBUG, "In RequestRecovery, return 0 for %s\n", cached_server.c_str());
	rsock->close();
	delete rsock;
	return 0;
}

int CachedServer::CacheStateTransition(compat_classad::ClassAd& ad, std::unordered_map<std::string, std::string>& alive_map) {
	// we need to consider different cases for a cache:
	// 1. if there are any EXPIRED heartbeats, this cache either becomes finished or died.
	// 	1.1 if ON and EXPIRED are more than required survivors to reconstruct the cache, this cache goes to finished set
	// 	1.2 otherwise, this cache goes to died set
	// 2. all CacheDs are ON ---- this cache is healthy
	// 3. ON and EXPIRED are more than required survivors to reconstruct the cache, this cache goes to DOWN and try to recovery it
	// 4. ON and EXPIRED are less than required survivors, goes to DANGER and wait there (sometimes due to heartbeat lag,
	//    this cache might appear to be healthy in next heartbeat cycle
	// 5. If a cache lingers in DANGER state over 10 minutes (also the legacy heartbeat when redundancy caches expire in worker nodes),
	//    we move this cache to died set.
	// above cases assume all CacheDs report the cache'redundancy to this manager
	dprintf(D_FULLDEBUG, "In EvaluateCacheState 1\n");	
	long long int lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	std::string cache_owner;
	int redundancy_id;
	std::string transfer_redundancy_files;
	double max_failure_rate;
	std::string encode_technique;
	int encode_field_size;
	int encode_packet_size;
	int encode_buffer_size;

	if (!ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include lease_expiry\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include cache_name\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include cache_id_str\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include redundancy_source\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include redundancy_manager\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include redundancy_method\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include redundancy_selection\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include redundancy_candidates\n");
		return 1;
	}
	if (!ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include data_number\n");
		return 1;
	}
	if (!ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include parity_number\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include cache_owner\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include redundancy_ids\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include transfer_redundancy_files\n");
		return 1;
	}
	if (!ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include max_failure_rate\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrString("EncodeCodeTech", encode_technique)) {
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include EncodeCodeTech in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodeFieldSize", encode_field_size)) {
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include EncodeFieldSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodePacketSize", encode_packet_size)) {
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include EncodePacketSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodeBufferSize", encode_buffer_size)) {
		dprintf(D_FULLDEBUG, "In CacheStateTransition, classad does not include EncodeBufferSize in request\n");
		return 1;
	}

	dprintf(D_FULLDEBUG, "In CacheStateTransition 2\n");	

	int on_count = 0;
	int expired_count = 0;
	int off_count = 0;
	int proactive_count = 0;
	// proactive vector in which an element is a pair that maps between cached name and cached start time
	std::vector<std::pair<std::string, long long int>> proactive_vector;
	int required_survivor = -1;
	if(redundancy_method == "Replication") {
		required_survivor = 1;
	} else if(redundancy_method == "ErasureCoding") {
		required_survivor = data_number;
	}
	if(required_survivor < 0) {
		dprintf(D_FULLDEBUG, "In CacheStateTransition, required_survior less than 0\n");
		return 1;
	}
	time_t now = time(NULL);
	for(std::unordered_map<std::string, std::string>::iterator it = alive_map.begin(); it != alive_map.end(); ++it) {
		if(it->second == "ON") {
			on_count++;
			// Proactive case
			std::string cached_name = it->first;
			time_t boot_time = cached_boot_time_map[cached_name];
			if (now - boot_time > PROACTIVE_TIME * 60) {
				proactive_count++;
				std::pair<std::string, long long int> p = std::make_pair(cached_name, boot_time);
				proactive_vector.push_back(p);
			}
		} else if(it->second == "OFF") {
			off_count++;
		} else if(it->second == "EXPIRED") {
			expired_count++;
		}
	}

	if(proactive_count) {
		// sort proactive vector by the start time
		std::sort(proactive_vector.begin(), proactive_vector.end(), [](std::pair<std::string,long long int> a, std::pair<std::string,long long int> b){return a.second < b.second;});
	}

	std::string cache_key = cache_name + "+" + cache_id_str;
	// enter to the state machine
	if((health_state_set.find(cache_key) == health_state_set.end()) && (down_state_set.find(cache_key) == down_state_set.end()) && (danger_state_set.find(cache_key) == danger_state_set.end())) {
		if(on_count == data_number + parity_number) {
			health_state_set.insert(cache_key);
			dprintf(D_FULLDEBUG, "In CacheStateTransition, insert cache %s into HEALTH state\n", cache_key.c_str());
		} else if(on_count >= required_survivor) {
			down_state_set.insert(cache_key);
			dprintf(D_FULLDEBUG, "In CacheStateTransition, insert cache %s into DOWN state\n", cache_key.c_str());
		} else if(on_count < required_survivor) {
			danger_state_set.insert(cache_key);
			dprintf(D_FULLDEBUG, "In CacheStateTransition, insert cache %s into DANGER state\n", cache_key.c_str());
		}
		return 0;
	}

	// state machine state transition
	if(health_state_set.find(cache_key) != health_state_set.end()) {
		if(expired_count > 0) {
			if(expired_count+on_count >= required_survivor) {
				// cache finished successfully
				health_state_set.erase(cache_key);
				finished_set.insert(cache_key);
			} else {
				// cache dead
				health_state_set.erase(cache_key);
				died_set.insert(cache_key);
			}
		} else {
			if(on_count == data_number + parity_number) {
				// stay in health_state
				if(proactive_count) {
					std::string cached_name = proactive_vector[0].first;
					health_state_set.erase(cache_key);
					down_state_set.insert(cache_key);
					redundancy_host_map[cache_key]->erase(cached_name);
				}
			} else if(on_count >= required_survivor) {
				// go to DOWN state
				health_state_set.erase(cache_key);
				down_state_set.insert(cache_key);
			} else if(on_count < required_survivor) {
				// go to DANGER state
				health_state_set.erase(cache_key);
				danger_state_set.insert(cache_key);
			} else {
				dprintf(D_FULLDEBUG, "In CacheStateTransition, cache %s in HEALTH state, but unknown transition\n", cache_key.c_str());
			}
		}
	} else if(down_state_set.find(cache_key) != down_state_set.end()) {
		if(expired_count > 0) {
			if(expired_count+on_count >= required_survivor) {
				// cache finished successfully
				down_state_set.erase(cache_key);
				finished_set.insert(cache_key);
			} else {
				// cache dead
				down_state_set.erase(cache_key);
				died_set.insert(cache_key);
			}
		} else {
			if(on_count == data_number + parity_number) {
				// go to HEALTH state
				down_state_set.erase(cache_key);
				health_state_set.insert(cache_key);
			} else if(on_count >= required_survivor) {
				// stay in DOWN state
			} else if(on_count < required_survivor) {
				// go to DANGER state
				down_state_set.erase(cache_key);
				danger_state_set.insert(cache_key);
			} else {
				dprintf(D_FULLDEBUG, "In CacheStateTransition, cache %s in DOWN state, but unknown transition\n", cache_key.c_str());
			}
		}
	} else if(danger_state_set.find(cache_key) != danger_state_set.end()) {
		if(expired_count > 0) {
			if(expired_count+on_count >= required_survivor) {
				// cache finished successfully
				danger_state_set.erase(cache_key);
				finished_set.insert(cache_key);
			} else {
				// cache dead
				danger_state_set.erase(cache_key);
				died_set.insert(cache_key);
			}
		} else {
			if(on_count == data_number + parity_number) {
				// go to HEALTH state
				danger_state_set.erase(cache_key);
				health_state_set.insert(cache_key);
			} else if(on_count >= required_survivor) {
				// stay in DOWN state
				danger_state_set.erase(cache_key);
				down_state_set.insert(cache_key);
			} else if(on_count < required_survivor) {
				// move caches which have been lingering for over 3 minutes
				time_t now = time(NULL);
				long long int time_to_failure_seconds = (lease_expiry + 180 - now);
				if(time_to_failure_seconds <= 0) {
					danger_state_set.erase(cache_key);
					died_set.insert(cache_key);
				}
			} else {
				dprintf(D_FULLDEBUG, "In CacheStateTransition, cache %s in DANGER state, but unknown transition\n", cache_key.c_str());
			}
		}
	}
			
	if(down_state_set.find(cache_key) != down_state_set.end()) {
		dprintf(D_FULLDEBUG, "In CacheStateTransition, cache %s is recovering\n", cache_key.c_str());
		int rec = RecoverCacheRedundancy(ad, alive_map, proactive_vector);
		if(rec) {
			dprintf(D_FULLDEBUG, "In CacheStateTransition, recovery failed\n");
		}
	}

	return 0;
}

int CachedServer::RecoverCacheRedundancy(compat_classad::ClassAd& ad, std::unordered_map<std::string, std::string>& alive_map, std::vector<std::pair<std::string, long long int>>& proactive_vector) {

	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 1\n");	
	long long int lease_expiry;
	std::string cache_name;
	std::string cache_id_str;
	std::string redundancy_source;
	std::string redundancy_manager;
	std::string redundancy_method;
	std::string redundancy_selection;
	std::string redundancy_flexibility;
	std::string redundancy_candidates;
	std::string redundancy_ids;
	int data_number;
	int parity_number;
	std::string cache_owner;
	int redundancy_id;
	std::string transfer_redundancy_files;
	double max_failure_rate;
	std::string encode_technique;
	int encode_field_size;
	int encode_packet_size;
	int encode_buffer_size;

	if (!ad.EvaluateAttrInt(ATTR_LEASE_EXPIRATION, lease_expiry))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include lease_expiry\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_CACHE_NAME, cache_name))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include cache_name\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_CACHE_ID, cache_id_str))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include cache_id_str\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancySource", redundancy_source))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include redundancy_source\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyManager", redundancy_manager))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include redundancy_manager\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include redundancy_method\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include redundancy_selection\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include redundancy_flexibility\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include redundancy_candidates\n");
		return 1;
	}
	if (!ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include data_number\n");
		return 1;
	}
	if (!ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include parity_number\n");
		return 1;
	}
	if (!ad.EvaluateAttrString(ATTR_OWNER, cache_owner))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include cache_owner\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include redundancy_ids\n");
		return 1;
	}
	if (!ad.EvaluateAttrString("TransferRedundancyFiles", transfer_redundancy_files))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include transfer_redundancy_files\n");
		return 1;
	}
	if (!ad.EvaluateAttrReal("MaxFailureRate", max_failure_rate))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include max_failure_rate\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrString("EncodeCodeTech", encode_technique)) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include EncodeCodeTech in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodeFieldSize", encode_field_size)) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include EncodeFieldSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodePacketSize", encode_packet_size)) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include EncodePacketSize in request\n");
		return 1;
	}
	if (redundancy_method == "ErasureCoding" && !ad.EvaluateAttrInt("EncodeBufferSize", encode_buffer_size)) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, classad does not include EncodeBufferSize in request\n");
		return 1;
	}

	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 2\n");	

	std::unordered_map<std::string, std::string> id_candidate_map;
	std::unordered_map<std::string, std::string> candidate_id_map;
	std::vector<std::string> candidates;
	std::vector<std::string> ids;
	boost::split(candidates, redundancy_candidates, boost::is_any_of(","));
	boost::split(ids, redundancy_ids, boost::is_any_of(","));
	for(int i = 0; i < candidates.size(); ++i) {
		id_candidate_map[ids[i]] = candidates[i];
		candidate_id_map[candidates[i]] = ids[i];
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 3\n");	

	std::vector<std::string> constraint;
	std::vector<std::string> blockout;
	std::vector<std::string> proactive;
	for(std::unordered_map<std::string, std::string>::iterator it = alive_map.begin(); it != alive_map.end(); ++it) {
		if(it->second == "ON") {
			// Only recover the first proactive cached.
			if((!proactive_vector.empty()) && (it->first == proactive_vector[0].first)) {
				blockout.push_back(it->first);
				proactive.push_back(it->first);
			} else {
				constraint.push_back(it->first);
			}
		} else if(it->second == "OFF") {
			blockout.push_back(it->first);
		}
	}

	std::string location_constraint;
	std::string id_constraint;
	for(int i = 0; i < constraint.size(); ++i) {
		location_constraint += constraint[i];
		location_constraint += ",";
		id_constraint += candidate_id_map[constraint[i]];
		id_constraint += ",";
	}
	if(!location_constraint.empty() && location_constraint.back() == ',') {
		location_constraint.pop_back();
	}
	if(!id_constraint.empty() && id_constraint.back() == ',') {
		id_constraint.pop_back();
	}
	// redundancy manager should also be blocked out
	blockout.push_back(m_daemonName);
	std::string location_blockout;
	for(int i = 0; i < blockout.size(); ++i) {
		location_blockout += blockout[i];
		location_blockout += ",";
	}
	if(!location_blockout.empty() && location_blockout.back() == ',') {
		location_blockout.pop_back();
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 4, location_constraint = %s, id_constraint = %s, location_blockout = %s\n", location_constraint.c_str(), id_constraint.c_str(), location_blockout.c_str());	

	time_t now = time(NULL);
	long long int time_to_failure_minutes = (lease_expiry - now) / 60;
	if (time_to_failure_minutes <= 0) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, time_to_failure_minutes is less than 0, do not need to recovery this cache\n");
		std::string dirname = cache_name + "+" + cache_id_str;
		// Update cache state to OBSOLETE only in CacheManager
		// TODO: maybe we need to delete legacy caches in worker CacheDs
		m_log->BeginTransaction();
		int state = OBSOLETE;
		SetAttributeInt(dirname, ATTR_CACHE_STATE, state);
		m_log->CommitTransaction();
		return 0;
	}

	compat_classad::ClassAd require_ad;
	std::string version = CondorVersion();
	require_ad.InsertAttr("CondorVersion", version);
	require_ad.InsertAttr("LocationConstraint", location_constraint);
	require_ad.InsertAttr("LocationBlockout", location_blockout);
	require_ad.InsertAttr("IDConstraint", id_constraint);
	require_ad.InsertAttr("DataNumberConstraint", data_number);
	require_ad.InsertAttr("ParityNumberConstraint", parity_number);
	require_ad.InsertAttr("MethodConstraint", redundancy_method);
	require_ad.InsertAttr("SelectionConstraint", redundancy_selection);
	require_ad.InsertAttr("FlexibilityConstraint", redundancy_flexibility);
	require_ad.InsertAttr("MaxFailureRate", max_failure_rate);
	require_ad.InsertAttr("TimeToFailureMinutes", time_to_failure_minutes);
	// TODO: may add CacheSize logistics because erasure coding can change the actually size stored on each individual CacheD
	require_ad.InsertAttr("CacheSize", 0);
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 5\n");	

	compat_classad::ClassAd policy_ad;
	int rc;
	rc = NegotiateCacheflowManager(require_ad, policy_ad);
	if(rc) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, NegotiateCacheflowManager failed\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 6\n");	

	// Get a new set of attributes
	if (!policy_ad.EvaluateAttrString("RedundancyCandidates", redundancy_candidates))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, policy_ad did not include redundancy_candidates\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancyMap", redundancy_ids))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, policy_ad did not include redundancy_ids\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancyMethod", redundancy_method))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, policy_ad did not include redundancy_method\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancySelection", redundancy_selection))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, policy_ad did not include redundancy_selection\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrString("RedundancyFlexibility", redundancy_flexibility))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, policy_ad did not include redundancy_flexibility\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrInt("DataNumber", data_number))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, policy_ad did not include data_number\n");
		return 1;
	}
	if (!policy_ad.EvaluateAttrInt("ParityNumber", parity_number))
	{
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, policy_ad did not include parity_number\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 7, redundancy_candidates = %s, redundancy_map = %s\n", redundancy_candidates.c_str(), redundancy_ids.c_str());	
	std::vector<std::string> new_candidates;
	std::vector<std::string> new_ids;
	boost::split(new_candidates, redundancy_candidates, boost::is_any_of(","));
	boost::split(new_ids, redundancy_ids, boost::is_any_of(","));

	// reconstruct_cached_vec store newly added cacheds in which the reconstruction of orginal cache should be adopted
	std::vector<std::string> reconstruct_cached_vec;
	int new_cached_count = 0;
	// we should clear candidate_id_map and id_candidate_map because new_candidates could be less than candidates
	candidate_id_map.clear();
	id_candidate_map.clear();
	for(int i = 0; i < new_candidates.size(); ++i) {
		if(find(candidates.begin(), candidates.end(), new_candidates[i]) == candidates.end()) {
			reconstruct_cached_vec.push_back(new_candidates[i]);
			new_cached_count++;
		}
		// recreated maps
		candidate_id_map[new_candidates[i]] = new_ids[i];
		id_candidate_map[new_ids[i]] = new_candidates[i];
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, new_cached_count = %d\n", new_cached_count);
	std::string new_redundancy_ids;
	std::string new_redundancy_candidates;
	for(int i = 0; i < new_candidates.size(); ++i) {
		std::string key = std::to_string(i+1);
		new_redundancy_candidates += id_candidate_map[key];
		new_redundancy_candidates += ",";
		new_redundancy_ids += key;
		new_redundancy_ids += ",";
	}
	if(!new_redundancy_candidates.empty() && new_redundancy_candidates.back() == ',') {
		new_redundancy_candidates.pop_back();
	}
	if(!new_redundancy_ids.empty() && new_redundancy_ids.back() == ',') {
		new_redundancy_ids.pop_back();
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 8\n");	

	// recovery failure
	std::string recovery_sources;
	std::string recovery_ids;
	std::vector<std::string> recovery_sources_vec;
	std::vector<std::string> recovery_ids_vec;
	// when erasure coding is used, we need to retrieve at least data_number pieces of data to reconstruct an original file and we just choose
	// the first survivors in constraint array;
	// when replication is used, we only need one data to reconstruct an original file
	// but here, we send all available redundancy information to the cached that is going to recver the failure. In that cached, we immediately start recovery and stop downloading redundancy if that cached gets enough redundancy pieces: 1 for replication, k for erasure coding.
	int n = -1;
	if(redundancy_method == "Replication") {
		n = constraint.size();
	} else if(redundancy_method == "ErasureCoding") {
		n = constraint.size();
	}
	for(int i = 0; i < n; ++i) {
		recovery_sources_vec.push_back(constraint[i]);
		recovery_ids_vec.push_back(candidate_id_map[constraint[i]]);
	}
	for(int i = 0; i < recovery_sources_vec.size(); ++i) {
		recovery_sources += recovery_sources_vec[i];
		recovery_sources += ",";
	}
	for(int i = 0; i < recovery_ids_vec.size(); ++i) {
		recovery_ids += recovery_ids_vec[i];
		recovery_ids += ",";
	}
	if(!recovery_sources.empty() && recovery_sources.back() == ',') {
		recovery_sources.pop_back();
	}
	if(!recovery_ids.empty() && recovery_ids.back() == ',') {
		recovery_ids.pop_back();
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, recovery_sources = %s, recovery_ids = %s\n", recovery_sources.c_str(), recovery_ids.c_str());
	// send to reconstruct_cached_vec's cached servers to reconstruct failures
	compat_classad::ClassAd send_ad = policy_ad;
	send_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
	send_ad.InsertAttr(ATTR_CACHE_NAME, cache_name);
	send_ad.InsertAttr(ATTR_CACHE_ID, cache_id_str);
	send_ad.InsertAttr(ATTR_OWNER, cache_owner);
	send_ad.InsertAttr("RedundancySource", redundancy_source);
	send_ad.InsertAttr("RedundancyManager", redundancy_manager);
	send_ad.InsertAttr("TransferRedundancyFiles", transfer_redundancy_files);
	send_ad.InsertAttr("MaxFailureRate", max_failure_rate);
	if(redundancy_method == "ErasureCoding") {
		send_ad.InsertAttr("EncodeCodeTech", encode_technique);
		send_ad.InsertAttr("EncodeFieldSize", encode_field_size);
		send_ad.InsertAttr("EncodePacketSize", encode_packet_size);
		send_ad.InsertAttr("EncodeBufferSize", encode_buffer_size);
	}
	send_ad.InsertAttr("RecoverySources", recovery_sources);
	send_ad.InsertAttr("RecoveryIDs", recovery_ids);
	for(int i = 0; i < reconstruct_cached_vec.size(); ++i) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, RequestRecovery iteration is %d\n", i);
		const std::string cached_server = reconstruct_cached_vec[i];
		// don't forget to assign redundancy_id to this cached
		send_ad.InsertAttr("RedundancyID", stoi(candidate_id_map[cached_server]));
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, cached_server = %s, RedundancyID = %d\n", cached_server.c_str(), stoi(candidate_id_map[cached_server]));//##
		compat_classad::ClassAd receive_ad;
		rc = RequestRecovery(cached_server, send_ad, receive_ad);
		recovery_fs << now << ", " << "cache: " << cache_name << "+" << cache_id_str << ", " << "cached_server: " << cached_server << ", " << "recovery sources: " << recovery_sources << std::endl;
		if(rc) {
			dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, RequestRecovery failed for %s\n", cached_server.c_str());
		} else {
			dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, RequestRecovery succeeded for %s\n", cached_server.c_str());
		}
	}

	// send to constraint's cached servers to update candidates and ids
	for(int i = 0; i < constraint.size(); ++i) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, UpdateRecovery iteration is %d\n", i);
		const std::string cached_server = constraint[i];
		// don't forget to assign redundancy_id to this cached
		send_ad.InsertAttr("RedundancyID", stoi(candidate_id_map[cached_server]));
		send_ad.InsertAttr(ATTR_CACHE_STATE, COMMITTED);
		compat_classad::ClassAd receive_ad;
		rc = UpdateRecovery(cached_server, send_ad, receive_ad);
		if(rc) {
			dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, UpdateRecovery failed for %s\n", cached_server.c_str());
		} else {
			dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, UpdateRecovery succeeded for %s\n", cached_server.c_str());
		}
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 11\n");	

	// send to proactive's cached servers to update cache state to OBSOLETE
	for(int i = 0; i < proactive.size(); ++i) {
		dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, OBSOLETE iteration is %d\n", i);
		const std::string cached_server = proactive[i];
		// don't forget to assign OBSOLETE to this cached
		send_ad.InsertAttr(ATTR_CACHE_STATE, OBSOLETE);
		compat_classad::ClassAd receive_ad;
		rc = UpdateRecovery(cached_server, send_ad, receive_ad);
		if(rc) {
			dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, UpdateRecovery failed for %s\n", cached_server.c_str());
		} else {
			dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy, UpdateRecovery succeeded for %s\n", cached_server.c_str());
		}
	}
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 11\n");	

	// update redundancy locations on manager itself
	std::string dirname = cache_name + "+" + cache_id_str;
	m_log->BeginTransaction();
	SetAttributeString(dirname, "RedundancyCandidates", new_redundancy_candidates);
	SetAttributeString(dirname, "RedundancyMap", new_redundancy_ids);
	m_log->CommitTransaction();
	dprintf(D_FULLDEBUG, "In RecoverCacheRedundancy 12\n");	

	return 0;
}

void CachedServer::CheckRedundancyCacheds()
{
	dprintf(D_FULLDEBUG, "entering CheckRedundancyCacheds\n");
	cache_to_unordered::iterator it_cache = redundancy_host_map.begin();
	time_t now = time(NULL);
	std::set<std::string> current_cache_set;
	int current_on_count = 0;
	int current_off_count = 0;
	int current_expired_count = 0;
	while(it_cache != redundancy_host_map.end()) {

		std::string cache_key = it_cache->first;

		// this cache first appears in the map, there might be some lag of heartbeat signals, so skip this round.
		if(existed_set.find(cache_key) == existed_set.end()) {
			existed_set.insert(cache_key);
			it_cache++;
			continue;
		}
		// if died and finished caches, so we need to skip it
		if(died_set.find(cache_key) != died_set.end()) {
			it_cache++;
			continue;
		}
		if(finished_set.find(cache_key) != finished_set.end()) {
			it_cache++;
			continue;
		}

		time_t cache_expiry = cache_expiry_map[cache_key];

		string_to_time::iterator it_host = (it_cache->second)->begin();

		// this cache exists in this time point
		redundancy_map_fs << now << ", " << "CACHE: " << cache_key << std::endl;

		// hash map is used to store the current statuses of all CacheDs where this cache distributes redundancies
		std::unordered_map<std::string, std::string> alive_map;
		while(it_host != (it_cache->second)->end()) {
			std::string cached_name = it_host->first;
			time_t last_beat = it_host->second;
			// if the manager has not received heartbeat over 100 seconds, it needs to recover
			if(now - last_beat > 120) {
				if(now <= cache_expiry) {
					alive_map[cached_name] = "OFF";
					current_cache_set.insert(cache_key);
					current_off_count++;
					redundancy_map_fs << "OFF: " << cached_name << std::endl;
				} else {
					// should never happen, just for testing purpose
					alive_map[cached_name] = "EXPIRED";
					current_expired_count++;
					redundancy_map_fs << "EXPIRED: " << cached_name << std::endl;
				}
			} else {
				if(now <= cache_expiry) {
					alive_map[cached_name] = "ON";
					current_cache_set.insert(cache_key);
					current_on_count++;
					redundancy_map_fs << "ON: " << cached_name << std::endl;
				} else {
					// should never happen, just for testing purpose
					alive_map[cached_name] = "EXPIRED";
					current_expired_count++;
					redundancy_map_fs << "EXPIRED: " << cached_name << std::endl;
				}
			}
			it_host++;
		}
		redundancy_map_fs << "END CACHE" << std::endl;

		// delete all OFF cached entries for this cache
		std::unordered_map<std::string, std::string>::iterator it_alive_map = alive_map.begin();
		while(it_alive_map != alive_map.end()) {
			std::string cached_name = it_alive_map->first;
			if(it_alive_map->second == "OFF") {
				// remove failed cached entry
				dprintf(D_FULLDEBUG, "In CheckRedundancyCacheds, remove %s for %s\n", cached_name.c_str(), cache_key.c_str());
				(it_cache->second)->erase(cached_name);
			}
			it_alive_map++;
		}

		// now it's time to try to recover the cache.
		// all CacheD entries for this cache should be ON
		std::vector<std::string> cache_name_id;
		boost::split(cache_name_id, cache_key, boost::is_any_of("+"));
		std::string cache_name = cache_name_id[0];
		std::string cache_id_str = cache_name_id[1];
		char buf[512];
		sprintf(buf, "(%s == \"%s\") && (%s == \"%s\")", ATTR_CACHE_NAME, cache_name.c_str(), ATTR_CACHE_ID, cache_id_str.c_str());
		std::list<compat_classad::ClassAd> caches = QueryCacheLog(buf);
		if(caches.size() > 1) {
			dprintf(D_FULLDEBUG, "In CheckRedundancyCacheds, there are multiple same cache records\n");
		}
		compat_classad::ClassAd cache = caches.front();
		int res = CacheStateTransition(cache, alive_map);
		if(res) {
			dprintf(D_FULLDEBUG, "In CheckRedundancyCacheds, CacheStateTransition failed\n");
		}
		it_cache++;
	}
	// recording current storage cost
	redundancy_count_fs << now << ", " << current_cache_set.size() << ", " << current_on_count << ", " << current_off_count << ", " << current_expired_count << ", " << initialized_set.size() << ", " << finished_set.size() << ", " << died_set.size() << ", " << existed_set.size() << std::endl;
	network_perf_fs << now << ", " << m_daemonName.c_str() << ", " << upload_count << ", " << upload_duration.count() << ", " << total_download_count << ", " << total_download_duration.count() << ", " << write_download_count << ", " << write_download_duration.count() << ", " << recovery_download_count << ", " << recovery_download_duration.count() << std::endl;
	dprintf(D_FULLDEBUG, "exiting CheckRedundancyCacheds\n");
	daemonCore->Reset_Timer(m_check_redundancy_cached_timer, 120);
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


