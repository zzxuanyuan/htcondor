#include "JobSpoolDir.h"
#include "condor_config.h"
#include <stdio.h>

int main()
{
	config(); // Initialize param();

	//Initialize dprintf logging (reroute to stderr)
	Termlog = 1;
	dprintf_config("TOOL", fileno(stderr));

	JobSpoolDir s;
	bool b = s.Initialize(123,4,5,true);
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
	s.DestroyClusterDirectory();

	return 0;
}
