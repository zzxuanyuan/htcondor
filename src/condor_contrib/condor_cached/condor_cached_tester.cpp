
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
	if (rc) {
		fprintf(stderr, "FAIL: Return code from createCacheDir: %d\nError contents: %s\n", rc, err.getFullText().c_str());
	} else {
		fprintf(stdout, "SUCCESS: Initial creation of cache successful\n");
	}

	std::list<std::string> files;
	files.push_front("/etc/hosts");
	rc = client.uploadFiles(cacheName, files, err);
	if (rc) {
		fprintf(stderr, "FAIL: Return code from uploadFiles: %d\nError contents: %s\n", rc, err.getFullText().c_str());
		return 1;
	} else {
		fprintf(stdout, "SUCCESS: Initial upload of file successful\n");
	}

	// Attempt to upload the files again, should be an error:
	rc = client.uploadFiles(cacheName, files, err);
	if (rc) {
		fprintf(stderr, "SUCCESS: Return code from uploadFiles: %d\nError contents: %s\n", rc, err.getFullText().c_str());
	} else {
		fprintf(stdout, "FAIL: Second upload of file successful, it shouldn't be...\n");
		return 1;
	}


	char destination[PATH_MAX];
	getcwd(destination, PATH_MAX);
	rc = client.downloadFiles(cacheName, destination, err);
	if (rc) {
		fprintf(stderr, "FAIL: Return code from downloadFiles: %d\nError contents: %s\n", rc, err.getFullText().c_str());
		return 1;
	} else {
		fprintf(stdout, "SUCCESS: Download of files successful\n");
	}
	
	// Remove the cache directory
	rc = client.removeCacheDir(cacheName, err);
	if (rc) {
		fprintf(stderr, "FAIL: Return code from removeCacheDir: %d\nError contents: %s\n", rc, err.getFullText().c_str());
		return 1;
	} else {
		fprintf(stdout, "SUCCESS: Removal of Cache\n");
	}

	return 0;
}
