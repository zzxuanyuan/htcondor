#include "JobSpoolDir.h"
#include "condor_config.h"
#include <stdio.h>

int main()
{
	config(); // Initialize param();
	JobSpoolDir s;
	bool b = s.Initialize(123,4,5,true);
	if( ! b ) {
		printf("Failed to initialize\n");
		return 1;
	}
	//s.SetCmd("/example/executable");

	printf("%s\n",s.SandboxPath(true).GetCStr());
	printf("%s\n",s.TransferPath(true).GetCStr());
	sleep(10);
	s.DestroyProcessDirectory();
	s.DestroyClusterDirectory();

	return 0;
}
