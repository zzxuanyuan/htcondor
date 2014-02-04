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

#include <sqlite3.h>

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
/*
	if (m_db != NULL)
	{
		sqlite3_close(m_db);
	}
	if (sqlite3_open(m_db_fname.c_str(), &m_db))
	{
		dprintf(D_ALWAYS, "Failed to open cached database %s: %s\n", m_db_fname.c_str(), sqlite3_errmsg(m_db));
		sqlite3_close(m_db);
		EXCEPT("Failed to open cached database %s: %s\n", m_db_fname.c_str(), sqlite3_errmsg(m_db));
	}

	// Check DB schema version; if the DB is unusable or not of the same version, we will rewrite it.
	const std::string select_version = "select version from cached_version";
	sqlite3_stmt * version_statement;
	bool reinitialize = false;
	if (sqlite3_prepare_v2(m_db, select_version.c_str(), select_version.size()+1, &version_statement, NULL))
	{
		dprintf(D_ALWAYS, "Unable to prepare statement (%s) in sqlite: %s\n", select_version.c_str(), sqlite3_errmsg(m_db));
		reinitialize = true;
	}
	if (!reinitialize)
	{
		int rc;
		rc = sqlite3_step(version_statement);
		if (rc == SQLITE_ROW)
		{
			int db_schema_version = sqlite3_column_int(version_statement, 0);
			if (db_schema_version != m_schema_version)
			{
				dprintf(D_ALWAYS, "DB schema version %d does not match code version %d.\n", db_schema_version, m_schema_version);
				reinitialize = true;
			}
		}
		else
		{
			dprintf(D_FULLDEBUG, "Failure in reading version from DB; will re-initialize.  %s\n", sqlite3_errmsg(m_db));
			reinitialize = true;
		}
	}
	sqlite3_finalize(version_statement);

	if (reinitialize) { InitializeDB(); }
*/
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
/*
	dprintf(D_ALWAYS, "Re-initializing database.\n");
	if (sqlite3_exec(m_db, "DROP TABLE IF EXISTS cached_version", NULL, NULL, NULL))
	{
		dprintf(D_ALWAYS, "Failed to drop cached_version table: %s\n", sqlite3_errmsg(m_db));
		return 1;
	}
	if (sqlite3_exec(m_db, "CREATE TABLE cached_version (version int)", NULL, NULL, NULL))
	{
		dprintf(D_ALWAYS, "Failed to create cached_version table: %s\n", sqlite3_errmsg(m_db));
		return 1;
	}
	sqlite3_stmt * version_statement;
	const std::string version_statement_str = "INSERT INTO cached_version VALUES(?)";
	if (sqlite3_prepare_v2(m_db, version_statement_str.c_str(), version_statement_str.size()+1, &version_statement, NULL))
	{
		dprintf(D_ALWAYS, "Unable to prepare cached_version initialization statement: %s\n", sqlite3_errmsg(m_db));
		return 1;
	}
	if (sqlite3_bind_int(version_statement, 1, m_schema_version))
	{
		dprintf(D_ALWAYS, "Failed to bind version statement to %d: %s\n", m_schema_version, sqlite3_errmsg(m_db));
		sqlite3_finalize(version_statement);
		return 1;
	}
	if (sqlite3_step(version_statement) != SQLITE_DONE)
	{
		dprintf(D_ALWAYS, "Failed to insert current cached version: %s\n", sqlite3_errmsg(m_db));
		sqlite3_finalize(version_statement);
		return 1;
	}
	sqlite3_finalize(version_statement);
	return RebuildDB();
*/
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

		// Insert ad into cache
	long long cache_id = m_id++;
	std::string cache_id_str = boost::lexical_cast<std::string>(cache_id);
	boost::replace_all(dirname, "$(UNIQUE_ID)", cache_id_str);
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
                return PutErrorAd(sock, 1, "UploadFiles", "Request missing CondorVersion attribute");
        }
        if (!request_ad.EvaluateAttrString("CacheName", dirname))
        {
                return PutErrorAd(sock, 1, "UploadFiles", "Request missing CacheName attribute");
        }
	CondorError err;
	compat_classad::ClassAd *cache_ad;
	if (!GetCacheAd(dirname, cache_ad, err))
	{
		return PutErrorAd(sock, 1, "UploadFiles", err.getFullText());
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
	// From here on out, this is the file transfer server socket.
	FileTransfer ft;
	ft.SimpleInit(cache_ad, true, true, static_cast<ReliSock*>(sock));
	ft.setPeerVersion(version.c_str());
	UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	ft.RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
	ft.DownloadFiles(false);
	return KEEP_STREAM;
}

int CachedServer::DownloadFiles(int /*cmd*/, Stream * sock)
{
	compat_classad::ClassAd request_ad;
	if (!getClassAd(sock, request_ad) || !sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to read request for DownloadFiles.\n");
		return 1;
	}
	std::string dirname;
	std::string version;
	if (!request_ad.EvaluateAttrString("CondorVersion", version))
	{
		return PutErrorAd(sock, 1, "DownloadFiles", "Request missing CondorVersion attribute");
	}
	if (!request_ad.EvaluateAttrString("CacheName", dirname))
	{
		return PutErrorAd(sock, 1, "DownloadFiles", "Request missing CacheName attribute");
	}
	// TODO: Lookup ad in DB, 
	return PutErrorAd(sock, 2, "DownloadFiles", "Method not implemented");
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


int CachedServer::GetCacheAd(const std::string &dirname, compat_classad::ClassAd *&cache_ad, CondorError &err)
{
	if (m_log->table.lookup(dirname.c_str(), cache_ad) == -1)
	{
		err.pushf("CACHED", 3, "Cache ad %s not found", dirname.c_str());
		return 1;
	}
	return 0;
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

