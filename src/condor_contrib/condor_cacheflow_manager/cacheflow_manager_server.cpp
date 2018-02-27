#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "classad_hashtable.h"
#include "get_daemon_name.h"
#include "ipv6_hostname.h"
#include <list>
#include <map>
#include "basename.h"

#include "cacheflow_manager_server.h"
#include "dc_cacheflow_manager.h"

#include <sstream>

CacheflowManagerServer::CacheflowManagerServer()
{
	update_collector_tid = -1;
	reaper_tid = -1;
}

CacheflowManagerServer::~CacheflowManagerServer()
{
}

void CacheflowManagerServer::Init()
{
	update_collector_tid = daemonCore->Register_Timer (
			60,
			(TimerHandlercpp) &CacheflowManagerServer::UpdateCollector,
			"Update Collector",
			(Service*)this );

	reaper_tid = daemonCore->Register_Reaper("dummy_reaper",
			(ReaperHandler) &CacheflowManagerServer::dummy_reaper,
			"dummy_reaper",NULL);

	ASSERT( reaper_tid >= 0 );
}

void CacheflowManagerServer::UpdateCollector() {
	dprintf(D_FULLDEBUG, "enter CacheflowManager::UpdateCollector()\n");

	// Update the available caches on this server
	compat_classad::ClassAd published_classad = GenerateClassAd();
	dPrintAd(D_FULLDEBUG, published_classad);
	int rc = daemonCore->sendUpdates(UPDATE_AD_GENERIC, &published_classad);
	if (rc == 0) {
		dprintf(D_FAILURE | D_ALWAYS, "Failed to send commands to collectors, rc = %i\n", rc);
	} else {
		dprintf(D_FULLDEBUG, "Sent updates to %i collectors\n", rc);
	}

	dprintf( D_FULLDEBUG, "exit CacheflowManager::UpdateCollector\n" );
}

/**
 * Generate the daemon's classad, with all the information
 */

compat_classad::ClassAd CacheflowManagerServer::GenerateClassAd() {

	// Update the available caches on this server
	compat_classad::ClassAd published_classad;

	daemonCore->publish(&published_classad);

	published_classad.InsertAttr("CacheflowManager", true);

	return published_classad;
}

int CacheflowManagerServer::dummy_reaper(Service *, int pid, int) {
	dprintf(D_ALWAYS,"dummy-reaper: pid %d exited; ignored\n",pid);
	return TRUE;
}


