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

#include "ttmanager.h"
#include "file_sql.h"

char logParamList[][30] = {"NEGOTIATOR_SQLLOG", "SCHEDD_SQLLOG", "SHADOW_SQLLOG", "STARTER_SQLLOG", "STARTD_SQLLOG", "SUBMIT_SQLLOG", ""};

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
	} else {
		sprintf(sqlLogList[numLogs], "sql.log");
		sprintf(sqlLogCopyList[numLogs], "%s.copy", sqlLogList[numLogs]);
	}
	numLogs++;

		// the "thrown" file is for recording events where big files are thrown away
	if (tmp) {
		sprintf(sqlLogCopyList[THROWFILE], "%s/thrown.log", tmp);
		free(tmp);
	}
	else {
		sprintf(sqlLogCopyList[THROWFILE], "thrown.log");
	}
	
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
	char *buf = (char *)0;

	int  buflength=0;
	int  retval;
	bool firststmt = true;
	char optype[7], eventtype[EVENTTYPEMAX];
	AttrList *ad = 0, *ad1 = 0;

		/* copy files */	
	for(int i=0; i < numLogs; i++) {
		filesqlobj = new FILESQL(sqlLogList[i], O_CREAT|O_RDWR);

		retval = filesqlobj->file_open();
		if (retval < 0) {
			goto ERROREXIT;
		}

		retval = filesqlobj->file_lock();
		
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

		// the last file is the "thrown" file
	for(int i=0; i < numLogs+1; i++) {
		buf =(char *) malloc(2048 * sizeof(char));
		filesqlobj = new FILESQL(sqlLogCopyList[i], O_CREAT|O_RDWR);

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

			if(firststmt) {
				if(DBObj->odbc_beginxtstmt("BEGIN") < 0) {
					dprintf(D_ALWAYS, "Begin transaction --- Error\n");
					goto DBERROR;
				}
				firststmt = false;
			}

			sscanf(buf, "%7s %50s", optype, eventtype);

			if (strcmp(optype, "NEW") == 0) {
					// first read in the classad until the seperate ***

				if( !( ad=filesqlobj->file_readAttrList()) ) {
					goto ERROREXIT;
				} 				

				if (strcmp(eventtype, "Machines") == 0) {		
					if  (insertMachines(ad) <   0) 
						goto DBERROR;
				} else if (strcmp(eventtype, "Events") == 0) {
					if  (insertEvents(ad) <   0) 
						goto DBERROR;
				} else if (strcmp(eventtype, "Files") == 0) {
					if  (insertFiles(ad) <   0) 
						goto DBERROR;
				} else if (strcmp(eventtype, "Fileusages") == 0) {
					if  (insertFileusages(ad) <   0) 
						goto DBERROR;
				} else {
					if (insertPlain(ad, eventtype) < 0) 
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

				updatePlain(ad, ad1, eventtype);
				 
			} else if (strcmp(optype, "DELETE") == 0) {
				dprintf(D_ALWAYS, "DELETE not supported\n");
			} else {
				dprintf(D_ALWAYS, "unknown optype in log: %s\n", optype);
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
				goto DBERROR;
			}
		}

		if(filesqlobj) {
			delete filesqlobj;
			filesqlobj = NULL;
		}

		if (buf)
			free(buf);
	}

	return 1;

 ERROREXIT:
	if(filesqlobj) {
		delete filesqlobj;
	}
	
	if (buf)
		free(buf);

	if (ad)
		delete ad;
	
	return retval;

 DBERROR:
	if(filesqlobj) {
		delete filesqlobj;
	}	
	
	if (buf) 
		free(buf);

	if (ad)
		delete ad;

	if (!DBObj->isConnected()) {
		this -> checkAndThrowBigFiles();
	}

	return -1;
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

void TTManager::checkAndThrowBigFiles() {
	struct stat file_status;
	FILESQL *filesqlobj, *thrownfileobj;
	char ascTime[150];
	int len;

	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	char tmp[512];

	thrownfileobj = new FILESQL(sqlLogCopyList[THROWFILE]);
	thrownfileobj ->file_open();

	for(int i=0; i < numLogs; i++) {
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

			sprintf(tmp, "filename = \"%s\"", sqlLogCopyList[i]);
			tmpClP1->Insert(tmp);		
			
			sprintf(tmp, "machine = \"%s\"", my_full_hostname());
			tmpClP1->Insert(tmp);		

			sprintf(tmp, "size = %d", (int)file_status.st_size);
			tmpClP1->Insert(tmp);		
			
			sprintf(tmp, "throwtime = \"%s\"", ascTime);
			tmpClP1->Insert(tmp);				
			
			thrownfileobj->file_newEvent("Thrown", tmpClP1);
		}
	}

	delete thrownfileobj;
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

int TTManager::insertMachines(AttrList *ad) {
	HashTable<MyString, MyString> newClAd(200, attHashFunction, updateDuplicateKeys);
	char sql_stmt[1000];
	MyString classAd;
	const char *iter;
	char attName[100], *attVal, attNameList[1000]="", attValList[1000]="", tmpVal[500];
	int isFirst = TRUE;
	MyString aName, aVal, temp, machine_id;
	char *tmp1;

	ad->sPrint(classAd);

	// Insert stuff into Machine_Classad

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				// strip quotes from attVal (if any)
			attValLen = strlen(attVal);
			if (attVal[attValLen-1] == '"' || attVal[attValLen-1] == '\'')
				attVal[attValLen-1] = 0;
			if (attVal[0] == '"' || attVal[0] == '\'') {
				
				tmp1 = attVal+1;
				strcpy(tmpVal, tmp1);
				strcpy(attVal, tmpVal);
			}
			
				
			if (!isStaticMachineAttr(attName)) {
					// should go into machine_classad table
				if (isFirst) {
						//is the first in the list
					isFirst = FALSE;
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
					strcat(attNameList, ", ");
					strcat(attNameList, (strcmp(attName, ATTR_NAME) ? attName : "machine_id"));

					strcat(attValList, ", ");
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

				}
			} else {  // static attributes (go into Machine table)
			        aName = attName;
				aVal = attVal;
				// insert into new ClassAd too (since this needs to go into DB)
				newClAd.insert(aName, aVal);
			}
			iter = classAd.GetNextToken("\n", true);
		}

	strcat(attNameList, ")");
	strcat(attValList, ")");
	
	 sprintf(sql_stmt, "INSERT INTO Machine_Classad_History (SELECT * FROM Machine_Classad WHERE machine_id = '%s')", machine_id.Value());
	 
	 if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 return -1;
	 }

	 sprintf(sql_stmt, "DELETE FROM Machine_Classad WHERE machine_id = '%s'", machine_id.Value());

	 if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 return -1;
	 }

	 sprintf(sql_stmt, "INSERT INTO Machine_Classad %s VALUES %s", attNameList, attValList);

	 if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 return -1;
	 }

		// Insert changes into Machine
	newClAd.startIterations();
	while (newClAd.iterate(aName, aVal)) {
		sprintf(sql_stmt, "INSERT INTO Machine SELECT '%s', '%s', '%s', %s WHERE NOT EXISTS (SELECT * FROM Machine WHERE machine_id = '%s' AND attr_name = '%s')", 
				machine_id.Value(), aName.Value(), aVal.Value(), tmpVal, machine_id.Value(), aName.Value());

		if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			return -1;
		}
			
		sprintf(sql_stmt, "INSERT INTO Machine_History SELECT machine_id, attr_name, attr_value, start_time, %s FROM Machine WHERE machine_id = '%s' AND attr_name = '%s' AND attr_value != '%s'", 
				tmpVal, machine_id.Value(), aName.Value(), aVal.Value());

		if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			return -1;
		}

		sprintf(sql_stmt, "UPDATE Machine SET attr_value = '%s', start_time = %s WHERE machine_id = '%s' AND attr_name = '%s' AND attr_value != '%s';", 
				aVal.Value(), tmpVal, machine_id.Value(), aName.Value(), aVal.Value());

		if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
			dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			return -1;
		}

	}

	return 0;

}

int TTManager::insertPlain(AttrList *ad, char *tableName) {
	char sql_stmt[1000];
	MyString classAd;
	const char *iter;	
	char attName[100], *attVal, attNameList[1000]="", attValList[1000]="";
	int isFirst = TRUE;

	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;
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
			
			if (isFirst) {
					//is the first in the list
				isFirst = FALSE;
				sprintf(attNameList, "(%s", attName);
				sprintf(attValList, "(%s", attVal);
			} else {					
						// is not the first in the list
					strcat(attNameList, ", ");
					strcat(attNameList, attName);
					strcat(attValList, ", ");
					strcat(attValList, attVal);
					
			}

			iter = classAd.GetNextToken("\n", true);
		}

	strcat(attNameList, ")");
	strcat(attValList, ")");

	sprintf(sql_stmt, "INSERT INTO %s %s VALUES %s;", tableName, attNameList, attValList);

	if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		return -1;
	}

	return 0;
}

int TTManager::insertEvents(AttrList *ad) {
	char sql_stmt[1000];
	MyString classAd;
	const char *iter;	
	char attName[100], *attVal;
	char scheddname[50], cluster[10], proc[10], subproc[10], 
		eventts[100], messagestr[512];
	int eventtype;
	
	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;
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

			if (strcmp(attName, "scheddname") == 0) {
				strcpy(scheddname, attVal);
			} else if (strcmp(attName, "cid") == 0) {
				strcpy(cluster, attVal);
			} else if (strcmp(attName, "pid") == 0) {
				strcpy(proc, attVal);
			} else if (strcmp(attName, "spid") == 0) {
				strcpy(subproc, attVal);
			} else if (strcmp(attName, "eventtype") == 0) {
				eventtype = atoi(attVal);
			} else if (strcmp(attName, "eventtime") == 0) {
				strcpy(eventts, attVal);
			} else if (strcmp(attName, "description") == 0) {
				strcpy(messagestr, attVal);
			}
					   
			iter = classAd.GetNextToken("\n", true);
		}

	if (eventtype == ULOG_JOB_ABORTED || eventtype == ULOG_JOB_HELD || ULOG_JOB_RELEASED) {
		sprintf(sql_stmt, "INSERT INTO events VALUES (%s, %s, %s, NULL, %d, %s, %s);", 
				scheddname, cluster, proc, eventtype, eventts, messagestr);
	} else {
		sprintf(sql_stmt, "INSERT INTO events SELECT %s, %s, %s, run_id, %d, %s, %s  FROM runs WHERE scheddname = %s  AND cid = %s and pid = %s AND spid = %s AND endtype is null;", 
				scheddname, cluster, proc, eventtype, eventts, messagestr, scheddname, cluster, proc, subproc);
	}

	if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		return -1;
	}

	return 0;
}

int TTManager::insertFiles(AttrList *ad) {
	char sql_stmt[1000];
	MyString classAd;
	const char *iter;	
	char attName[100], *attVal;
	
	char f_name[_POSIX_PATH_MAX], f_host[50], f_path[_POSIX_PATH_MAX], f_ts[30];
	int f_size;
	char pathname[_POSIX_PATH_MAX];
	char hexSum[MAC_SIZE*2+1], sum[MAC_SIZE+1];	
	int len;
	char *tmp1, tmpVal[500];

	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;
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
					   
			iter = classAd.GetNextToken("\n", true);
		}

		// strip the quotes from path and name so that we can compute checksum
	len = strlen(f_path);

	if (f_path[len-1] == '\'')
		f_path[len-1] = 0;
	
	if (f_path[0] == '\'') {
		tmp1 = f_path+1;
		strcpy(tmpVal, tmp1);
		strcpy(f_path, tmpVal);
	}

	len = strlen(f_name);
	if (f_name[len-1] == '\'')
		f_name[len-1] = 0;
	
	if (f_name[0] == '\'') {
		tmp1 = f_name+1;
		strcpy(tmpVal, tmp1);
		strcpy(f_name, tmpVal);
	}

	sprintf(pathname, "%s/%s", f_path, f_name);
	
  	if ((f_size > 0) && file_checksum(pathname, f_size, sum)) {
  		for (int i = 0; i < MAC_SIZE; i++)
  			sprintf(&hexSum[2*i], "%2x", sum[i]);		
  		hexSum[2*MAC_SIZE] = '\0';
  	}
  	else
		hexSum[0] = '\0';

	sprintf(sql_stmt, 
			"INSERT INTO files SELECT NEXTVAL('seqfileid'), '%s', %s, '%s', %s, %d, '%s' WHERE NOT EXISTS (SELECT * FROM files WHERE  f_name='%s' and f_path='%s' and f_host=%s and f_ts=%s);", 
			f_name, f_host, f_path, f_ts, f_size, hexSum, f_name, f_path, f_host, f_ts);
	
	if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		return -1;
	}

	return 0;
}

int TTManager::insertFileusages(AttrList *ad) {
	char sql_stmt[1000];
	MyString classAd;
	const char *iter;	
	char attName[100], *attVal;
	
	char f_name[_POSIX_PATH_MAX], f_host[50], f_path[_POSIX_PATH_MAX], f_ts[30], globaljobid[100], type[20];
	int f_size;
 
	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			int attValLen;
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

					   
			iter = classAd.GetNextToken("\n", true);
		}

	sprintf(sql_stmt, 
			"INSERT INTO fileusages SELECT %s, f_id, %s FROM files WHERE  f_name=%s and f_path=%s and f_host=%s and f_ts=%s LIMIT 1;", globaljobid, type, f_name, f_path, f_host, f_ts);
	
	if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		return -1;
	}

	return 0;
}

int TTManager::updatePlain(AttrList *info, AttrList *condition, char *tableName) {
	char sql_stmt[1000];
	MyString classAd, classAd1;
	const char *iter;	
	char setList[1000]="", whereList[1000]="";
	char attName[100], *attVal;

	if (!info) return 0;

	info->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);	

	while (iter != NULL)
		{
			int attValLen;

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
			
			strcat(setList, attName);
			strcat(setList, " = ");
			strcat(setList, attVal);
			strcat(setList, ", ");

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

				sscanf(iter, "%s =", attName);
				attVal = strstr(iter, "= ");
				attVal += 2;			

					// change smth=null (in classad) to smth is null (in sql)
				if (strcmp(attVal, "null") == 0) {
					strcat(whereList, attName);
					strcat(whereList, " is null and ");

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

				iter = classAd1.GetNextToken("\n", true);
			}
			// remove the last comma
		whereList[strlen(whereList)-5] = 0;
	}

		// build sql stmt
	sprintf(sql_stmt, "UPDATE %s SET %s WHERE %s;", tableName, setList, whereList);		
	
	if (DBObj->odbc_sqlstmt(sql_stmt) < 0) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		return -1;
	}
	
	return 0;
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
		return FALSE;
	}

	return TRUE;
}
