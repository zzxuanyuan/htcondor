#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "cached_server.h"
#include "compat_classad.h"

#include <sqlite3.h>

#define SCHEMA_VERSION 1

const int CachedServer::m_schema_version(SCHEMA_VERSION);

CachedServer::CachedServer():
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
			(CommandHandlercpp)&CachedServer::UploadFiles,
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
}


int
CachedServer::InitializeDB()
{
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
	return PutErrorAd(sock, 2, "CreateCacheDir", "Method not implemented");
}

int CachedServer::UploadFiles(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
}

int CachedServer::DownloadFiles(int /*cmd*/, Stream * /*sock*/)
{
	return 0;
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



