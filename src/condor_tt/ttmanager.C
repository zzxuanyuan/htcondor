/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "get_daemon_name.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "ttmanager.h"
#include "file_sql.h"

char logParamList[][30] = {"NEGOTIATOR_SQLLOG", "SCHEDD_SQLLOG", "SHADOW_SQLLOG", "STARTER_SQLLOG", "STARTD_SQLLOG", "SUBMIT_SQLLOG", ""};

//! constructor
TTManager::TTManager()
{
		//nothing here...its all done in config()
	DBObj = (ODBC  *) 0;
}

//! destructor
TTManager::~TTManager()
{
		// release Objects
	numLogs = 0;
	if (DBObj)
		delete DBObj;
}

void
TTManager::config(bool reconfig) 
{
	char *tmp;
	int   i = 0;

	numLogs = 0;

	pollingTimeId = -1;

		/* check all possible log parameters */
	while (logParamList[i][0] != '\0') {
		tmp = param(logParamList[i]);
		if (tmp) {
			strncpy(sqlLogList[numLogs], tmp, MAXLOGPATHLEN-5);
			sprintf(sqlLogCopyList[numLogs], "%s.copy", sqlLogList[numLogs]);
			numLogs++;
			free(tmp);
		}		
		i++;
	}

		/* add the default log file in case no log file is specified in config */
	tmp = param("LOG");
	if (tmp) {
		sprintf(sqlLogList[numLogs], "%s/sql.log", tmp);
		sprintf(sqlLogCopyList[numLogs], "%s.copy", sqlLogList[numLogs]);
		free(tmp);
	} else {
		sprintf(sqlLogList[numLogs], "sql.log");
		sprintf(sqlLogCopyList[numLogs], "%s.copy", sqlLogList[numLogs]);
	}
	numLogs++;
		
		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	char *pollingPeriod_str = param("TT_POLLING_PERIOD");
	if(pollingPeriod_str) {
		pollingPeriod = atoi(pollingPeriod_str);
		free(pollingPeriod_str);
	}
	else pollingPeriod = 10;
  
	dprintf(D_ALWAYS, "Using Polling Period = %d\n", pollingPeriod);
	dprintf(D_ALWAYS, "Using logs ");
	for(int i=0; i < numLogs; i++)
		dprintf(D_ALWAYS, "%s ", sqlLogList[i]);
	dprintf(D_ALWAYS, "\n");

		// this function is also called when condor_reconfig is issued
		// and so we dont want to recreate all essential objects
	if(!reconfig) {
		numTimesPolled = 0; 		
	}

	DBObj = createConnection();  
}

//! register all timer handlers
void
TTManager::registerTimers()
{
		// clear previous timers
	if (pollingTimeId >= 0)
		daemonCore->Cancel_Timer(pollingTimeId);

		// register timer handlers
	pollingTimeId = daemonCore->Register_Timer(0, 
											   pollingPeriod,
											   (Eventcpp)&TTManager::pollingTime, 
											   "pollingTime", this);
}

//! timer handler for each polling event
void
TTManager::pollingTime()
{
	dprintf(D_ALWAYS, "******** Start of Polling ********\n");

		/*
		  instead of exiting on error, we simply return 
		  this means that tt will not usually exit on loss of 
		  database connectivity, and that it will keep polling
		  and trying to connect until database is back up again 
		  and then resume execution 
		*/
	if (maintain() == 0) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Probing <<<<<<<<\n");
		return;
			//DC_Exit(1);
	}

	dprintf(D_ALWAYS, "********* End of Probing *********\n");
	numTimesPolled++;
}

int
TTManager::maintain() 
{
	FILESQL *filesqlobj;
//	char buf[1024];
	char *buf;

	int  buflength=0;
	int  retval;
	bool firststmt = true;

		/* copy files */	
	for(int i=0; i < numLogs; i++) {
		filesqlobj = new FILESQL(sqlLogList[i], O_RDWR);

		retval = filesqlobj->file_open();
		if (retval < 0) {
			goto ERROREXIT;
		}

		filesqlobj->file_lock();
		
		if (retval < 0) {
			goto ERROREXIT;
		}		
		
		if (this->append(sqlLogCopyList[i], sqlLogList[i]) < 0) {
			goto ERROREXIT;
		}

		if((retval = filesqlobj->file_truncate()) < 0) {
			goto ERROREXIT;
		}

		if((retval = filesqlobj->file_unlock()) < 0) {
			goto ERROREXIT;
		}
		delete filesqlobj;
	}

	for(int i=0; i < numLogs; i++) {
		buf =(char *) malloc(2048 * sizeof(char));
		filesqlobj = new FILESQL(sqlLogCopyList[i], O_RDWR);

		retval = filesqlobj->file_open();
		
		if (retval < 0) {
			goto ERROREXIT;
		}
			
		filesqlobj->file_lock();
		
		if (retval < 0) {
			goto ERROREXIT;
		}

		firststmt = true;
		while(filesqlobj->file_readline(buf) != EOF) {
			buflength = strlen(buf);
			if(buflength == 0) {
				break;
			}
			if(buf[buflength-1] != ';') {
				buf[buflength] = ';';
				buf[buflength+1] = '\0';
			}

			if(firststmt) {
				if(DBObj->odbc_beginxtstmt("BEGIN") < 0) {
					dprintf(D_ALWAYS, "Begin transaction --- Error\n");
				}
				firststmt = false;
			}
			if (DBObj->odbc_sqlstmt(buf) < 0) {
				dprintf(D_ALWAYS, "Inserting Statement --- Error\n");
				dprintf(D_ALWAYS, "sql = %s\n", buf);
				filesqlobj->file_unlock();
				delete filesqlobj;
				free(buf);		
				return -1; // return a error code -1
			}
		}

		if((retval = filesqlobj->file_truncate()) < 0) {
			goto ERROREXIT;
		}

		if((retval = filesqlobj->file_unlock()) < 0) {
			goto ERROREXIT;
		}

		if(!firststmt) {
			if(DBObj->odbc_endxtstmt("END") < 0) {
				dprintf(D_ALWAYS, "End transaction --- Error\n");
			}
		}

		if(filesqlobj) {
			delete filesqlobj;
			free(buf);
			filesqlobj = NULL;
		}
	}

	return 1;

 ERROREXIT:
	if(filesqlobj) {
		delete filesqlobj;
	}
	free(buf);
	return retval;
}


int TTManager::append(char *destF, char *srcF) 
{	
	int dest, src;
	char buffer[4096];
	int rv;

	dest = open (destF, O_WRONLY|O_CREAT|O_APPEND, 0644);
	src = open (srcF, O_RDONLY);

	rv = read(src, buffer, 4096);

	while(rv > 0) {
		if (write(dest, buffer, rv) < 0) {
			return -1;
		}
		
		rv = read(src, buffer, 4096);
	}

	close (dest);
	close (src);

	return 0;
}
