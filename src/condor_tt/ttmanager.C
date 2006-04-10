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
#include "my_hostname.h"
#include "condor_attributes.h"
#include "condor_event.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include "condor_md.h"

#include "jobqueuedbmanager.h"
#include "ttmanager.h"
#include "file_sql.h"
#include "database.h"
#include "pgsqldatabase.h"
#include "misc_utils.h"

#define TIMELEN 60

char logParamList[][30] = {"NEGOTIATOR_SQLLOG", "SCHEDD_SQLLOG", 
						   "SHADOW_SQLLOG", "STARTER_SQLLOG", 
						   "STARTD_SQLLOG", "SUBMIT_SQLLOG", ""};

#define FILESIZELIMT 1900000000L
#define THROWFILE numLogs
#define EVENTTYPEMAX 50

#define TYPE_STRING    1
#define TYPE_NUMBER    2
#define TYPE_TIMESTAMP 3

static int attHashFunction (const MyString &str, int numBuckets);
static int isStaticMachineAttr(char *attName);
static int typeOf(char *attName);
static int file_checksum(char *filePathName, int fileSize, char *sum);
static QuillErrCode append(char *destF, char *srcF);

//! constructor
TTManager::TTManager()
{
		//nothing here...its all done in config()
	DBObj = (Database  *) 0;
}

//! destructor
TTManager::~TTManager()
{
		// release Objects
	numLogs = 0;
		// the object will be destroyed in the destructor of jqDBManager
	DBObj = (Database  *) 0;
}

void
TTManager::config(bool reconfig) 
{
	char *tmp;
	int   i = 0, j = 0;
	int   found = 0;

	numLogs = 0;

	pollingTimeId = -1;

		/* check all possible log parameters */
	while (logParamList[i][0] != '\0') {
		tmp = param(logParamList[i]);
		if (tmp) {
			
				// check if the new log file is already in the list
			for (j = 0 ; j < numLogs; j++) {
				if (strcmp(tmp, sqlLogList[j]) == 0) {
					found = 1;
					break;
				}
			}

			if (found) {
				free(tmp);
				i++;
				continue;
			}
				
			strncpy(sqlLogList[numLogs], tmp, MAXLOGPATHLEN-5);
			snprintf(sqlLogCopyList[numLogs], MAXLOGPATHLEN, "%s.copy", sqlLogList[numLogs]);
			numLogs++;
			free(tmp);
		}		
		i++;
	}

		/* add the default log file in case no log file is specified in config */
	tmp = param("LOG");
	if (tmp) {
		snprintf(sqlLogList[numLogs], MAXLOGPATHLEN-5, "%s/sql.log", tmp);
		snprintf(sqlLogCopyList[numLogs], MAXLOGPATHLEN, "%s.copy", sqlLogList[numLogs]);
	} else {
		snprintf(sqlLogList[numLogs], MAXLOGPATHLEN-5, "sql.log");
		snprintf(sqlLogCopyList[numLogs], MAXLOGPATHLEN, "%s.copy", sqlLogList[numLogs]);
	}
	numLogs++;

		// the "thrown" file is for recording events where big files are thrown away
	if (tmp) {
		snprintf(sqlLogCopyList[THROWFILE], MAXLOGPATHLEN, "%s/thrown.log", tmp);
		free(tmp);
	}
	else {
		snprintf(sqlLogCopyList[THROWFILE], MAXLOGPATHLEN, "thrown.log");
	}
	
		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	char *pollingPeriod_str = param("QUILLPP_POLLING_PERIOD");
	if(pollingPeriod_str) {
		pollingPeriod = atoi(pollingPeriod_str);
		free(pollingPeriod_str);
	}
	else pollingPeriod = 10;
  
	dprintf(D_ALWAYS, "Using Polling Period = %d\n", pollingPeriod);
	dprintf(D_ALWAYS, "Using logs ");
	for(i=0; i < numLogs; i++)
		dprintf(D_ALWAYS, "%s ", sqlLogList[i]);
	dprintf(D_ALWAYS, "\n");

	jqDBManager.config(reconfig);
	jqDBManager.init();

	DBObj = jqDBManager.getJobQueueDBObj();
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
		/*
		  instead of exiting on error, we simply return 
		  this means that tt will not usually exit on loss of 
		  database connectivity or other errors, and that it will keep polling
		  until it's successful.
		*/
	maintain();
}

void
TTManager::maintain() 
{	
	QuillErrCode retcode;
	bool bothOk = TRUE;

		// first call the job queue maintain function
	dprintf(D_ALWAYS, "******** Start of Polling Job Queue Log ********\n");

	retcode = jqDBManager.maintain();

	if (retcode == FAILURE) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Polling Job Queue Log <<<<<<<<\n");		
		bothOk = FALSE;
	} else {
		dprintf(D_ALWAYS, "********* End of Polling Job Queue Log *********\n");
	}

		// call the event log maintain function
	dprintf(D_ALWAYS, "******** Start of Polling Event Log ********\n");

	retcode = this->event_maintain();

	if (retcode == FAILURE) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Polling Event Log <<<<<<<<\n");		
		bothOk = FALSE;
	} else {
		dprintf(D_ALWAYS, "********* End of Polling Event Log *********\n");
	}

		// update currency if both log polling succeed
	if (bothOk) {
			// update the currency table
		char 	sql_str[1024];
		char    lastupdate[100];
		struct tm *tm;
		time_t clock;
		int ret_st;
		const char *scheddname;

		(void)time(  (time_t *)&clock );
		tm = localtime( (time_t *)&clock );	

		snprintf(lastupdate, 100, "%d/%d/%d %02d:%02d:%02d %s", 
				 tm->tm_mon+1,
				 tm->tm_mday,
				 tm->tm_year+1900,
				 tm->tm_hour,
				 tm->tm_min,
				 tm->tm_sec,
				 my_timezone(tm->tm_isdst));
	
		scheddname = jqDBManager.getScheddname();
	
		snprintf(sql_str, 1023, "UPDATE currency SET lastupdate = '%s' WHERE datasource = '%s';", lastupdate, scheddname);

		ret_st = DBObj->execCommand(sql_str);
		if (ret_st == FAILURE) {
			dprintf(D_ALWAYS, "Update currency --- ERROR [SQL] %s\n", sql_str);
		}
	}
}

QuillErrCode
TTManager::event_maintain() 
{
	FILESQL *filesqlobj = NULL;
	const char *buf = (char *)0;

	int  buflength=0;
	bool firststmt = true;
	char optype[7], eventtype[EVENTTYPEMAX];
	AttrList *ad = 0, *ad1 = 0;
	MyString *line_buf = 0;
	
		/* copy event log files */	
	int i;
	for(i=0; i < numLogs; i++) {
		filesqlobj = new FILESQL(sqlLogList[i], O_CREAT|O_RDWR);

	    if (filesqlobj->file_open() == FAILURE) {
			goto ERROREXIT;
		}

		if (filesqlobj->file_lock() == FAILURE) {
			goto ERROREXIT;
		}		
		
		if (append(sqlLogCopyList[i], sqlLogList[i]) == FAILURE) {
			goto ERROREXIT;
		}

		if(filesqlobj->file_truncate() == FAILURE) {
			goto ERROREXIT;
		}

		if(filesqlobj->file_unlock() == FAILURE) {
			goto ERROREXIT;
		}
		delete filesqlobj;
		filesqlobj = NULL;
	}

		// check if connection is ok, if not, try to reset it now
		// if connection still bad, don't bother processing the logs
	if ((DBObj->checkConnection() == FAILURE) && 
		(DBObj->resetConnection() == FAILURE)) {
		goto DBERROR;
	}

		// notice we add 1 to numLogs because the last file is the special "thrown" file
	for(i=0; i < numLogs+1; i++) {
		filesqlobj = new FILESQL(sqlLogCopyList[i], O_CREAT|O_RDWR);

		if (filesqlobj->file_open() == FAILURE) {
			goto ERROREXIT;
		}
			
		if (filesqlobj->file_lock() == FAILURE) {
			goto ERROREXIT;
		}

		firststmt = true;
		line_buf = new MyString();
		while(filesqlobj->file_readline(line_buf)) {
			buf = line_buf->Value();

				// if it is empty, we assume it's the end of log file
			buflength = strlen(buf);
			if(buflength == 0) {
				break;
			}

				// if this is just a new line, just skip it
			if (strcmp(buf, "\n") == 0) {
				delete line_buf;
				line_buf = new MyString();
				continue;
			}

			if(firststmt) {
				if((DBObj->beginTransaction()) == FAILURE) {
					dprintf(D_ALWAYS, "Begin transaction --- Error\n");
					goto DBERROR;
				}
				firststmt = false;
			}

				// init the optype and eventtype
			strcpy(optype, "");
			strcpy(eventtype, "");
			sscanf(buf, "%7s %50s", optype, eventtype);

			if (strcmp(optype, "NEW") == 0) {
					// first read in the classad until the seperate ***

				if( !( ad=filesqlobj->file_readAttrList()) ) {
					goto ERROREXIT;
				} 				

				if (strcmp(eventtype, "Machines") == 0) {		
					if  (insertMachines(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcmp(eventtype, "Events") == 0) {
					if  (insertEvents(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcmp(eventtype, "Files") == 0) {
					if  (insertFiles(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcmp(eventtype, "Fileusages") == 0) {
					if  (insertFileusages(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcmp(eventtype, "History") == 0) {
					if  (insertHistoryJob(ad) == FAILURE) 
						goto DBERROR;
				} else {
					if (insertBasic(ad, eventtype) == FAILURE) 
						goto DBERROR;
				}
				
				delete ad;
				ad  = 0;

			} else if (strcmp(optype, "UPDATE") == 0) {
				if( !( ad=filesqlobj->file_readAttrList()) ) {
					goto ERROREXIT;
				} 		
			   
					// the second ad can be null, meaning there is no where clause
				ad1=filesqlobj->file_readAttrList();

				if (updateBasic(ad, ad1, eventtype) == FAILURE)
					goto DBERROR;
				
				delete ad;
				if (ad1) delete ad1;
				ad = 0;
				ad1 = 0;

			} else if (strcmp(optype, "DELETE") == 0) {
				dprintf(D_ALWAYS, "DELETE not supported yet\n");
			} else {
				dprintf(D_ALWAYS, "unknown optype in log: %s\n", optype);
			}

				// destroy the string object to reclaim memory
			delete line_buf;
			line_buf = new MyString();
		}

		if(filesqlobj->file_truncate() == FAILURE) {
			goto ERROREXIT;
		}

		if(filesqlobj->file_unlock() == FAILURE) {
			goto ERROREXIT;
		}

		if(!firststmt) {
			if((DBObj->commitTransaction()) == FAILURE) {
				dprintf(D_ALWAYS, "End transaction --- Error\n");
				goto DBERROR;
			}
		}

		if(filesqlobj) {
			delete filesqlobj;
			filesqlobj = NULL;
		}

		if (line_buf) {
			delete line_buf;
			line_buf = (MyString *)0;
		}
	}

	return SUCCESS;

 ERROREXIT:
	if(filesqlobj) {
		delete filesqlobj;
	}
	
	if (line_buf) {
		delete line_buf;
	}

	if (ad)
		delete ad;
	
	return FAILURE;

 DBERROR:

	dprintf(D_ALWAYS, "\t%s\n", DBObj->getDBError());

	if(filesqlobj) {
		delete filesqlobj;
	}	
	
	if (line_buf) 
		delete line_buf;

	if (ad)
		delete ad;

	// the failed transaction must be rolled back
	// so that subsequent SQLs don't continue to fail
	DBObj->rollbackTransaction();

	if (DBObj->checkConnection() == FAILURE) {
		this -> checkAndThrowBigFiles();
	}

	return FAILURE;
}

void TTManager::checkAndThrowBigFiles() {
	struct stat file_status;
	FILESQL *filesqlobj, *thrownfileobj;
	char ascTime[150] = "";
	int len;

	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	char tmp[512];

	thrownfileobj = new FILESQL(sqlLogCopyList[THROWFILE]);
	thrownfileobj ->file_open();

	int i;
	for(i=0; i < numLogs; i++) {
		stat(sqlLogCopyList[i], &file_status);
		
			// if the file is bigger than the max file size, we throw it away 
		if (file_status.st_size > FILESIZELIMT) {
			filesqlobj = new FILESQL(sqlLogCopyList[i], O_RDWR);
			filesqlobj->file_open();
			filesqlobj->file_truncate();
			delete filesqlobj;

			strcpy(ascTime,ctime(&file_status.st_mtime));
			len = strlen(ascTime);
			ascTime[len-1] = '\0';

			snprintf(tmp, 512, "filename = \"%s\"", sqlLogCopyList[i]);
			tmpClP1->Insert(tmp);		
			
			snprintf(tmp, 512, "machine_id = \"%s\"", my_full_hostname());
			tmpClP1->Insert(tmp);		

			snprintf(tmp, 512, "size = %d", (int)file_status.st_size);
			tmpClP1->Insert(tmp);		
			
			snprintf(tmp, 512, "throwtime = \"%s\"", ascTime);
			tmpClP1->Insert(tmp);				
			
			thrownfileobj->file_newEvent("Thrown", tmpClP1);
		}
	}

	delete thrownfileobj;
}

QuillErrCode TTManager::insertMachines(AttrList *ad) {
	HashTable<MyString, MyString> newClAd(200, attHashFunction, updateDuplicateKeys);
	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;

		// The way we use the following variables are risky of buffer overflow
		// therefore the following tries to allocate an amount that is unlikely 
		// to be exceeded. The horizontal machine table has around 30 columns.
		// on average, each column name or value is allowed to be 300 char long.

	char *attName = NULL, *attVal, *attNameList = NULL, *attValList = NULL, *tmpVal = NULL;
	int isFirst = TRUE;
	MyString aName, aVal, temp, machine_id;
	char *tmp1;
	char *inlist = NULL;

	char lastHeardFrom[300] = "";

		// previous LastHeardFrom from the current classad
		// previous LastHeardFrom from the database's machine_classad
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st, len, num_result=0;

	ad->sPrint(classAd);

	// Insert stuff into Machine_Classad

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;
			
				// the attribute name can't be longer than the log entry line size
				// make sure attName is freed always to prevent memory leak.
			attName = (char *)malloc(strlen(iter));

			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				// strip quotes from attVal (if any)
			attValLen = strlen(attVal);
			if (attVal[attValLen-1] == '"' || attVal[attValLen-1] == '\'')
				attVal[attValLen-1] = 0;
			if (attVal[0] == '"' || attVal[0] == '\'') {
				
				tmpVal = (char *) malloc(strlen(attVal));
				tmp1 = attVal+1;
				strcpy(tmpVal, tmp1);
				strcpy(attVal, tmpVal);
				free(tmpVal);
				tmpVal  = NULL;
			}
			
			if (strcmp(attName, ATTR_PREV_LAST_HEARD_FROM) == 0) {
				prevLHFInAd = atoi(attVal);
			}
			else if (!isStaticMachineAttr(attName)) {
					// should go into machine_classad table
				if (isFirst) {
						//is the first in the list
					isFirst = FALSE;
					
						// 11 is the string length of "machine_id", 5 is for the overhead for enclosing
						// parenthesis
					attNameList = (char *) malloc (((strlen(attName) > 11)?strlen(attName):11) + 5);
					
						// 80 is the extra length needed for converting a seconds value to timestamp value
						// 7 is for the overhead of enclosing parenthesis and quotes
					attValList = (char *) malloc (strlen(attVal) + ((typeOf(attName) == TYPE_TIMESTAMP)?80:7));

					sprintf(attNameList, "(%s", (strcmp(attName, ATTR_NAME) ? attName : "machine_id") );
					if (!strcmp(attName, ATTR_NAME))
					    machine_id = attVal;
					switch (typeOf(attName)) {

					case TYPE_STRING:
						sprintf(attValList, "('%s'", attVal);
						break;
					case TYPE_TIMESTAMP:
						sprintf(attValList, "(('epoch'::timestamp + '%s seconds') at time zone 'UTC'", attVal);
						break;
					case TYPE_NUMBER:
						sprintf(attValList, "(%s", attVal);
						break;
					default:
						sprintf(attValList, "(%s", attVal);
						break;							
					}
				} else {
						// is not the first in the list
					
					attNameList = (char *) realloc (attNameList, strlen(attNameList) + 
													((strlen(attName) > 11)?strlen(attName):11) + 5);
					attValList = (char *) realloc (attValList, strlen(attValList) + 
												   strlen(attVal) + ((typeOf(attName) == TYPE_TIMESTAMP)?80:8));

					strcat(attNameList, ", ");
					strcat(attNameList, (strcmp(attName, ATTR_NAME) ? attName : "machine_id"));

					strcat(attValList, ", ");

					tmpVal = (char  *) malloc(strlen(attVal) + 100);

					switch (typeOf(attName)) {

					case TYPE_STRING:
						sprintf(tmpVal, "'%s'", attVal);
						break;
					case TYPE_TIMESTAMP:
						sprintf(tmpVal, "('epoch'::timestamp + '%s seconds') at time zone 'UTC'", attVal);
						break;
					case TYPE_NUMBER:
						sprintf(tmpVal, "%s", attVal);
						break;
					default:
						sprintf(tmpVal, "%s", attVal);
						break;							
					}

					strcat(attValList, tmpVal);
					
					free(tmpVal);
				}
			} else {  // static attributes (go into Machine table)
			        aName = attName;
				aVal = attVal;
				// insert into new ClassAd too (since this needs to go into DB)
				newClAd.insert(aName, aVal);
				if (NULL == inlist) {
					inlist = (char *) malloc (strlen(attName)+5);
					sprintf(inlist, "('%s'", attName);
				} else {
					inlist = (char *) realloc (inlist, strlen(inlist) + strlen(attName) + 5);
					strcat (inlist, ",'");
					strcat (inlist, attName);
					strcat (inlist, "'");
				}
			}

			if (strcasecmp(attName, ATTR_LAST_HEARD_FROM) == 0) {
				sprintf(lastHeardFrom, "('epoch'::timestamp + '%s seconds') at time zone 'UTC'", attVal);
			}

			free(attName);

			iter = classAd.GetNextToken("\n", true);
		}

	if (attNameList) strcat(attNameList, ")");
	if (attValList) strcat(attValList, ")");
	if (inlist) strcat(inlist, ")");

	len = 1024 + strlen(machine_id.Value()) + strlen(lastHeardFrom);
	sql_stmt = (char *) malloc (len);

		// get the previous lastheardfrom from the database 
	snprintf(sql_stmt, len, "SELECT extract(epoch from lastheardfrom) FROM Machine_Classad WHERE machine_id = '%s'", machine_id.Value());

	ret_st = DBObj->execQuery(sql_stmt, num_result);
	
	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}
	else if (ret_st == SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0,0));		
	}
	
		// set end time if the previous lastHeardFrom matches, otherwise
		// leave it as NULL (by default)
	if (prevLHFInDB == prevLHFInAd) {
		snprintf(sql_stmt, len, "INSERT INTO Machine_Classad_History (SELECT *, %s FROM Machine_Classad WHERE machine_id = '%s')", lastHeardFrom, machine_id.Value());
	} else {
		snprintf(sql_stmt, len, "INSERT INTO Machine_Classad_History (SELECT * FROM Machine_Classad WHERE machine_id = '%s')", machine_id.Value());
	}
	 
	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 if (attNameList) free(attNameList);
		 if (attValList) free(attValList);		 
		 if (inlist) free(inlist);
		 free(sql_stmt);
		 return FAILURE;
	 }

	 snprintf(sql_stmt, len, "DELETE FROM Machine_Classad WHERE machine_id = '%s'", machine_id.Value());

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 if (attNameList) free(attNameList);
		 if (attValList) free(attValList);		 
		 if (inlist) free(inlist);
		 free(sql_stmt);
		 return FAILURE;
	 }

	 free(sql_stmt);

	 sql_stmt = (char *) malloc(100 + strlen(attNameList) + strlen(attValList));

	 sprintf(sql_stmt, "INSERT INTO Machine_Classad %s VALUES %s", attNameList, attValList);

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 if (attNameList) free(attNameList);
		 if (attValList) free(attValList);
		 if (inlist) free(inlist);
		 free(sql_stmt);
		 return FAILURE;
	 }
	 
	 free(sql_stmt);

	 if (attNameList) free(attNameList);
	 if (attValList) free(attValList);

		// Insert changes into Machine
	 sql_stmt = (char *) malloc (1000 + strlen(inlist) + strlen(machine_id.Value()) + strlen(lastHeardFrom) );

		 // if the previous lastHeardFrom doesn't match, this means the 
		 // daemon has been shutdown for a while, we should move everything
		 // into the machine_history (with a NULL end_time)!
	 if (prevLHFInDB == prevLHFInAd) {
		 sprintf(sql_stmt, "INSERT INTO Machine_History SELECT machine_id, attr, val, start_time, %s FROM Machine WHERE machine_id = '%s' AND attr NOT IN %s", lastHeardFrom, machine_id.Value(), inlist);
	 } else {
		 sprintf(sql_stmt, "INSERT INTO Machine_History SELECT machine_id, attr, val, start_time FROM Machine WHERE machine_id = '%s'", machine_id.Value());		 
	 }

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 free(sql_stmt);
	 	 if (inlist) free(inlist);
		 return FAILURE;
	 }
		
	 if (prevLHFInDB == prevLHFInAd) {
		 sprintf(sql_stmt, "DELETE FROM Machine WHERE machine_id = '%s' AND attr NOT IN %s", machine_id.Value(), inlist);
	 } else {
		 sprintf(sql_stmt, "DELETE FROM Machine WHERE machine_id = '%s'", machine_id.Value());		 
	 }

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 free(sql_stmt);
	 	 if (inlist) free(inlist);
		 return FAILURE;
	 }
	 free(sql_stmt);
 	 if (inlist) free(inlist);
	 
	 newClAd.startIterations();
	 while (newClAd.iterate(aName, aVal)) {
		 
	 	 sql_stmt = (char *) malloc (1000 + 2*strlen(machine_id.Value()) + 2*strlen(aName.Value()) + 
								strlen(aVal.Value()) + strlen(lastHeardFrom));

		 sprintf(sql_stmt, "INSERT INTO Machine SELECT '%s', '%s', '%s', %s WHERE NOT EXISTS (SELECT * FROM Machine WHERE machine_id = '%s' AND attr = '%s')", 
				 machine_id.Value(), aName.Value(), aVal.Value(), lastHeardFrom, machine_id.Value(), aName.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			 free(sql_stmt);
			 return FAILURE;
		 }
			
		 sprintf(sql_stmt, "INSERT INTO Machine_History SELECT machine_id, attr, val, start_time, %s FROM Machine WHERE machine_id = '%s' AND attr = '%s' AND val != '%s'", 
				 lastHeardFrom, machine_id.Value(), aName.Value(), aVal.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);		
			 free(sql_stmt);
			 return FAILURE;
		 }

		 sprintf(sql_stmt, "UPDATE Machine SET val = '%s', start_time = %s WHERE machine_id = '%s' AND attr = '%s' AND val != '%s';", 
				 aVal.Value(), lastHeardFrom, machine_id.Value(), aName.Value(), aVal.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			free(sql_stmt);
			return FAILURE;
		 }
		 
		 free(sql_stmt);
	 }
	 return SUCCESS;
}

QuillErrCode TTManager::insertBasic(AttrList *ad, char *tableName) {
	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;	
	char *newvalue;

		// this function is very risky in that it is used for inserting
		// rows into any table in a basic way. If a table has many columns,
		// this function can easily overflow the following buffers. Please 
		// don't forget to fix it!!
	char *attName = NULL, *attVal, *attNameList = NULL, *attValList = NULL;
	int isFirst = TRUE;

	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;
			
				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				// escape single quote if any within the value
			newvalue = fillEscapeCharacters(attVal);
		
				// change double quotes to single quote if any
			attValLen = strlen(newvalue);
 
			if (newvalue[attValLen-1] == '"')
				newvalue[attValLen-1] = '\'';

			if (newvalue[0] == '"') {
				newvalue[0] = '\'';
			}			
			
			if (isFirst) {
					//is the first in the list
				isFirst = FALSE;

				attNameList = (char *) malloc (strlen(attName) + 4);
				attValList = (char *) malloc (strlen(newvalue) + 4);
											  
				sprintf(attNameList, "(%s", attName);
				sprintf(attValList, "(%s", newvalue);
			} else {					
						// is not the first in the list
				attNameList = (char *) realloc(attNameList, strlen(attNameList) + strlen(attName) + 5);
				attValList = (char *) realloc(attValList, strlen(attValList) + strlen(newvalue) + 5);
				
				strcat(attNameList, ", ");
				strcat(attNameList, attName);
				strcat(attValList, ", ");
				strcat(attValList, newvalue);					
			}

			free(newvalue);
			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	if (attNameList) strcat(attNameList, ")");
	if (attValList) strcat(attValList, ")");


	sql_stmt = (char *) malloc (50 + strlen(tableName) + strlen(attNameList) + strlen(attValList));

	sprintf(sql_stmt, "INSERT INTO %s %s VALUES %s;", tableName, attNameList, attValList);

	if (attNameList) free(attNameList);
	if (attValList) free(attValList);	
	
	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}

	free(sql_stmt);

	return SUCCESS;
}

QuillErrCode TTManager::insertEvents(AttrList *ad) {
	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;	
	char *attName = NULL, *attVal;
	char scheddname[50] = "", cluster[10] = "", proc[10] = "", 
		subproc[10] = "", 
		eventts[100] = "", messagestr[512] = "";
	int eventtype;
	char *newvalue;

	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;

				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				// escape single quote if any within the value
			newvalue = fillEscapeCharacters(attVal);

				// change double quotes to single quote if any
			attValLen = strlen(newvalue);
 
			if (newvalue[attValLen-1] == '"')
				newvalue[attValLen-1] = '\'';

			if (newvalue[0] == '"') {
				newvalue[0] = '\'';
			}

			if (strcmp(attName, "scheddname") == 0) {
				strcpy(scheddname, newvalue);
			} else if (strcmp(attName, "cluster") == 0) {
				strcpy(cluster, newvalue);
			} else if (strcmp(attName, "proc") == 0) {
				strcpy(proc, newvalue);
			} else if (strcmp(attName, "spid") == 0) {
				strcpy(subproc, newvalue);
			} else if (strcmp(attName, "eventtype") == 0) {
				eventtype = atoi(newvalue);
			} else if (strcmp(attName, "eventtime") == 0) {
				strcpy(eventts, newvalue);
			} else if (strcmp(attName, "description") == 0) {
				strcpy(messagestr, newvalue);
			}
			
			free(newvalue);
			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	sql_stmt = (char *) malloc(1000 + 2*strlen(scheddname) + 2*strlen(cluster) + 
							   2*strlen(proc) + strlen(eventts) + 
							   strlen(messagestr) + strlen(subproc));

	if (eventtype == ULOG_JOB_ABORTED || eventtype == ULOG_JOB_HELD || ULOG_JOB_RELEASED) {
		sprintf(sql_stmt, "INSERT INTO events VALUES (%s, %s, %s, NULL, %d, %s, %s);", 
				scheddname, cluster, proc, eventtype, eventts, messagestr);
	} else {
		sprintf(sql_stmt, "INSERT INTO events SELECT %s, %s, %s, run_id, %d, %s, %s  FROM runs WHERE scheddname = %s  AND cluster = %s and proc = %s AND spid = %s AND endtype is null;", 
				scheddname, cluster, proc, eventtype, eventts, messagestr, scheddname, cluster, proc, subproc);
	}

	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}

	free(sql_stmt);
	return SUCCESS;
}

QuillErrCode TTManager::insertFiles(AttrList *ad) {
	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;	
	char *attName = NULL, *attVal;
	
	char f_name[_POSIX_PATH_MAX] = "", f_host[50] = "", 
		f_path[_POSIX_PATH_MAX] = "", f_ts[30] = "";
	int f_size;
	char pathname[_POSIX_PATH_MAX] = "";
	char hexSum[MAC_SIZE*2+1] = "", sum[MAC_SIZE+1] = "";	
	int len;
	char *tmp1, *tmpVal = NULL;
	bool fileSame = TRUE;
	struct stat file_status;

	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;

				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				// change double quotes to single quote if any
			attValLen = strlen(attVal);
 
			if (attVal[attValLen-1] == '"')
				attVal[attValLen-1] = '\'';

			if (attVal[0] == '"') {
				attVal[0] = '\'';
			}

			if (strcmp(attName, "f_name") == 0) {
				strcpy(f_name, attVal);
			} else if (strcmp(attName, "f_host") == 0) {
				strcpy(f_host, attVal);
			} else if (strcmp(attName, "f_path") == 0) {
				strcpy(f_path, attVal);
			} else if (strcmp(attName, "f_ts") == 0) {
				strcpy(f_ts, attVal);
			} else if (strcmp(attName, "f_size") == 0) {
				f_size = atoi(attVal);
			}
			
			free(attName);

			iter = classAd.GetNextToken("\n", true);
		}

		// strip the quotes from path and name so that we can compute checksum
	len = strlen(f_path);

	if (f_path[len-1] == '\'')
		f_path[len-1] = 0;
	
	if (f_path[0] == '\'') {
		tmpVal = (char *) malloc(strlen(f_path));
		tmp1 = f_path+1;
		strcpy(tmpVal, tmp1);
		strcpy(f_path, tmpVal);
		free(tmpVal);
	}

	len = strlen(f_name);
	if (f_name[len-1] == '\'')
		f_name[len-1] = 0;
	
	if (f_name[0] == '\'') {
		tmpVal = (char *) malloc(strlen(f_name));
		tmp1 = f_name+1;
		strcpy(tmpVal, tmp1);
		strcpy(f_name, tmpVal);
		free(tmpVal);
	}

	sprintf(pathname, "%s/%s", f_path, f_name);

		// check if the file is still there and the same
	if (stat(pathname, &file_status) < 0) {
		dprintf(D_FULLDEBUG, "ERROR: File '%s' can not be accessed.\n", 
				pathname);
		fileSame = FALSE;
	} else {
		char ascTime[TIMELEN];

		// build ascii time to be stored in database
		char *tmp;
		tmp = ctime(&file_status.st_mtime);
		len = strlen(tmp);
		ascTime[0] = '\'';
		strncpy(&ascTime[1], (const char *)tmp, len-1); /* ignore the last newline character */
		ascTime[len] = '\'';		
		ascTime[len+1] = '\0';

		if (strcmp(f_ts, ascTime) != 0) {
			fileSame = FALSE;
		}
	}

  	if (fileSame && (f_size > 0) && file_checksum(pathname, f_size, sum)) {
		int i;
  		for (i = 0; i < MAC_SIZE; i++)
  			sprintf(&hexSum[2*i], "%2x", sum[i]);		
  		hexSum[2*MAC_SIZE] = '\0';
  	}
  	else
		hexSum[0] = '\0';

	sql_stmt = (char *)malloc(1000 + 2*(strlen(f_name)+strlen(f_host)+strlen(f_path)+strlen(f_ts)) +
							  strlen(hexSum));

	sprintf(sql_stmt, 
			"INSERT INTO files SELECT NEXTVAL('seqfileid'), '%s', %s, '%s', %s, %d, '%s' WHERE NOT EXISTS (SELECT * FROM files WHERE  name='%s' and path='%s' and host=%s and lastmodified=%s);", 
			f_name, f_host, f_path, f_ts, f_size, hexSum, f_name, f_path, f_host, f_ts);
	
	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}

	free(sql_stmt);
	return SUCCESS;
}

QuillErrCode TTManager::insertFileusages(AttrList *ad) {
	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;	
	char *attName, *attVal;
	
	char f_name[_POSIX_PATH_MAX] = "", f_host[50] = "", f_path[_POSIX_PATH_MAX] = "", f_ts[30] = "", globaljobid[100] = "", type[20] = "";
	int f_size;
 
	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;
			
				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				// change double quotes to single quote if any
			attValLen = strlen(attVal);
 
			if (attVal[attValLen-1] == '"')
				attVal[attValLen-1] = '\'';

			if (attVal[0] == '"') {
				attVal[0] = '\'';
			}

			if (strcmp(attName, "f_name") == 0) {
				strcpy(f_name, attVal);
			} else if (strcmp(attName, "f_host") == 0) {
				strcpy(f_host, attVal);
			} else if (strcmp(attName, "f_path") == 0) {
				strcpy(f_path, attVal);
			} else if (strcmp(attName, "f_ts") == 0) {
				strcpy(f_ts, attVal);
			} else if (strcmp(attName, "f_size") == 0) {
				f_size = atoi(attVal);
			} else if (strcmp(attName, "globalJobId") == 0) {
				strcpy(globaljobid, attVal);
			} else if (strcmp(attName, "type") == 0) {
				strcpy(type, attVal);
			}

			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	sql_stmt = (char *) malloc (1000+strlen(globaljobid)+strlen(type)+strlen(f_name)+
								strlen(f_path)+strlen(f_host)+strlen(f_ts));
	sprintf(sql_stmt, 
			"INSERT INTO fileusages SELECT %s, file_id, %s FROM files WHERE  name=%s and path=%s and host=%s and lastmodified=%s LIMIT 1;", globaljobid, type, f_name, f_path, f_host, f_ts);
	
	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}

	free(sql_stmt);
	return SUCCESS;
}

QuillErrCode TTManager::insertHistoryJob(AttrList *ad) {
  int        cid, pid;
  char       *sql_stmt = NULL;
  ExprTree *expr;
  ExprTree *L_expr;
  ExprTree *R_expr;
  char *value = NULL;
  char *name = NULL, *newname = NULL;
  char *tempvalue = NULL;
  char *newvalue;

  bool flag1=false, flag2=false,flag3=false, flag4=false;
  const char *scheddname = jqDBManager.getScheddname();

  ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cid);
  ad->EvalInteger (ATTR_PROC_ID, NULL, pid);

  sql_stmt = (char *)malloc(1000 + 2*(strlen(scheddname) + 20));

  sprintf(sql_stmt,
          "DELETE FROM History_Horizontal WHERE scheddname = '%s' AND cluster = %d AND proc = %d;INSERT INTO History_Horizontal(scheddname, cluster, proc) VALUES('%s', %d, %d);", scheddname, cid, pid, scheddname, cid, pid);

  if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 free(sql_stmt);
		 return FAILURE;	  
  }
  else {
	  free(sql_stmt);

	  ad->ResetExpr(); // for iteration initialization
	  while((expr=ad->NextExpr()) != NULL) {
		  L_expr = expr->LArg();
		  L_expr->PrintToNewStr(&name);

		  if (name == NULL) break;

		  R_expr = expr->RArg();
		  R_expr->PrintToNewStr(&value);

		  if (value == NULL) {
			  free(name);
			  break;	  	  
		  }

		  /* the following are to avoid overwriting the attr values. The hack is based on the fact that 
		   * an attribute of a job ad comes before the attribute of a cluster ad. And this is because 
		   * attribute list of cluster ad is chained to a job ad.
		   */
		  if(strcasecmp(name, "jobstatus") == 0) {
			  if(flag4) continue;
			  flag4 = true;
		  }

		  if(strcasecmp(name, "remotewallclocktime") == 0) {
			  if(flag1) continue;
			  flag1 = true;
		  }
		  else if(strcasecmp(name, "completiondate") == 0) {
			  if(flag2) continue;
			  flag2 = true;
		  }
		  else if(strcasecmp(name, "committedtime") == 0) {
			  if(flag3) continue;
			  flag3 = true;
		  }

		  if(isHorizontalHistoryAttribute(name)) {
			  if(strcasecmp(name, "in") == 0 ||
				 strcasecmp(name, "user") == 0) {
				  newname = (char *)malloc(strlen(name)+3);
				  snprintf(newname, strlen(name)+3, "%s_j", name);
				  free(name);
				  name = newname;
			  }

			  if (strcasecmp(name, "user_j") == 0) {
				  tempvalue = (char *)malloc(strlen(value));
				  strncpy(tempvalue, value+1, strlen(value)-2);
				  tempvalue[strlen(value)-2] = '\0';
				  strcpy(value, tempvalue);
				  free(tempvalue);
			  }
	  
			  sql_stmt = (char *) malloc(1000 + strlen(name) + strlen(value) + strlen(scheddname));

			  if(strcasecmp(name, "qdate") == 0 || 
				 strcasecmp(name, "lastmatchtime") == 0 || 
				 strcasecmp(name, "jobstartdate") == 0 || 
				 strcasecmp(name, "jobcurrentstartdate") == 0 ||
				 strcasecmp(name, "enteredcurrentstatus") == 0 ||
				 strcasecmp(name, "completiondate") == 0
				 ) {
					  // avoid updating with epoch time
				  if (strcmp(value, "0") == 0) {
					  free(name);
					  free(value);
					  continue;
				  } 
			
				  sprintf(sql_stmt,
						  "UPDATE History_Horizontal SET %s = (('epoch'::timestamp + '%s seconds') at time zone 'UTC') WHERE scheddname = '%s' and cluster = %d and proc = %d;", name, value, scheddname, cid, pid);

			  }	else {
				  strip_double_quote(value);
				  newvalue = fillEscapeCharacters(value);
				  sprintf(sql_stmt, 
						  "UPDATE History_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cluster = %d and proc = %d;", name, newvalue, scheddname, cid, pid);			  
				  free(newvalue);
			  }
		  } else {
			  strip_double_quote(value);                
			  newvalue = fillEscapeCharacters(value);

			  sql_stmt = (char *) malloc(1000+2*(strlen(scheddname) + strlen(name) + strlen(newvalue)));

			  sprintf(sql_stmt, 
					  "DELETE FROM History_Vertical WHERE scheddname = '%s' AND cluster = %d AND proc = %d AND attr = '%s'; INSERT INTO History_Vertical(scheddname, cluster, proc, attr, val) VALUES('%s', %d, %d, '%s', '%s');", scheddname, cid, pid, name, scheddname, cid, pid, name, newvalue);

			  free(newvalue);
		  }	  

		  if (DBObj->execCommand(sql_stmt) == FAILURE) {
			  dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			  dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);

			  free(name);
			  free(value);
			  free(sql_stmt);

			  return FAILURE;
		  }
		  
		  free(name);
		  name = NULL;
		  free(value);
		  value = NULL;
		  free(sql_stmt);
	  }
  }  

  return SUCCESS;
}

QuillErrCode TTManager::updateBasic(AttrList *info, AttrList *condition, 
									char *tableName) {
	char *sql_stmt = NULL;
	MyString classAd, classAd1;
	const char *iter;	
	char setList[1000]="", whereList[1000]="";
	char *attName = NULL, *attVal;
	char *newvalue;

	if (!info) return SUCCESS;

	info->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);	

	while (iter != NULL)
		{
			int attValLen;

				// the attribute name can't be longer than the log entry line size
			attName = (char *)malloc(strlen(iter));
			
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				// escape single quote if any within the value
			newvalue = fillEscapeCharacters(attVal);

				// change double quotes to single quote if any
			attValLen = strlen(newvalue);
 
			if (newvalue[attValLen-1] == '"')
				newvalue[attValLen-1] = '\'';

			if (newvalue[0] == '"') {
				newvalue[0] = '\'';
			}			
			
			strcat(setList, attName);
			strcat(setList, " = ");
			strcat(setList, newvalue);
			strcat(setList, ", ");

			free(newvalue);
			free(attName);

			iter = classAd.GetNextToken("\n", true);
		}

		// remove the last comma
	setList[strlen(setList)-2] = 0;

	if (condition) {
		condition->sPrint(classAd1);

		classAd1.Tokenize();
		iter = classAd1.GetNextToken("\n", true);	

		while (iter != NULL)
			{
				int attValLen;
				
					// the attribute name can't be longer than the log entry line size
				attName = (char *)malloc(strlen(iter));
				
				sscanf(iter, "%s =", attName);
				attVal = strstr(iter, "= ");
				attVal += 2;			

					// change smth=null (in classad) to smth is null (in sql)
				if (strcmp(attVal, "null") == 0) {
					strcat(whereList, attName);
					strcat(whereList, " is null and ");

					free(attName);

					iter = classAd1.GetNextToken("\n", true);
					continue;
				}

					// change double quotes to single quote if any
				attValLen = strlen(attVal);
 
				if (attVal[attValLen-1] == '"')
					attVal[attValLen-1] = '\'';

				if (attVal[0] == '"') {
					attVal[0] = '\'';
				}			
			
				strcat(whereList, attName);
				strcat(whereList, " = ");
				strcat(whereList, attVal);
				strcat(whereList, " and ");

				free(attName);
				iter = classAd1.GetNextToken("\n", true);
			}
		
			// remove the last comma
		whereList[strlen(whereList)-5] = 0;
	}

	sql_stmt = (char *) malloc (100 + strlen(tableName) + strlen(setList) + strlen(whereList));
		// build sql stmt
	sprintf(sql_stmt, "UPDATE %s SET %s WHERE %s;", tableName, setList, whereList);		
	
	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}
	
	free(sql_stmt);

	return SUCCESS;
}

static int 
typeOf(char *attName)
{
	if (!(strcmp(attName, ATTR_CKPT_SERVER) && strcmp(attName, "CKPT_SERVER_HOST") &&
		  strcmp(attName, ATTR_STATE) && strcmp(attName, ATTR_ACTIVITY) &&
		  strcmp(attName, ATTR_CPU_IS_BUSY) && strcmp(attName, ATTR_RANK) && 
		  strcmp(attName, ATTR_REQUIREMENTS) && strcmp(attName, ATTR_NAME) &&
		  strcmp(attName, ATTR_OPSYS) && strcmp(attName, ATTR_ARCH) &&
		  strcmp(attName, "GlobalJobId") &&
		  strcmp(attName, "username") &&
		  strcmp(attName, "scheddname") &&
		  strcmp(attName, "startdname") && 
		  strcmp(attName, "diagnosis") &&
		  strcmp(attName, "remote_user")
		  )
		)
		return TYPE_STRING;

	if (!(strcmp(attName, ATTR_KEYBOARD_IDLE) && strcmp(attName, ATTR_CONSOLE_IDLE) &&
		  strcmp(attName, ATTR_LOAD_AVG) && strcmp(attName, "CondorLoadAvg") &&
		  strcmp(attName, ATTR_TOTAL_LOAD_AVG) && strcmp(attName, ATTR_VIRTUAL_MEMORY) &&
		  strcmp(attName, ATTR_MEMORY ) && strcmp(attName, ATTR_TOTAL_VIRTUAL_MEMORY) &&
		  strcmp(attName, ATTR_CPU_BUSY_TIME) && strcmp(attName, ATTR_CURRENT_RANK) &&
		  strcmp(attName, ATTR_CLOCK_MIN) && strcmp(attName, ATTR_CLOCK_DAY) && 
		  strcmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) && strcmp(attName, ATTR_UPDATESTATS_TOTAL) &&
		  strcmp(attName, ATTR_UPDATESTATS_SEQUENCED) && strcmp(attName, ATTR_UPDATESTATS_LOST) &&
		  strcmp(attName, "cluster") && 
		  strcmp(attName, "proc") 
		  )
		)
		return TYPE_NUMBER;

	if (!(strcmp(attName, ATTR_LAST_HEARD_FROM) &&
		  strcmp(attName, ATTR_ENTERED_CURRENT_ACTIVITY) && strcmp(attName, ATTR_ENTERED_CURRENT_STATE) &&
		  strcmp(attName, "reject_time") &&
		  strcmp(attName, "match_time")
		  )
		)
		return TYPE_TIMESTAMP;

	return -1;
}

static int isStaticMachineAttr(char *attName)
{
	return (strcmp(attName, ATTR_OPSYS) && strcmp(attName, ATTR_ARCH) &&
			strcmp(attName, ATTR_CKPT_SERVER) && strcmp(attName, "CKPT_SERVER_HOST") &&
			strcmp(attName, ATTR_STATE) && strcmp(attName, ATTR_ACTIVITY) &&
			strcmp(attName, ATTR_KEYBOARD_IDLE) && strcmp(attName, ATTR_CONSOLE_IDLE) &&
			strcmp(attName, ATTR_LOAD_AVG) && strcmp(attName, "CondorLoadAvg") &&
			strcmp(attName, ATTR_TOTAL_LOAD_AVG) && strcmp(attName, ATTR_VIRTUAL_MEMORY) &&
			strcmp(attName, ATTR_MEMORY ) && strcmp(attName, ATTR_TOTAL_VIRTUAL_MEMORY) &&
			strcmp(attName, ATTR_CPU_BUSY_TIME) && strcmp(attName, ATTR_CPU_IS_BUSY) &&
			strcmp(attName, ATTR_RANK) && strcmp(attName, ATTR_CURRENT_RANK) &&
			strcmp(attName, ATTR_REQUIREMENTS) && strcmp(attName, ATTR_CLOCK_MIN) &&
			strcmp(attName, ATTR_CLOCK_DAY) && strcmp(attName, ATTR_LAST_HEARD_FROM) &&
			strcmp(attName, ATTR_ENTERED_CURRENT_ACTIVITY) && strcmp(attName, ATTR_ENTERED_CURRENT_STATE) &&
			strcmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) && strcmp(attName, ATTR_UPDATESTATS_TOTAL) &&
			strcmp(attName, ATTR_UPDATESTATS_SEQUENCED) && strcmp(attName, ATTR_UPDATESTATS_LOST) &&
			strcmp(attName, ATTR_NAME) && strcmp(attName, "GlobalJobId") &&
			strcmp(attName, "LastHeardFrom")
			);
}

static int file_checksum(char *filePathName, int fileSize, char *sum) {
	int fd;
	char *data;
	Condor_MD_MAC *checker = new Condor_MD_MAC();
	unsigned char *checksum;

	if (!filePathName || !sum) 
		return FALSE;

	fd = open(filePathName, O_RDONLY, 0);
	if (fd < 0) {
		dprintf(D_FULLDEBUG, "schedd_file_checksum: can't open %s\n", filePathName);
		return FALSE;
	}

	data = (char *)mmap(0, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == ((char *) -1))
		dprintf(D_FULLDEBUG, "schedd_file_checksum: mmap failed\n");

	close(fd);

	checker->addMD((unsigned char *) data, fileSize);
	if (munmap(data, fileSize) < 0)
		dprintf(D_FULLDEBUG, "schedd_file_checksum: munmap failed\n");

	checksum = checker->computeMD();

	if (checksum){
        memcpy(sum, checksum, MAC_SIZE);
        free(checksum);
	}
	else {
		dprintf(D_FULLDEBUG, "schedd_file_checksum: computeMD failed\n");
		delete checker;
		return FALSE;
	}

	delete checker;
	return TRUE;
}

static QuillErrCode append(char *destF, char *srcF) 
{	
	int dest, src;
	char buffer[4096];
	int rv;

	dest = open (destF, O_WRONLY|O_CREAT|O_APPEND, 0644);
	src = open (srcF, O_RDONLY);

	rv = read(src, buffer, 4096);

	while(rv > 0) {
		if (write(dest, buffer, rv) < 0) {
			return FAILURE;
		}
		
		rv = read(src, buffer, 4096);
	}

	close (dest);
	close (src);

	return SUCCESS;
}

// hash function for strings
static int attHashFunction (const MyString &str, int numBuckets)
{
        int i = str.Length() - 1, hashVal = 0;
        while (i >= 0)
        {
                hashVal += str[i];
                i--;
        }
        return (hashVal % numBuckets);
}
