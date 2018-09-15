
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

int DCStorageOptimizer::listStorageOptimizers(const std::string& requirements, std::list<compat_classad::ClassAd>& result_list, CondorError& err) {

	if (!_addr && !locate())
	{
		err.push("STORAGE_OPTIMIZER", 2, error() && error()[0] ? error() : "Failed to locate remote storage optimizer");
		return 2;
	}
	
	ReliSock *rsock = (ReliSock *)startCommand(
	STORAGE_OPTIMIZER_LIST_STORAGE_OPTIMIZERS, Stream::reli_sock, 20 );
	
	if (!rsock)
	{
		err.push("STORAGE_OPTIMIZER", 1, "Failed to start command to remote storage optimizer");
		return 1;
	}
	
	compat_classad::ClassAd request_ad;
	std::string version = CondorVersion();
	request_ad.InsertAttr("CondorVersion", version);
	
	if (!requirements.empty()) {
		request_ad.InsertAttr(ATTR_REQUIREMENTS, requirements);
	} else {
		request_ad.InsertAttr(ATTR_REQUIREMENTS, "TRUE");
	}
	
	
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}
	
	request_ad.Clear();
	
	// Now get all the replies.
	rsock->decode();
	
	while(true) {
		if (!getClassAd(rsock, request_ad) || !rsock->end_of_message())
		{
			err.push("STORAGE_OPTIMIZER", 2, "Request to remote storage optimizer closed before completing protocol.");
			delete rsock;
			return 2;
		}
		
		int is_end = 0;

		if(request_ad.LookupBool("FinalAd", is_end)) {
			if (is_end)
				break;
		}
		
		result_list.push_back(request_ad);
		fPrintAd (stdout, request_ad);
	}
	
	delete rsock;
	return 0;
}
