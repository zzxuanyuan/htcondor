/* This file implements the ability for a process to find the currently open
	files by another (or self) process */

#include "condor_common.h"
#include "condor_debug.h"
#include "open_files_in_pid.h"

using namespace std;

set<string> open_files_in_pid(pid_t pid)
{
	set<string> open_file_set;

#if defined(LINUX)

	string tmp;
	DIR *dir = NULL;
	dirent *dent = NULL;
	int path_len = sizeof(dent->d_name) + 1;
	char path[path_len]; // *sigh*
	char f[path_len];
	char fd_loc[path_len];
	char real_path[PATH_MAX];
	int ret;
	struct stat buf;

	ASSERT(path_len > 0);

	// Dig around in the proc file system looking for open files for the
	// specified pid. This is Linux only, for now. I can't use Condor's
	// Directory object since it would require more of the c++ util than
	// the condor_procd is supposed to be using.

	sprintf(fd_loc, "/proc/%lu/fd", (long unsigned) pid);
	dir = opendir(fd_loc);
	if (dir == NULL) {
		// If there isn't an fd directory to open, then I suppose there
		// aren't any open file descriptors for this pid, eh?
		return open_file_set;
	}

	// If a file is open multiple times, that's fine, we isnert it multiple
	// times into the STL set, but it'll only appear once in there due to 
	// STL set semantics.
	while((dent = readdir(dir))) {
		// skip the usual suspects
		if (strcmp(dent->d_name, ".") == MATCH ||
			strcmp(dent->d_name, "..")  == MATCH) 
		{
			continue;
		}

		// convert the dent path to a fully qualified path since we know from
		// where we got it.
		strcpy(path, fd_loc);
		strcat(path, "/");
		strcat(path, dent->d_name);

		// read the symlink out of the fully qualified dent path
		memset(f, 0, path_len);
		ret = readlink(path, f, path_len);
		if (ret < 0) {
			continue;
		}
		f[path_len - 1] = '\0';

		// convert the de-symlinked path to a fully qualified path
		memset(real_path, 0, PATH_MAX);
		if (realpath(f, real_path) == NULL) {
			continue;
		}

		// Now we'll check to see what kind of file it is...
		if (stat(real_path, &buf) < 0) {
			// Maybe the pid closed it in the time I was looking, no
			// harm done, just restart the checking on the next file.
			continue;
		}

		// Only add the real_path to the open_files_set if it is a regular
		// file.
		if (S_ISREG(buf.st_mode)) {
			// Finally, add the regular file to the open_file_set!
			open_file_set.insert(tmp = real_path);
		}
	}

	if (closedir(dir) < 0) {
		dprintf(D_ALWAYS, 
			"open_files_in_pid(): ERROR: Programmer error with DIR "
			"structure: %d(%s).\n", errno, strerror(errno));
	}

#endif

	return open_file_set;
}

