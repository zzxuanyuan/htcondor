#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "cached_server.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "get_daemon_name.h"
#include <list>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "directory.h"

#include "cached_torrent.h"


#define SCHEMA_VERSION 1

const int CachedServer::m_schema_version(SCHEMA_VERSION);
const char *CachedServer::m_header_key("CACHE_ID");

CachedServer::CachedServer():
	m_registered_handlers(false)
{
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
	}
	
	// Create the name of the cache
	char* raw_name = build_valid_daemon_name("cached");
	m_daemonName = raw_name;
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

	InitTracker();
	
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
	
	daemonCore->Reset_Timer(m_advertise_cache_daemon_timer, 60);
	
}


/**
	*	Advertise the caches stored on this server
	*
	*/
void CachedServer::AdvertiseCaches() {
	
	classad::ClassAdParser	parser;
	ExprTree	*tree;
	
	// Create the requirements expression
	char buf[512];
	sprintf(buf, "(%s == %i) && (%s =?= true)", ATTR_CACHE_STATE, COMMITTED, ATTR_CACHE_ORIGINATOR);
	dprintf(D_FULLDEBUG, "AdvertiseCaches: Cache Query = %s\n", buf);
	
	if ( !( tree = parser.ParseExpression(buf) )) {
		dprintf(D_ALWAYS | D_FAILURE, "AdvertiseCaches: Unable to parse expression %s\n", buf);
		return;
	}
		
	//TransactionSentry sentry(m_log);
	ClassAdLog::filter_iterator it(&m_log->table, tree, 1000);
	ClassAdLog::filter_iterator end(&m_log->table, NULL, 0, true);
	compat_classad::ClassAdList caches;
	
	while ( it != end ) {
		ClassAd* tmp_ad = *it++;
		if (!tmp_ad) {
			dprintf(D_FAILURE | D_ALWAYS, "AdvertiseCaches: Classad is blank\n");
			break;
		}
		std::string cache_name;
		if ( tmp_ad->EvaluateAttrString(ATTR_CACHE_NAME, cache_name) ) {
			dprintf(D_FAILURE | D_ALWAYS, "AdvertiseCaches: Cache exists, but has no name\n" );
			dPrintAd(D_FULLDEBUG, **it);
		}
		
		// Copy the classad, and insert into the caches
		ClassAd* newClassad = (ClassAd*)tmp_ad->Copy();
		caches.Insert(newClassad);

		dprintf(D_FULLDEBUG, "Found %s as a cache\n", cache_name.c_str());
		dPrintAd(D_FULLDEBUG, *newClassad);
		
		
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
	ClassAd *ad, *cache_ad;
	adList.Open();
	caches.Open();
	
	// Loop through the caches and the cached's and attempt to match.
	while ((ad = adList.Next())) {
		Daemon new_daemon(ad, DT_GENERIC, "");
		if(!new_daemon.locate()) {
			dprintf(D_ALWAYS | D_FAILURE, "Failed to locate daemon...\n");
			continue;
		} else {
			dprintf(D_FULLDEBUG, "Located daemon at %s\n", new_daemon.name());
		}
		ClassAdList matched_caches;
		
		while ((cache_ad = caches.Next())) {
		
			classad::MatchClassAd mad;
			bool match = false;
			
			mad.ReplaceLeftAd(ad);
                        dprintf(D_FULLDEBUG, "Left ad:\n");
                        dPrintAd(D_FULLDEBUG, *ad);
			mad.ReplaceRightAd(cache_ad);
                        dprintf(D_FULLDEBUG, "Right ad:\n");
                        dPrintAd(D_FULLDEBUG, *cache_ad);
			if (mad.EvaluateAttrBool("symmetricMatch", match) && match) {
				dprintf(D_FULLDEBUG, "Cache matched cached\n");
                                compat_classad::ClassAd *new_ad = (compat_classad::ClassAd*)cache_ad->Copy();
                                new_ad->SetParentScope(NULL);
				matched_caches.Insert(new_ad);
				
			} else {
				dprintf(D_FULLDEBUG, "Cache did not match cache\n");
			}
			mad.RemoveLeftAd();
			mad.RemoveRightAd();
		}
		
		//dPrintAd(D_FULLDEBUG, *ad);
		
		// Now send the matched caches to the remote cached
		if (matched_caches.Length() > 0) {
			// Start the command
			ReliSock *rsock = (ReliSock *)new_daemon.startCommand(
							CACHED_CREATE_REPLICA, Stream::reli_sock, 20 );
			
			if (!rsock) {
				dprintf(D_FAILURE | D_ALWAYS, "Failed to connect to remote system: %s\n", new_daemon.name());
				continue;
			}
			
			matched_caches.Open();
			
			for (int i = 0; i < matched_caches.Length(); i++) {
				ClassAd * ad = matched_caches.Next();
				
				if (!putClassAd(rsock, *ad) || !rsock->end_of_message())
				{
					// Can't send another response!  Must just hang-up.
					break;
				}
				
				// Now send the terminal classad
                                compat_classad::ClassAd final_ad = GenerateClassAd();
				final_ad.Assign("FinalReplicationRequest", true);
				if (!putClassAd(rsock, final_ad) || !rsock->end_of_message())
				{
					// Can't send another response!  Must just hang-up.
					break;
				}
				delete rsock;
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
	
	// TODO: Make requirements more dynamic by using ATTR values.
	log_ad.InsertAttr(ATTR_REQUIREMENTS, "MY.DiskUsage < TARGET.TotalDisk");
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
		// Anything that needs to be done when a cache uploaded is completed should be here
		dprintf(D_FULLDEBUG, "Finished transfer\n");
		filesize_t cache_size = m_server.CalculateCacheSize(m_cacheName);
		m_server.SetLogCacheSize(m_cacheName, cache_size);
		m_server.SetCacheUploadStatus(m_cacheName, CachedServer::COMMITTED);
		CondorError err;
		dprintf(D_FULLDEBUG, "Creatting torrent\n");
		std::string cache_dir = m_server.GetCacheDir(m_cacheName, err);
		MakeTorrent(cache_dir);
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
	UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	ft->RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
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

int CachedServer::ListCacheDirs(int /*cmd*/, Stream * /*sock*/)
{
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
		dprintf(D_ALWAYS | D_FAILURE, "Failed to read request for RemoveCacheDir.\n");
		return 1;
	}
	std::string dirname;
	std::string version;
	std::string replication_policy;
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
	// Re-create the machine's classad locally
	compat_classad::ClassAd my_ad = GenerateClassAd();
	while(true) {
		classad::MatchClassAd mad;
		bool match = false;
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
			mad.ReplaceLeftAd(&my_ad);
			mad.ReplaceRightAd(&request_ad);
			if (mad.EvaluateAttrBool("symmetricMatch", match) && match) {
				//dprintf(D_FULLDEBUG, "Cache matched cached\n");
                                compat_classad::ClassAd *new_ad = (compat_classad::ClassAd*)request_ad.Copy();
                                new_ad->SetParentScope(NULL);
                                replication_requests.Insert(new_ad);
				
			} else {
				//dprintf(D_FULLDEBUG, "Cache did not match cache\n");
			}
			mad.RemoveLeftAd();
			mad.RemoveRightAd();
			
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
	dprintf(D_FULLDEBUG, "Got %i replication requests from %s", replication_requests.Length(), remote_host.c_str());
	
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
		
		std::string dest = GetCacheDir(cache_name, err);
		
		// Clean up the cache ad, and put it in the log
		request_ptr->Delete(ATTR_CACHE_ORIGINATOR);
		request_ptr->Delete(ATTR_CACHE_STATE);
		
		// Put it in the log
		{
			TransactionSentry sentry(m_log);
			m_log->AppendAd(cache_name, *request_ptr, "*", "*");
		}
		
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
		ft->setMaxDownloadBytes(cache_size+4);
		

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
