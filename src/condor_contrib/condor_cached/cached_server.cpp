#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "cached_server.h"

#include <sqlite3.h>

CachedServer::CachedServer():
  m_db(NULL),
  m_registered_handlers(false)
{
  
  if ( !m_registered_handlers ) {
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

CachedServer::~CachedServer() {
  
  
  
  
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
		EXCEPT("Failed to open cached database %s: %s\n", m_db_fname.c_str(), sqlite3_errmsg(m_db));
		sqlite3_close(m_db);
	}
}


int CachedServer::CreateCacheDir(int cmd, Stream *sock) {
  
}

int CachedServer::UploadFiles(int cmd, Stream *sock) {
  
}

int CachedServer::DownloadFiles(int cmd, Stream *sock) {
  
}

int CachedServer::RemoveCacheDir(int cmd, Stream *sock) {
  
}

int CachedServer::UpdateLease(int cmd, Stream *sock) {
  
}

int CachedServer::ListCacheDirs(int cmd, Stream *sock) {
  
}

int CachedServer::ListFilesByPath(int cmd, Stream *sock) {
  
}

int CachedServer::CheckConsistency(int cmd, Stream *sock) {
  
}

int CachedServer::SetReplicationPolicy(int cmd, Stream *sock) {
  
}

int CachedServer::GetReplicationPolicy(int cmd, Stream *sock) {
  
}

int CachedServer::CreateReplica(int cmd, Stream *sock) {
  
}



