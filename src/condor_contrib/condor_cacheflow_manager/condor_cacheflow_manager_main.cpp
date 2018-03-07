#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "cacheflow_manager_server.h"

CacheflowManagerServer *cacheflow_manager_server;

//-------------------------------------------------------------

static void CleanUp()
{
	delete cacheflow_manager_server;
	cacheflow_manager_server = NULL;
}

//-------------------------------------------------------------

void main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_ALWAYS, "CacheflowManager main_init() called\n");
	cacheflow_manager_server = new CacheflowManagerServer();
	cacheflow_manager_server->InitAndReconfig();
}

//-------------------------------------------------------------

void main_config()
{
	dprintf(D_ALWAYS, "CacheflowManager main_config() called\n");
	cacheflow_manager_server->InitAndReconfig();
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "CacheflowManager main_shutdown_fast() called\n");
	CleanUp();
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "CacheflowManager main_shutdown_graceful() called\n");
	CleanUp();
	DC_Exit(0);
}

//-------------------------------------------------------------

int main( int argc, char **argv )
{
	set_mySubSystem("CACHEFLOW_MANAGER", SUBSYSTEM_TYPE_CACHEFLOW_MANAGER );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
