#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "subsystem_info.h"
#include "storage_optimizer_server.h"

StorageOptimizerServer *storage_optimizer_server = NULL;

//-------------------------------------------------------------

static void CleanUp()
{
	delete storage_optimizer_server;
	storage_optimizer_server = NULL;
}

//-------------------------------------------------------------

void main_init(int /* argc */, char * /* argv */ [])
{
	dprintf(D_ALWAYS, "main_init() called\n");
	storage_optimizer_server = new StorageOptimizerServer();
	storage_optimizer_server->InitAndReconfig();
}

//-------------------------------------------------------------

void main_config()
{
	dprintf(D_ALWAYS, "main_config() called\n");
	storage_optimizer_server->InitAndReconfig();
}

//-------------------------------------------------------------

void main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	CleanUp();
	DC_Exit(0);
}

//-------------------------------------------------------------

void main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	CleanUp();
	DC_Exit(0);
}

//-------------------------------------------------------------

int main( int argc, char **argv )
{
	set_mySubSystem("STORAGE_OPTIMIZER", SUBSYSTEM_TYPE_STORAGE_OPTIMIZER );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
