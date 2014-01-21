
#include "condor_common.h"
#include "condor_config.h"
#include "condor_distribution.h"

#include "time.h"

#include "dc_cached.h"

int
main(int argc, char * argv[])
{
	myDistro->Init( argc, argv );
	config();
	dprintf_set_tool_debug("TOOL", 0);

	DCCached client;
	CondorError err;
	int rc = client.createCacheDir("/tester", time(NULL)+86400, err);
	fprintf(stderr, "Return code from createCacheDir: %d\nError contents: %s\n", rc, err.getFullText().c_str());
	return 0;
}

