
#include "condor_common.h"

#include "compat_classad.h"
#include "condor_version.h"
#include "condor_attributes.h"
#include "file_transfer.h"
#include "directory.h"
#include <iostream>
#include <fstream>


#include "dc_cacheflow_manager.h"

DCCacheflowManager::DCCacheflowManager(const char * name, const char *pool)
: Daemon( DT_CACHEFLOW_MANAGER, name, pool )
{}

DCCacheflowManager::DCCacheflowManager(const ClassAd* ad, const char* pool)
: Daemon(ad, DT_CACHEFLOW_MANAGER, pool )
{}

int DCCacheflowManager::pingCacheflowManager(std::string &cacheName)
{
	dprintf(D_FULLDEBUG, "FULLDEBUG: In DCCacheflowManager::pingCacheflowManager!");//##
	return 0;
}

int DCCacheflowManager::getStoragePolicy(compat_classad::ClassAd& jobAd, compat_classad::ClassAd& responseAd, CondorError& err)
{
	dprintf(D_FULLDEBUG, "FULLDEBUG: In DCCacheflowManager::getStoragePolicy!");//##
	if (!_addr && !locate())
	{
		err.push("CACHEFLOW_MANAGER", 2, error() && error()[0] ? error() : "Failed to locate remote cacheflow_manager");
		return 2;
	}

	ReliSock *rsock = (ReliSock *)startCommand(
			CACHEFLOW_MANAGER_GET_STORAGE_POLICY, Stream::reli_sock, 20 );

	if (!rsock)
	{
		err.push("CACHEFLOW_MANAGER", 1, "Failed to start command to remote cacheflow_manager");
		return 1;
	}

	std::string version = CondorVersion();
	jobAd.InsertAttr("CondorVersion", version);

	if (!putClassAd(rsock, jobAd) || !rsock->end_of_message()) {
		delete rsock;
		return 1;
	}

	responseAd.Clear();

	// Now get all the replies.
	rsock->decode();

	if (!getClassAd(rsock, responseAd) || !rsock->end_of_message())
	{
		err.push("CACHED", 2, "Request to remote cached closed before completing protocol.");
		delete rsock;
		return 2;
	}

	delete rsock;
	return 0;
}
