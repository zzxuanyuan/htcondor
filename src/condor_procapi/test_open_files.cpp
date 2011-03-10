#include "condor_common.h"
#include "condor_debug.h"
#include "open_files_in_pid.h"

using namespace std;

int main(int argc, char *argv[])
{
	pid_t pid;
	set<string> files;
	set<string>::iterator it;
	int num = 10;
	FILE* fd[num];
	char path[4096];
	int i;

	for (i = 0; i < num; i++) {
		sprintf(path, "/tmp/a_%d", i);
		fd[i] = fopen(path, "w");
	}

	pid = getpid();

	if (argc == 2) {
		pid = atoi(argv[1]);
	}

	files = open_files_in_pid(pid);

	for(it = files.begin(); it != files.end(); it++) {
		printf("Found open regular file: %s\n", (*it).c_str());
	}

	for (i = 0; i < num; i++) {
		fclose(fd[i]);
	}

	return 0;
}
