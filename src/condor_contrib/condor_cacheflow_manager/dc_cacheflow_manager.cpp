
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
	: Daemon(ad, DT_GENERIC, pool )
{}

int
DCCacheflowManager::pingCacheflowManager(std::string &cacheName)
{
	dprintf(D_FULLDEBUG, "FULLDEBUG: In DCCacheflowManager::pingCacheflowManager!");//##
	return 0;
}
