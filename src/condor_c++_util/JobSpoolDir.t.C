#include "JobSpoolDir.h"
#include "condor_config.h"
#include <stdio.h>
#include <errno.h>

int main()
{
	config(); // Initialize param();

	//Initialize dprintf logging (reroute to stderr)
	Termlog = 1;
	dprintf_config("TOOL", fileno(stderr));

	int jobnum = 123;

	{
		printf("Testing staging executable over\n");
		// Test writing the executable
		printf("SendSpoolFileID: %s\n",SendSpoolFileOpaqueID(jobnum).GetCStr());
		MyString exe = ExecutablePathForWriting(jobnum);
		printf("Write EXE (simple): %s\n",exe.GetCStr());
		FILE * f = fopen(exe.GetCStr(), "w");
		if( ! f ) {
			printf("FAILED TO OPEN %d\n", errno);
			exit(1);
		}
		fprintf(f, "TEST");
		fclose(f);
		printf("Executable should be present\n");
		sleep(10);
	}

	{
		printf("Testing normal usage\n");
		JobSpoolDir s;
		bool b = s.Initialize(jobnum,4,5,true);
		if( ! b ) {
			printf("Failed to initialize\n");
			return 1;
		}
		//s.SetCmd("/example/executable");

		printf("Sandbox: %s\n",s.SandboxPath(true).GetCStr());
		printf("Transfer: %s\n",s.TransferPath(true).GetCStr());
		printf("Read  EXE: %s\n",s.ExecutablePathForReading().GetCStr());
		printf("Write EXE: %s\n",s.ExecutablePathForWriting().GetCStr());
		printf("Process directory should now exist\n");
		sleep(10);

		s.DestroyProcessDirectory();
		printf("Process directory should be gone\n");
		sleep(10);
	}
	{
		DestroyClusterDirectory(jobnum);
		printf("Everything should be gone\n");
	}

	return 0;
}
