#include <iostream.h>
#include <stdio.h>

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_io.h"
#include "sched.h"

// about self
static char *_FileName_ = __FILE__;

// variables from the config file
extern char *Log;
extern char *CollectorLog;
extern char *CondorAdministrator;
extern char *CondorDevelopers;
extern int   MaxCollectorLog;
extern int   ClientTimeout; 
extern int   QueryTimeout;
extern int   MachineUpdateInterval;


void
initializeParams()
{
    char *tmp;

	Log = param ("LOG");
	if (Log == NULL) 
	{
		EXCEPT ("Variable 'LOG' not found in config file.");
    }

    CollectorLog = param ("COLLECTOR_LOG");
	if (CollectorLog == NULL) 
	{
		EXCEPT ("Variable 'COLLECTOR_LOG' not found in config file.");
	}
		
    MaxCollectorLog = (tmp = param ("MAX_COLLECTOR_LOG")) ? atoi (tmp) : 64000;

    ClientTimeout = (tmp = param ("CLIENT_TIMEOUT")) ? atoi (tmp) : 30;

	QueryTimeout = (tmp = param ("QUERY_TIMEOUT")) ? atoi (tmp) : 60;

    MachineUpdateInterval = (tmp = param ("MACHINE_UPDATE_INTERVAL")) ?
		atoi (tmp) : 300;
    
    CondorDevelopers = param ("CONDOR_DEVELOPERS");
    CondorAdministrator = param ("CONDOR_ADMIN");
}




