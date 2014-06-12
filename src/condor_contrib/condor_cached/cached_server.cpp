#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "cached_server.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>
#include "directory.h"


#define SCHEMA_VERSION 1

const int CachedServer::m_schema_version(SCHEMA_VERSION);
const char *CachedServer::m_header_key("CACHE_ID");

CachedServer::CachedServer():
	m_log(NULL),
	m_db(NULL),
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
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_UPLOAD_FILES,
			"CACHED_UPLOAD_FILES",
			(CommandHandlercpp)&CachedServer::UploadToServer,
			"CachedServer::UploadFiles",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_DOWNLOAD_FILES,
			"CACHED_DOWNLOAD_FILES",
			(CommandHandlercpp)&CachedServer::DownloadFiles,
			"CachedServer::DownloadFiles",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_REMOVE_CACHE_DIR,
			"CACHED_REMOVE_CACHE_DIR",
			(CommandHandlercpp)&CachedServer::RemoveCacheDir,
			"CachedServer::RemoveCacheDir",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_UPDATE_LEASE,
			"CACHED_UPDATE_LEASE",
			(CommandHandlercpp)&CachedServer::UpdateLease,
			"CachedServer::UpdateLease",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_LIST_CACHE_DIRS,
			"CACHED_LIST_CACHE_DIRS",
			(CommandHandlercpp)&CachedServer::ListCacheDirs,
			"CachedServer::ListCacheDirs",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_LIST_FILES_BY_PATH,
			"CACHED_LIST_FILES_BY_PATH",
			(CommandHandlercpp)&CachedServer::ListFilesByPath,
			"CachedServer::ListFilesByPath",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_CHECK_CONSISTENCY,
			"CACHED_CHECK_CONSISTENCY",
			(CommandHandlercpp)&CachedServer::CheckConsistency,
			"CachedServer::CheckConsistency",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_SET_REPLICATION_POLICY,
			"CACHED_SET_REPLICATION_POLICY",
			(CommandHandlercpp)&CachedServer::SetReplicationPolicy,
			"CachedServer::SetReplicationPolicy",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_GET_REPLICATION_POLICY,
			"CACHED_GET_REPLICATION_POLICY",
			(CommandHandlercpp)&CachedServer::GetReplicationPolicy,
			"CachedServer::GetReplicationPolicy",
			this,
			WRITE );
		ASSERT( rc >= 0 );

		rc = daemonCore->Register_Command(
			CACHED_CREATE_REPLICA,
			"CACHED_CREATE_REPLICA",
			(CommandHandlercpp)&CachedServer::CreateReplica,
			"CachedServer::CreateReplica",
			this,
			WRITE );
		ASSERT( rc >= 0 );
	}
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
	CondorError err;
	compat_classad::ClassAd* cache_ad;
	if (GetCacheAd(dirname.c_str(), cache_ad, err)) {
		// Cache ad exists, cannot recreate
		dprintf(D_ALWAYS | D_FAILURE, "Client requested to create cache %s, but it already exists\n", dirname.c_str());
		return PutErrorAd(sock, 1, "CreateCacheDir", "Cache already exists.  Cannot recreate.");

	}

		// Insert ad into cache
	long long cache_id = m_id++;
	std::string cache_id_str = boost::lexical_cast<std::string>(cache_id);
	boost::replace_all(dirname, "$(UNIQUE_ID)", cache_id_str);

  // Create the directory
	// 1. Get the caching directory from the condor configuration
	std::string caching_dir = GetCacheDir(dirname, err);

	// 3. Create the caching directory
	if ( !mkdir_and_parents_if_needed(caching_dir.c_str(), S_IRWXU, PRIV_CONDOR) ) {
		dprintf( D_FAILURE|D_ALWAYS,
						"couldn't create caching dir %s: %s\n",
						caching_dir.c_str(),
						strerror(errno) );
	} else {
		dprintf(D_FULLDEBUG, "Creating caching directory for %s at %s\n",
						dirname.c_str(),
						caching_dir.c_str() );

	}


	classad::ClassAd log_ad;
	log_ad.InsertAttr(ATTR_CACHE_NAME, dirname);
	log_ad.InsertAttr(ATTR_CACHE_ID, cache_id);
	log_ad.InsertAttr(ATTR_LEASE_EXPIRATION, lease_expiry);
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
		classad_shared_ptr<FileTransfer> ft(ft_ptr);
		dprintf(D_FULLDEBUG, "Got a handle\n");
		m_server.SetCacheUploadStatus(m_cacheName, fi.success);
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
	CondorError err;
	compat_classad::ClassAd *cache_ad;
	if (!GetCacheAd(dirname, cache_ad, err))
	{
		dprintf(D_ALWAYS, "Unable to find dirname = %s in log\n", dirname.c_str());
		return PutErrorAd(sock, 1, "UploadFiles", err.getFullText());
	}

	// Check if the current dir is in a committed state
	if (GetUploadStatus(dirname) > 0) {
		dprintf(D_ALWAYS, "Cache %s is already commited, cannot upload files.\n", dirname.c_str());
		return PutErrorAd(sock, 1, "UploadFiles", "Cache is already committed.  Cannot upload more files.");
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
	FileTransfer ft;
	cache_ad->InsertAttr(ATTR_JOB_IWD, cachingDir.c_str());
	cache_ad->InsertAttr(ATTR_OUTPUT_DESTINATION, cachingDir);

	// TODO: Enable file ownership checks
	rc = ft.SimpleInit(cache_ad, false, true, static_cast<ReliSock*>(sock));
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed simple init\n");
	} else {
		dprintf(D_FULLDEBUG, "Successfully SimpleInit of filetransfer\n");
	}

	ft.setPeerVersion(version.c_str());
	UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	ft.RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);

	// TODO: Set to false for non-blocking.  Need to work on file transfer to
	// to incorporate initializing
	//
	rc = ft.DownloadFiles(true);
	if (!rc) {
		dprintf(D_ALWAYS | D_FAILURE, "Failed DownloadFiles\n");
	} else {
		dprintf(D_FULLDEBUG, "Successfully began downloading files\n");
		SetCacheUploadStatus(dirname.c_str(), true);
		GetUploadStatus(dirname.c_str());
	}
	return KEEP_STREAM;
}

int CachedServer::DownloadFiles(int /*cmd*/, Stream * sock)
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

	compat_classad::ClassAd response_ad;
	std::string my_version = CondorVersion();
	response_ad.InsertAttr("CondorVersion", my_version);
	response_ad.InsertAttr(ATTR_ERROR_CODE, 0);

	if (!putClassAd(sock, response_ad) || !sock->end_of_message())
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
	FileTransfer ft;
	ft.SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(sock));
	ft.setPeerVersion(version.c_str());
	UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	ft.RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
	ft.UploadFiles(true);
	return KEEP_STREAM;

}

int CachedServer::RemoveCacheDir(int /*cmd*/, Stream * /*sock*/)
{
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

int CachedServer::SetReplicationPolicy(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
}

int CachedServer::GetReplicationPolicy(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
}

int CachedServer::CreateReplica(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
}

/**
 *	Return the classad for the cache dirname
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


int CachedServer::SetCacheUploadStatus(const std::string &dirname, bool success)
{
	TransactionSentry sentry(m_log);
	if (!m_log->AdExistsInTableOrTransaction(dirname.c_str())) { return 0; }

		// TODO: Convert this to a real state.
	LogSetAttribute *attr = new LogSetAttribute(dirname.c_str(), "CacheState", boost::lexical_cast<std::string>(success).c_str());
	m_log->AppendLog(attr);
	return 0;
}

/*
 * Get the current upload status
 * returns:
 * 	<0 -	Failed to find dirname
 *   0	-	Cache dirname is uncommitted
 *	 >0 -	Cache dirname is already committed
 */
int CachedServer::GetUploadStatus(const std::string &dirname) {
	TransactionSentry sentry(m_log);

	// Check if the cache directory even exists
	if (!m_log->AdExistsInTableOrTransaction(dirname.c_str())) { return 0; }

	compat_classad::ClassAd *cache_ad;
	CondorError errorad;
	// Check the cache status
	if (GetCacheAd(dirname, cache_ad, errorad) == 0 )
		return -1;

	dprintf(D_FULLDEBUG, "Caching classad:");
	compat_classad::dPrintAd(D_FULLDEBUG, *cache_ad);

	int committed = 0;
	if (!cache_ad->EvalBool("CacheState", NULL, committed)) {
		// CacheState doesn't exist in the ad, so it's not committed
		return 0;
	} else {
		return committed;
	}


}

std::string CachedServer::GetCacheDir(const std::string &dirname, CondorError &err) {

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
