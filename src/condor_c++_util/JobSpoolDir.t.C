#include "JobSpoolDir.h"
#include "condor_config.h"
#include <stdio.h>

int main()
{
	config(); // Initialize param();

	//Initialize dprintf logging (reroute to stderr)
	Termlog = 1;
	dprintf_config("TOOL", fileno(stderr));

	int jobnum = 123;

	{
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
		sleep(10);
		s.DestroyProcessDirectory();
	}
	sleep(10);
	{
		DestroyClusterDirectory(jobnum);
	}

	return 0;
}
