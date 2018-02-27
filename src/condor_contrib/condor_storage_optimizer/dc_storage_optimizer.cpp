
#include "condor_common.h"

#include "compat_classad.h"
#include "condor_version.h"
#include "condor_attributes.h"
#include "file_transfer.h"
#include "directory.h"
#include <iostream>
#include <fstream>


#include "dc_storage_optimizer.h"

DCStorageOptimizer::DCStorageOptimizer(const char * name, const char *pool)
	: Daemon( DT_STORAGE_OPTIMIZER, name, pool )
{}
	
DCStorageOptimizer::DCStorageOptimizer(const ClassAd* ad, const char* pool)
	: Daemon(ad, DT_GENERIC, pool )
{}

int
DCStorageOptimizer::pingStorageOptimizer(std::string &cacheName)
{
	dprintf(D_FULLDEBUG, "FULLDEBUG: In DCStorageOptimizer::pingStorageOptimizer!");//##
	return 0;
}
