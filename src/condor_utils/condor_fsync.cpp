#include "condor_common.h"
#include "condor_fsync.h"

// Another way to approach this would be to test for fdatasync's existence
// in CMake.  However, this is the appropriate POSIX way to do the test;
// using ifndef WIN32 here mimics write_user_log.h.
#ifndef WIN32
#include <unistd.h>
#endif

bool condor_fsync_on = true;

int condor_fsync(int fd, const char* /*path*/)
{
	if(!condor_fsync_on)
		return 0;

	return fsync(fd);
}

int condor_fdatasync(int fd, const char* /*path*/)
{
	if (!condor_fsync_on)
	{
		return 0;
	}

#ifdef _POSIX_SYNCHRONIZED_IO
	return fdatasync(fd);
#else
	return fsync(fd);
#endif
}

