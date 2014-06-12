
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
	std::string cacheName = "/tester";
	time_t expiry = time(NULL) + 86400;
	int rc = client.createCacheDir(cacheName, expiry, err);
	fprintf(stderr, "Return code from createCacheDir: %d\nError contents: %s\n", rc, err.getFullText().c_str());
	if (rc) {
		return 1;
	}

	std::list<std::string> files;
	files.push_front("/etc/hosts");
	rc = client.uploadFiles(cacheName, files, err);
	fprintf(stderr, "Return code from uploadFiles: %d\nError contents: %s\n", rc, err.getFullText().c_str());
	if (rc) {
		return 1;
	}


	char destination[PATH_MAX];
	getcwd(destination, PATH_MAX);
	rc = client.downloadFiles(cacheName, destination, err);
	fprintf(stderr, "Return code from downloadFiles: %d\nError contents: %s\n", rc, err.getFullText().c_str());
	if (rc) {
		return 1;
	}

	return 0;
}
