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
#include "file_xml.h"
#include "jobqueuedatabase.h"
#include "pgsqldatabase.h"
#include "misc_utils.h"
#include "condor_ttdb.h"

char logParamList[][30] = {"NEGOTIATOR_SQLLOG", "SCHEDD_SQLLOG", 
						   "SHADOW_SQLLOG", "STARTER_SQLLOG", 
						   "STARTD_SQLLOG", "SUBMIT_SQLLOG", 
						   "COLLECTOR_SQLLOG", ""};

#define CONDOR_TT_FILESIZELIMT 1900000000L
#define CONDOR_TT_THROWFILE numLogs
#define CONDOR_TT_EVENTTYPEMAXLEN 100
#define CONDOR_TT_TIMELEN 60

#define CONDOR_TT_TYPE_STRING    1
#define CONDOR_TT_TYPE_NUMBER    2
#define CONDOR_TT_TYPE_TIMESTAMP 3

static int attHashFunction (const MyString &str, int numBuckets);
static int isHorizontalMachineAttr(char *attName);
static int isHorizontalDaemonAttr(char *attName);
static int isHorizontalScheddAttr(char *attName);
static int typeOf(char *attName);
static int file_checksum(char *filePathName, int fileSize, char *sum);
static QuillErrCode append(char *destF, char *srcF);
static void stripquotes(char *strv);

//! constructor
TTManager::TTManager()
{
		//nothing here...its all done in config()
	DBObj = (JobQueueDatabase  *) 0;
}

//! destructor
TTManager::~TTManager()
{
	if (collectors) {
		delete collectors;
	}

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
			
			found = 0;

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
				
			strncpy(sqlLogList[numLogs], tmp, CONDOR_TT_MAXLOGPATHLEN-5);
			snprintf(sqlLogCopyList[numLogs], CONDOR_TT_MAXLOGPATHLEN, 
					 "%s.copy", sqlLogList[numLogs]);
			numLogs++;
			free(tmp);
		}		
		i++;
	}

		/* add the default log file in case no log file is specified in 
		   config 
		*/
	tmp = param("LOG");
	if (tmp) {
		snprintf(sqlLogList[numLogs], CONDOR_TT_MAXLOGPATHLEN-5, 
				 "%s/sql.log", tmp);
		snprintf(sqlLogCopyList[numLogs], CONDOR_TT_MAXLOGPATHLEN, 
				 "%s.copy", sqlLogList[numLogs]);
	} else {
		snprintf(sqlLogList[numLogs], CONDOR_TT_MAXLOGPATHLEN-5, "sql.log");
		snprintf(sqlLogCopyList[numLogs], CONDOR_TT_MAXLOGPATHLEN, 
				 "%s.copy", sqlLogList[numLogs]);
	}
	numLogs++;

		/* the "thrown" file is for recording events where big files are 
		   thrown away
		*/
	if (tmp) {
		snprintf(sqlLogCopyList[CONDOR_TT_THROWFILE], CONDOR_TT_MAXLOGPATHLEN,
				 "%s/thrown.log", tmp);
		free(tmp);
	}
	else {
		snprintf(sqlLogCopyList[CONDOR_TT_THROWFILE], CONDOR_TT_MAXLOGPATHLEN,
				 "thrown.log");
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
	dt = jqDBManager.getJobQueueDBType();
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

	dprintf(D_ALWAYS, "++++++++ Sending Quill ad to collector ++++++++\n");

	if(!ad) {
		createQuillAd();
	}

	updateQuillAd();

	collectors->sendUpdates ( UPDATE_QUILL_AD, ad, NULL, true );

	dprintf(D_ALWAYS, "++++++++ Sent Quill ad to collector ++++++++\n");

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
		time_t clock;
		int ret_st;
		const char *scheddname;
		char  *ts_expr;

		(void)time(  (time_t *)&clock );
		ts_expr = condor_ttdb_buildts(&clock, dt);
	
		if (ts_expr == NULL) 
			{
				dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
				return;
			}

		scheddname = jqDBManager.getScheddname();

		snprintf(sql_str, 1023, "UPDATE currency SET lastupdate = %s WHERE datasource = '%s'", ts_expr, scheddname);

		free(ts_expr);

		ret_st = DBObj->execCommand(sql_str);
		if (ret_st == FAILURE) {
			dprintf(D_ALWAYS, "Update currency --- ERROR [SQL] %s\n", sql_str);
		}

		ret_st = DBObj->commitTransaction();
		if (ret_st == FAILURE) {
			dprintf(D_ALWAYS, "Commit transaction failed in TTManager::maintain\n");
		}

	}

		// call the xml log maintain function
	dprintf(D_ALWAYS, "******** Start of Polling XML Log ********\n");

	retcode = this->xml_maintain();

	if (retcode == FAILURE) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Polling XML Log <<<<<<<<\n");		
		bothOk = FALSE;
	} else {
		dprintf(D_ALWAYS, "********* End of Polling XML Log *********\n");
	}

}


//! update the QUILL_AD sent to the collector
/*! This method only updates the ad with new values of dynamic attributes
 *  See createQuillAd for how to create the ad in the first place
 */

void TTManager::updateQuillAd(void) {
	char expr[1000];

	/*
	sprintf( expr, "%s = %d", ATTR_QUILL_SQL_LAST_BATCH, 
			 lastBatchSqlProcessed);
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_QUILL_SQL_TOTAL, 
			 totalSqlProcessed);
	ad->Insert(expr);

	sprintf( expr, "%s = %d", "TimeToProcessLastBatch", 
			 secsLastBatch);
	ad->Insert(expr);

	sprintf( expr, "%s = %d", "IsConnectedToDB", 
			 isConnectedToDB);
	ad->Insert(expr);
	*/
}

//! create the QUILL_AD sent to the collector
/*! This method reads all quill-related configuration options from the 
 *  config file and creates a classad which can be sent to the collector
 */

void TTManager::createQuillAd(void) {
	char expr[1000];

	char *scheddName;

	char *mysockname;
	char *tmp;

	ad = new ClassAd();
	ad->SetMyTypeName(QUILL_ADTYPE);
	ad->SetTargetTypeName("");
  
	config_fill_ad(ad);

		// schedd info is used to identify the schedd 
		// corresponding to this quill 

	tmp = param( "SCHEDD_NAME" );
	if( tmp ) {
		scheddName = build_valid_daemon_name( tmp );
	} else {
		scheddName = default_daemon_name();
	}

	char *quill_name = param("QUILL_NAME");
	if(!quill_name) {
		EXCEPT("Cannot find variable QUILL_NAME in config file\n");
	}

	if (param_boolean("QUILL_IS_REMOTELY_QUERYABLE", true) == true) {
		sprintf( expr, "%s = TRUE", ATTR_QUILL_IS_REMOTELY_QUERYABLE);
	} else {
		sprintf( expr, "%s = FALSE", ATTR_QUILL_IS_REMOTELY_QUERYABLE);
	}
	ad->Insert(expr);

	sprintf( expr, "%s = %d", "QuillPollingPeriod", pollingPeriod );
	ad->Insert(expr);

	/*
	char *quill_query_passwd = param("QUILL_DB_QUERY_PASSWORD");
	if(!quill_query_passwd) {
		EXCEPT("Cannot find variable QUILL_DB_QUERY_PASSWORD "
			   "in config file\n");
	}
  
	sprintf( expr, "%s = \"%s\"", ATTR_QUILL_DB_QUERY_PASSWORD, 
			 quill_query_passwd );
	ad->Insert(expr);
	*/

	sprintf( expr, "%s = \"%s\"", ATTR_NAME, quill_name );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_SCHEDD_NAME, scheddName );
	ad->Insert(expr);

	if(scheddName) {
		delete scheddName;
	}

	sprintf( expr, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() ); 
	ad->Insert(expr);
  
		// Put in our sinful string.  Note, this is never going to
		// change, so we only need to initialize it once.
	mysockname = strdup( daemonCore->InfoCommandSinfulString() );

	sprintf( expr, "%s = \"%s\"", ATTR_MY_ADDRESS, mysockname );
	ad->Insert(expr);

	/*
	sprintf( expr, "%s = \"<%s>\"", ATTR_QUILL_DB_IP_ADDR, 
			 jobQueueDBIpAddress );
	ad->Insert(expr);
	*/

	/*
	sprintf( expr, "%s = \"%s\"", ATTR_QUILL_DB_NAME, jobQueueDBName );
	ad->Insert(expr);
	*/

	collectors = CollectorList::create();
  
	if(tmp) free(tmp);
	/*if(quill_query_passwd) free(quill_query_passwd); */
	if(quill_name) free(quill_name);
	if(mysockname) free(mysockname);
}

QuillErrCode
TTManager::event_maintain() 
{
	FILESQL *filesqlobj = NULL;
	const char *buf = (char *)0;

	int  buflength=0;
	bool firststmt = true;
	char optype[7], eventtype[CONDOR_TT_EVENTTYPEMAXLEN];
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

		/* process copies of event logs, notice we add 1 to numLogs because 
		   the last file is the special "thrown" file
		*/
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

				if (strcasecmp(eventtype, "Machines") == 0) {		
					if  (insertMachines(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Events") == 0) {
					if  (insertEvents(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Files") == 0) {
					if  (insertFiles(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Fileusages") == 0) {
					if  (insertFileusages(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "History") == 0) {
					if  (insertHistoryJob(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "ScheddAd") == 0) {
					if  (insertScheddAd(ad) == FAILURE) 
						goto DBERROR;	
				} else if (strcasecmp(eventtype, "MasterAd") == 0) {
					if  (insertMasterAd(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "NegotiatorAd") == 0) {
					if  (insertNegotiatorAd(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Runs") == 0) {
					if  (insertRuns(ad) == FAILURE) 
						goto DBERROR;
				} else if (strcasecmp(eventtype, "Transfers") == 0) {
					if  (insertTransfers(ad) == FAILURE) 
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
	
	if (ad1) 
		delete ad1;

	return FAILURE;

 DBERROR:
	if(filesqlobj) {
		delete filesqlobj;
	}	
	
	if (line_buf) 
		delete line_buf;

	if (ad)
		delete ad;

	if (ad1) 
		delete ad1;

	// the failed transaction must be rolled back
	// so that subsequent SQLs don't continue to fail
	DBObj->rollbackTransaction();

	this -> checkAndThrowBigFiles();

	if (DBObj->checkConnection() == FAILURE) {
		DBObj->resetConnection();
	}

	return FAILURE;
}

void TTManager::checkAndThrowBigFiles() {
	struct stat file_status;
	FILESQL *filesqlobj, *thrownfileobj;

	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	char tmp[512];

	thrownfileobj = new FILESQL(sqlLogCopyList[CONDOR_TT_THROWFILE]);
	thrownfileobj ->file_open();

	int i;
	for(i=0; i < numLogs; i++) {
		stat(sqlLogCopyList[i], &file_status);
		
			// if the file is bigger than the max file size, we throw it away 
		if (file_status.st_size > CONDOR_TT_FILESIZELIMT) {
			filesqlobj = new FILESQL(sqlLogCopyList[i], O_RDWR);
			filesqlobj->file_open();
			filesqlobj->file_truncate();
			delete filesqlobj;

			snprintf(tmp, 512, "filename = \"%s\"", sqlLogCopyList[i]);
			tmpClP1->Insert(tmp);		
			
			snprintf(tmp, 512, "machine_id = \"%s\"", my_full_hostname());
			tmpClP1->Insert(tmp);		

			snprintf(tmp, 512, "size = %d", (int)file_status.st_size);
			tmpClP1->Insert(tmp);		
			
			snprintf(tmp, 512, "throwtime = %d", (int)file_status.st_mtime);
			tmpClP1->Insert(tmp);				
			
			thrownfileobj->file_newEvent("Thrown", tmpClP1);
		}
	}

	delete thrownfileobj;
}

QuillErrCode
TTManager::xml_maintain() 
{
	bool want_xml = false;
	FILEXML *filexmlobj = NULL;
	char xmlParamList[][30] = {"NEGOTIATOR_XMLLOG", "SCHEDD_XMLLOG", 
						   "SHADOW_XMLLOG", "STARTER_XMLLOG", 
						   "STARTD_XMLLOG", "SUBMIT_XMLLOG", 
						   "COLLECTOR_XMLLOG", ""};	
	char *dump_path = "/u/p/a/pachu/RA/LogCorr/xml-logs";
	char *tmp, *fname;

	int numXLogs = 0, i = 0, found = 0;
	char    xmlLogList[CONDOR_TT_MAXLOGNUM][CONDOR_TT_MAXLOGPATHLEN];
	char    xmlLogCopyList[CONDOR_TT_MAXLOGNUM][CONDOR_TT_MAXLOGPATHLEN];

		// check if XML logging is turned on & if not, exit
	want_xml = param_boolean("WANT_XML_LOG", false);

	if ( !want_xml )
		return SUCCESS;

		// build list of xml logs and the copies
	while (xmlParamList[i][0] != '\0') {
		tmp = param(xmlParamList[i]);
		if (tmp) {
			
			found = 0;

				// check if the new log file is already in the list
			int j;
			for (j = 0 ; j < numXLogs; j++) {
				if (strcmp(tmp, xmlLogList[j]) == 0) {
					found = 1;
					break;
				}
			}

			if (found) {
				free(tmp);
				i++;
				continue;
			}
				
			strncpy(xmlLogList[numXLogs], tmp, CONDOR_TT_MAXLOGPATHLEN);
			fname = strrchr(tmp, '/')+1;
			snprintf(xmlLogCopyList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "%s/%s-%s.xml", dump_path, fname, my_hostname());
			numXLogs++;
			free(tmp);
		}		
		i++;
	}

		/* add the default log file in case no log file is specified in config */
	tmp = param("LOG");
	if (tmp) {
		snprintf(xmlLogList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "%s/Events.xml", tmp);
		snprintf(xmlLogCopyList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "%s/Events-%s.xml", dump_path, my_hostname());
	} else {
		snprintf(xmlLogList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "Events.xml");
		snprintf(xmlLogCopyList[numXLogs], CONDOR_TT_MAXLOGPATHLEN, "%s/Events-%s.xml", dump_path, my_hostname());
	}
	numXLogs++;	

		/* copy xml log files */	
	for(i=0; i < numXLogs; i++) {
		filexmlobj = new FILEXML(xmlLogList[i], O_CREAT|O_RDWR);

	    if (filexmlobj->file_open() == FAILURE) {
			goto ERROREXIT;
		}

		if (filexmlobj->file_lock() == FAILURE) {
			goto ERROREXIT;
		}		
		
		if (append(xmlLogCopyList[i], xmlLogList[i]) == FAILURE) {
			goto ERROREXIT;
		}

		if(filexmlobj->file_truncate() == FAILURE) {
			goto ERROREXIT;
		}

		if(filexmlobj->file_unlock() == FAILURE) {
			goto ERROREXIT;
		}
		delete filexmlobj;
		filexmlobj = NULL;
	}

	return SUCCESS;

 ERROREXIT:
	if(filexmlobj) {
		delete filexmlobj;
	}
	
	return FAILURE;

}


QuillErrCode TTManager::insertMachines(AttrList *ad) {
	HashTable<MyString, MyString> newClAd(200, attHashFunction, updateDuplicateKeys);
	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;
	char *attName = NULL, *attVal, *attNameList = NULL, 
		*attValList = NULL, *tmpVal = NULL;
	int isFirst = TRUE;
	MyString aName, aVal, temp, machine_id;
	char *inlist = NULL;

	char lastHeardFrom[300] = "";

		// previous LastHeardFrom from the current classad
		// previous LastHeardFrom from the database's machine_classad
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st, len;
	int  attr_type;
	int  num_result = 0;

	ad->sPrint(classAd);

	// Insert stuff into Machine_Classad

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
				/* the attribute name can't be longer than the log entry line 
				   size make sure attName is freed always to prevent memory 
				   leak. 
				*/
			attName = (char *)malloc(strlen(iter));

			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				/* strip quotes from attVal (if any), this way we can 
				   uniformly handle all attributes which are inserted into 
				   the vertical part where attribute value is stored in 
				   text type regardless what it's original data type is
				*/
			stripquotes(attVal);
	
			attr_type = typeOf(attName);

			if (strcasecmp(attName, ATTR_PREV_LAST_HEARD_FROM) == 0) {
				prevLHFInAd = atoi(attVal);
			}
			else if (isHorizontalMachineAttr(attName)) {

					// should go into machine_classad table
				if (isFirst) {
						//is the first in the list
					isFirst = FALSE;
					
					if (strcasecmp(attName, "lastheardfrom") == 0) {
							/* for lastheardfrom, we want to store both 
							   the epoch seconds and the string timestamp
							   value. The epoch seconds is stored in a column
							   named lastheardfrom_epoch.
							*/
						attNameList = (char *) malloc (2*strlen(attName) + 
													   20);

						attValList = (char *) malloc (strlen(attVal) + 200);

						sprintf(attNameList, 
								"(lastheardfrom, lastheardfrom_epoch");

					} else {
							/* 11 is the string length of "machine_id", 5 is 
							   for the overhead for enclosing parenthesis
							*/
						attNameList = (char *) malloc (((strlen(attName) > 11)?
														strlen(attName):11) 
													   + 5);
					
							/* 80 is the extra length needed for converting a 
							   seconds value to timestamp value, 7 is for the 
							   overhead of enclosing parenthesis and quotes
							*/

						attValList = (char *) malloc (strlen(attVal) + 
													  ((attr_type == 
														CONDOR_TT_TYPE_TIMESTAMP)?120:7));

						if (strcasecmp(attName, ATTR_NAME) == 0) {
							sprintf(attNameList, "(machine_id");
							machine_id = attVal;
						} else {
							sprintf(attNameList, "(%s", attName);
						}

					}

					switch (attr_type) {

					case CONDOR_TT_TYPE_STRING:
						sprintf(attValList, "('%s'", attVal);
						break;
					case CONDOR_TT_TYPE_TIMESTAMP:
						time_t clock;
						char *ts_expr;
						clock = atoi(attVal);
						
						ts_expr = condor_ttdb_buildts(&clock, dt);

						if (ts_expr == NULL) {
							dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
							if (attNameList) free(attNameList);
							if (attValList) free(attValList);		 
							if (inlist) free(inlist);					
							return FAILURE;							
						}

						if (strcasecmp(attName, "lastheardfrom") == 0) {
							sprintf(attValList, "(%s, %s", ts_expr, attVal);
							snprintf(lastHeardFrom, 300, "%s", ts_expr);
						} else {
							sprintf(attValList, "(%s", ts_expr);
						}

						free(ts_expr);

						break;
					case CONDOR_TT_TYPE_NUMBER:
						sprintf(attValList, "(%s", attVal);
						break;
					default:
						dprintf(D_ALWAYS, "insertMachines: Unsupported horizontal machine attribute\n");
						if (attNameList) free(attNameList);
						if (attValList) free(attValList);		 
						if (inlist) free(inlist);					
						return FAILURE;
						break;							
					}
				} else {
						// is not the first in the list
					if (strcasecmp(attName, "lastheardfrom") == 0) {
						attNameList = (char *) realloc (attNameList, 
														strlen(attNameList) + 
														2*strlen(attName)+20);
						attValList = (char *) realloc (attValList, 
													   strlen(attValList) + 
													   strlen(attVal) + 200);

						strcat(attNameList, ", ");
						
						strcat(attNameList, 
							   "lastheardfrom, lastheardfrom_epoch");

					} else {
					
						attNameList = (char *) realloc (attNameList, 
														strlen(attNameList) + 
														((strlen(attName) > 11)?strlen(attName):11) + 5);

						attValList = (char *) realloc (attValList, 
													   strlen(attValList) + 
													   strlen(attVal) + 
													   ((attr_type == CONDOR_TT_TYPE_TIMESTAMP)?120:8));

						strcat(attNameList, ", ");

						if (strcasecmp(attName, ATTR_NAME) == 0) {
							strcat(attNameList, "machine_id");
							machine_id = attVal;
						} else {
							strcat(attNameList, attName);
						}
					}

					strcat(attValList, ", ");

					tmpVal = (char  *) malloc(strlen(attVal) + 300);

					switch (attr_type) {

					case CONDOR_TT_TYPE_STRING:
						sprintf(tmpVal, "'%s'", attVal);
						break;
					case CONDOR_TT_TYPE_TIMESTAMP:
						time_t clock;
						char *ts_expr;
						clock = atoi(attVal);

						ts_expr = condor_ttdb_buildts(&clock, dt);

						if (ts_expr == NULL) {
							dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
							if (attNameList) free(attNameList);
							if (attValList) free(attValList);		 
							if (inlist) free(inlist);					
							return FAILURE;							
						}
						
						if (strcasecmp(attName, "lastheardfrom") == 0) {
							sprintf(tmpVal, "%s, %s", ts_expr, attVal);
							snprintf(lastHeardFrom, 300, "%s", ts_expr);
						} else {
							sprintf(tmpVal, "%s", ts_expr);
						}

						free(ts_expr);

						break;
					case CONDOR_TT_TYPE_NUMBER:
						sprintf(tmpVal, "%s", attVal);
						break;
					default:
						dprintf(D_ALWAYS, "insertMachines: Unsupported horizontal machine attribute\n");
						if (attNameList) free(attNameList);
						if (attValList) free(attValList);		 
						if (inlist) free(inlist);					
						return FAILURE;
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

			free(attName);

			iter = classAd.GetNextToken("\n", true);
		}

	if (attNameList) strcat(attNameList, ")");
	if (attValList) strcat(attValList, ")");
	if (inlist) strcat(inlist, ")");

	len = 4096 + strlen(machine_id.Value()) + strlen(lastHeardFrom);
	sql_stmt = (char *) malloc (len);

		// get the previous lastheardfrom from the database 
	snprintf(sql_stmt, len, "SELECT lastheardfrom_epoch FROM Machine_Classad WHERE machine_id = '%s'", machine_id.Value());

	ret_st = DBObj->execQuery(sql_stmt, num_result);
	
	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}
	else if (ret_st == SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0, 0));		
	}
	
	DBObj->releaseQueryResult();

		// set end time if the previous lastHeardFrom matches, otherwise
		// leave it as NULL (by default)
	if (prevLHFInDB == prevLHFInAd) {
		snprintf(sql_stmt, len, "INSERT INTO Machine_Classad_History(machine_id, opsys, arch, ckptserver, ckpt_server_host, state, activity, keyboardidle, consoleidle, loadavg, condorloadavg, totalloadavg, virtualmemory, memory, totalvirtualmemory, cpubusytime, cpuisbusy, rank, currentrank , requirements, clockmin, clockday, lastheardfrom, enteredcurrentactivity, enteredcurrentstate, updatesequencenumber, updatestotal, updatessequenced, updateslost, globaljobid, end_time) SELECT machine_id, opsys, arch, ckptserver, ckpt_server_host, state, activity, keyboardidle, consoleidle, loadavg, condorloadavg, totalloadavg, virtualmemory, memory, totalvirtualmemory, cpubusytime, cpuisbusy, rank, currentrank , requirements, clockmin, clockday, lastheardfrom, enteredcurrentactivity, enteredcurrentstate, updatesequencenumber, updatestotal, updatessequenced, updateslost, globaljobid, %s FROM Machine_Classad WHERE machine_id = '%s'", lastHeardFrom, machine_id.Value());
	} else {
		snprintf(sql_stmt, len, "INSERT INTO Machine_Classad_History (machine_id, opsys, arch, ckptserver, ckpt_server_host, state, activity, keyboardidle, consoleidle, loadavg, condorloadavg, totalloadavg, virtualmemory, memory, totalvirtualmemory, cpubusytime, cpuisbusy, rank, currentrank , requirements, clockmin, clockday, lastheardfrom, enteredcurrentactivity, enteredcurrentstate, updatesequencenumber, updatestotal, updatessequenced, updateslost, globaljobid) SELECT machine_id, opsys, arch, ckptserver, ckpt_server_host, state, activity, keyboardidle, consoleidle, loadavg, condorloadavg, totalloadavg, virtualmemory, memory, totalvirtualmemory, cpubusytime, cpuisbusy, rank, currentrank , requirements, clockmin, clockday, lastheardfrom, enteredcurrentactivity, enteredcurrentstate, updatesequencenumber, updatestotal, updatessequenced, updateslost, globaljobid FROM Machine_Classad WHERE machine_id = '%s'", machine_id.Value());
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

	 len = 100 + strlen(attNameList) + strlen(attValList);
	 sql_stmt = (char *) malloc(len);

	 snprintf(sql_stmt, len, "INSERT INTO Machine_Classad %s VALUES %s", attNameList, attValList);

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
	 len = 1000 + strlen(inlist) + strlen(machine_id.Value()) + strlen(lastHeardFrom);
	 sql_stmt = (char *) malloc (len);

		 // if the previous lastHeardFrom doesn't match, this means the 
		 // daemon has been shutdown for a while, we should move everything
		 // into the machine_history (with a NULL end_time)!
	 if (prevLHFInDB == prevLHFInAd) {
		 snprintf(sql_stmt, len, "INSERT INTO Machine_History (machine_id, attr, val, start_time, end_time) SELECT machine_id, attr, val, start_time, %s FROM Machine WHERE machine_id = '%s' AND attr NOT IN %s", lastHeardFrom, machine_id.Value(), inlist);
	 } else {
		 snprintf(sql_stmt, len, "INSERT INTO Machine_History (machine_id, attr, val, start_time) SELECT machine_id, attr, val, start_time FROM Machine WHERE machine_id = '%s'", machine_id.Value());		 
	 }

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 free(sql_stmt);
	 	 if (inlist) free(inlist);
		 return FAILURE;
	 }
		
	 if (prevLHFInDB == prevLHFInAd) {
		 snprintf(sql_stmt, len, "DELETE FROM Machine WHERE machine_id = '%s' AND attr NOT IN %s", machine_id.Value(), inlist);
	 } else {
		 snprintf(sql_stmt, len, "DELETE FROM Machine WHERE machine_id = '%s'", machine_id.Value());		 
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
		 
		 len = 2048 + 2*strlen(machine_id.Value()) + 2*strlen(aName.Value()) + 
			 strlen(aVal.Value()) + strlen(lastHeardFrom);

	 	 sql_stmt = (char *) malloc (len);

		 snprintf(sql_stmt, len, "INSERT INTO Machine (machine_id, attr, val, start_time) SELECT '%s', '%s', '%s', %s FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM Machine WHERE machine_id = '%s' AND attr = '%s')", machine_id.Value(), aName.Value(), aVal.Value(), lastHeardFrom, machine_id.Value(), aName.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			 free(sql_stmt);
			 return FAILURE;
		 }
			
		 snprintf(sql_stmt, len, "INSERT INTO Machine_History (machine_id, attr, val, start_time, end_time) SELECT machine_id, attr, val, start_time, %s FROM Machine WHERE machine_id = '%s' AND attr = '%s' AND val != '%s'", lastHeardFrom, machine_id.Value(), aName.Value(), aVal.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);		
			 free(sql_stmt);
			 return FAILURE;
		 }

		 snprintf(sql_stmt, len, "UPDATE Machine SET val = '%s', start_time = %s WHERE machine_id = '%s' AND attr = '%s' AND val != '%s'", aVal.Value(), lastHeardFrom, machine_id.Value(), aName.Value(), aVal.Value());

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

QuillErrCode TTManager::insertScheddAd(AttrList *ad) {
	HashTable<MyString, MyString> newClAd(200, attHashFunction, updateDuplicateKeys); // for holding attributes going to vertical table

	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;
	
	char *attName = NULL, *attVal, 
		*attNameList = NULL, *attValList = NULL, 
		*tmpVal = NULL, *attNameList2 = NULL, *attValList2 = NULL;
	int firstScheddAttr = TRUE;
	MyString aName, aVal, temp;
	char *inlist = NULL;
	char lastHeardFrom[300] = "";
	char daemonName[300] = "";

		// previous LastHeardFrom from the current classad
		// previous LastHeardFrom from the database's machine_classad
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st, len;
	int  attr_type;
	int  num_result = 0;

		// first generate MyType='Scheduler' attribute
	attNameList = (char *) malloc (20);
	attValList = (char *) malloc (20);

	sprintf(attNameList, "(MyType");
	sprintf(attValList, "('Scheduler'");

	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			attName = (char *)malloc(strlen(iter));
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				/* strip quotes from attVal (if any), this way we can 
				   uniformly handle all attributes which are inserted into 
				   the vertical part where attribute value is stored in 
				   text type regardless what it's original data type is
				*/
			stripquotes(attVal);

			attr_type = typeOf(attName);

			if (strcasecmp(attName, ATTR_PREV_LAST_HEARD_FROM) == 0) {
				prevLHFInAd = atoi(attVal);
			}

			if (strcasecmp(attName, ATTR_NAME) == 0) {
				sprintf(daemonName, "%s", attVal);
			}
			
				/* notice that the Name and LastHeardFrom are both a 
				   horizontal daemon attribute and horizontal schedd attribute,
				   therefore we need to check both seperately.
				*/
			if (isHorizontalDaemonAttr(attName)) {

				if (strcasecmp(attName, "lastheardfrom") == 0) {
					attNameList = (char *) realloc (attNameList, 
													strlen(attNameList) + 
													2*strlen(attName)+20);

					attValList = (char *) realloc (attValList, 
												   strlen(attValList) + 
												   strlen(attVal) + 200);
					
					strcat(attNameList, ", ");
					
					strcat(attNameList, 
						   "lastheardfrom, lastheardfrom_epoch");
					
				} else {
					attNameList = (char *) realloc (attNameList, 
													strlen(attNameList) + 
													strlen(attName) + 5);

					attValList = (char *) realloc (attValList, 
												   strlen(attValList) + 
												   strlen(attVal) + 
												   ((attr_type == 
													 CONDOR_TT_TYPE_TIMESTAMP)?120:8));

					strcat(attNameList, ", ");
					strcat(attNameList, attName);
				}

				strcat(attValList, ", ");

				tmpVal = (char  *) malloc(strlen(attVal) + 300);

				switch (attr_type) {

				case CONDOR_TT_TYPE_STRING:
					sprintf(tmpVal, "'%s'", attVal);
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					char *ts_expr;
					clock = atoi(attVal);

					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr == NULL) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
						if (attNameList) free(attNameList);
						if (attValList) free(attValList);	
						if (attNameList2) free(attNameList2);
						if (attValList2) free(attValList2);
						if (inlist) free(inlist);					
						return FAILURE;							
					}

					if (strcasecmp(attName, "lastheardfrom") == 0) { 
						sprintf(tmpVal, "%s, %s", ts_expr, attVal);
						snprintf(lastHeardFrom, 300, "%s", ts_expr);
					} else {
						sprintf(tmpVal, "%s", ts_expr);
					}
					
					free(ts_expr);

					break;
				case CONDOR_TT_TYPE_NUMBER:
					sprintf(tmpVal, "%s", attVal);
					break;
				default:
					dprintf(D_ALWAYS, "insertScheddAd: unsupported horizontal daemon attribute\n");
					if (attNameList) free(attNameList);
					if (attValList) free(attValList);		 
					if (attNameList2) free(attNameList2);
					if (attValList2) free(attValList2);	
					if (inlist) free(inlist);					
					return FAILURE;
				}

				strcat(attValList, tmpVal);
					
				free(tmpVal);				
			}

			if (isHorizontalScheddAttr(attName)) {
				if (firstScheddAttr) {
						//is the first in the list
					firstScheddAttr = FALSE;
					
						// adding 5 for the overhead for enclosing parenthesis
					attNameList2 = (char *) malloc (strlen(attName) + 5);
					
						/* 80 is the extra length needed for converting a 
						   seconds value to timestamp value, 7 is for the 
						   overhead of enclosing parenthesis and quotes
						*/
					attValList2 = (char *) malloc (strlen(attVal) + 
												   ((attr_type == 
													 CONDOR_TT_TYPE_TIMESTAMP)?120:7));

					sprintf(attNameList2, "(%s", attName);

					switch (attr_type) {

					case CONDOR_TT_TYPE_STRING:
						sprintf(attValList2, "('%s'", attVal);
						break;
					case CONDOR_TT_TYPE_TIMESTAMP:
						time_t clock;
						char *ts_expr;
						clock = atoi(attVal);

						ts_expr = condor_ttdb_buildts(&clock, dt);

						if (ts_expr == NULL) {
							dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
							if (attNameList) free(attNameList);
							if (attValList) free(attValList);		 
							if (attNameList2) free(attNameList2);
							if (attValList2) free(attValList2);	
							if (inlist) free(inlist);					
							return FAILURE;							
						}
												
						sprintf(attValList2, "(%s", ts_expr);
						
						free(ts_expr);

						break;
					case CONDOR_TT_TYPE_NUMBER:
						sprintf(attValList2, "(%s", attVal);
						break;
					default:
						dprintf(D_ALWAYS, "insertScheddAd: Unsupported horizontal schedd attribute\n");
						if (attNameList) free(attNameList);
						if (attValList) free(attValList);		 
						if (attNameList2) free(attNameList2);
						if (attValList2) free(attValList2);	
						if (inlist) free(inlist);					
						return FAILURE;
					}
				} else {
						// is not the first in the list
					
					attNameList2 = (char *) realloc (attNameList2, 
													 strlen(attNameList2) + 
													 strlen(attName) + 5);
					attValList2 = (char *) realloc (attValList2, 
													strlen(attValList2) + 
													strlen(attVal) + 
													((attr_type == 
													  CONDOR_TT_TYPE_TIMESTAMP)?120:8));

					strcat(attNameList2, ", ");
					strcat(attNameList2, attName);

					strcat(attValList2, ", ");

					tmpVal = (char  *) malloc(strlen(attVal) + 100);

					switch (attr_type) {

					case CONDOR_TT_TYPE_STRING:
						sprintf(tmpVal, "'%s'", attVal);
						break;
					case CONDOR_TT_TYPE_TIMESTAMP:
						time_t clock;
						char *ts_expr;
						clock = atoi(attVal);

						ts_expr = condor_ttdb_buildts(&clock, dt);

						if (ts_expr == NULL) {
							dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
							if (attNameList) free(attNameList);
							if (attValList) free(attValList);		 
							if (attNameList2) free(attNameList2);
							if (attValList2) free(attValList2);
							if (inlist) free(inlist);					
							return FAILURE;							
						}

						sprintf(tmpVal, "%s", ts_expr);

						free(ts_expr);

						break;
					case CONDOR_TT_TYPE_NUMBER:
						sprintf(tmpVal, "%s", attVal);
						break;
					default:
						dprintf(D_ALWAYS, "insertScheddAd: Unsupported horizontal schedd attribute\n");
						if (attNameList) free(attNameList);
						if (attValList) free(attValList);		 
						if (attNameList2) free(attNameList2);
						if (attValList2) free(attValList2);	
						if (inlist) free(inlist);					
						return FAILURE;
					}

					strcat(attValList2, tmpVal);
					
					free(tmpVal);
				}
			}

			if (!isHorizontalScheddAttr(attName) && 
				!isHorizontalDaemonAttr(attName) &&
				strcasecmp(attName, ATTR_PREV_LAST_HEARD_FROM) != 0) {
					// the rest of attributes go to the vertical schedd table
				aName = attName;
				aVal = attVal;

					// insert into new ClassAd to be inserted into DB
				newClAd.insert(aName, aVal);				

					// build an inlist of the vertical attribute names
				if (NULL == inlist) {
					inlist = (char *) malloc (strlen(attName)+5);
					sprintf(inlist, "('%s'", attName);
				} else {
					inlist = (char *) realloc (inlist, strlen(inlist) + 
											   strlen(attName) + 5);
					strcat (inlist, ",'");
					strcat (inlist, attName);
					strcat (inlist, "'");
				}			
			}

			free (attName);
			iter = classAd.GetNextToken("\n", true);
		}

	if (attNameList) strcat(attNameList, ")");
	if (attValList) strcat(attValList, ")");
	if (attNameList2) strcat(attNameList2, ")");
	if (attValList2) strcat(attValList2, ")");
	if (inlist) strcat(inlist, ")");

	len = 2048 + strlen(daemonName) + strlen(lastHeardFrom);
	sql_stmt = (char *) malloc (len);	

		// get the previous lastheardfrom from the database 
	snprintf(sql_stmt, len, "SELECT lastheardfrom_epoch FROM daemon_horizontal WHERE MyType = 'Scheduler' AND Name = '%s'", daemonName);

	ret_st = DBObj->execQuery(sql_stmt, num_result);

	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}
	else if (ret_st == SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0, 0));	   
	}

	DBObj->releaseQueryResult();

		/* move the horizontal daemon attributes tuple to history
		   set end time if the previous lastHeardFrom matches, otherwise
		   leave it as NULL (by default)	
		*/
	if (prevLHFInDB == prevLHFInAd) {
		snprintf(sql_stmt, len, "INSERT INTO Daemon_Horizontal_History (MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, endtime) SELECT MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, %s FROM Daemon_Horizontal WHERE MyType = 'Scheduler' AND Name = '%s'", lastHeardFrom, daemonName);
	} else {
		snprintf(sql_stmt, len, "INSERT INTO Daemon_Horizontal_History (MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory) SELECT MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory FROM Daemon_Horizontal WHERE MyType = 'Scheduler' AND Name = '%s'", daemonName);
	}
	
	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		if (attNameList) free(attNameList);
		if (attValList) free(attValList);		 
		if (attNameList2) free(attNameList2);
		if (attValList2) free(attValList2);	
		if (inlist) free(inlist);
		free(sql_stmt);
		return FAILURE;
	}
	
	snprintf(sql_stmt, len, "DELETE FROM  Daemon_Horizontal WHERE MyType = 'Scheduler' AND Name = '%s'", daemonName);

	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		if (attNameList) free(attNameList);
		if (attValList) free(attValList);		 
		if (attNameList2) free(attNameList2);
		if (attValList2) free(attValList2);		 
		if (inlist) free(inlist);
		free(sql_stmt);
		return FAILURE;
	}

		/* move the horizontal schedd attributes tuple to history
		   set end time if the previous lastHeardFrom matches, otherwise
		   leave it as NULL (by default)	
		*/
	if (prevLHFInDB == prevLHFInAd) {
		snprintf(sql_stmt, len, "INSERT INTO Schedd_Horizontal_History (Name, LastHeardFrom, NumUsers, TotalIdleJobs, TotalRunningJobs, TotalJobAds, TotalHeldJobs, TotalFlockedJobs, TotalRemovedJobs, endtime) SELECT Name, LastHeardFrom, NumUsers, TotalIdleJobs, TotalRunningJobs, TotalJobAds, TotalHeldJobs, TotalFlockedJobs, TotalRemovedJobs, %s FROM Schedd_Horizontal WHERE Name = '%s'", lastHeardFrom, daemonName);
	} else {
		snprintf(sql_stmt, len, "INSERT INTO Schedd_Horizontal_History (Name, LastHeardFrom, NumUsers, TotalIdleJobs, TotalRunningJobs, TotalJobAds, TotalHeldJobs, TotalFlockedJobs, TotalRemovedJobs) SELECT Name, LastHeardFrom, NumUsers, TotalIdleJobs, TotalRunningJobs, TotalJobAds, TotalHeldJobs, TotalFlockedJobs, TotalRemovedJobs FROM Schedd_Horizontal WHERE Name = '%s'", daemonName);
	}
	
	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		if (attNameList) free(attNameList);
		if (attValList) free(attValList);		 
		if (attNameList2) free(attNameList2);
		if (attValList2) free(attValList2);	
		if (inlist) free(inlist);
		free(sql_stmt);
		return FAILURE;
	}
	
	snprintf(sql_stmt, len, "DELETE FROM  Schedd_Horizontal WHERE Name = '%s'", daemonName);

	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		if (attNameList) free(attNameList);
		if (attValList) free(attValList);		 
		if (attNameList2) free(attNameList2);
		if (attValList2) free(attValList2);		 
		if (inlist) free(inlist);
		free(sql_stmt);
		return FAILURE;
	}
	 
	free(sql_stmt);

		// insert new tuple into daemon_horizontal 
	len = 100 + strlen(attNameList) + strlen(attValList);
	sql_stmt = (char *) malloc(len);
	snprintf(sql_stmt, len, "INSERT INTO daemon_horizontal %s VALUES %s", attNameList, attValList);

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 if (attNameList) free(attNameList);
		 if (attValList) free(attValList);
		 if (attNameList2) free(attNameList2);
		 if (attValList2) free(attValList2);		
		 if (inlist) free(inlist);
		 free(sql_stmt);
		 return FAILURE;
	 }	 

	 free(sql_stmt);

		 // insert new tuple into schedd_horizontal 
	 len = 100 + strlen(attNameList2) + strlen(attValList2);
	 sql_stmt = (char *) malloc(len);
	 snprintf(sql_stmt, len, "INSERT INTO schedd_horizontal %s VALUES %s", attNameList2, attValList2);

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 if (attNameList) free(attNameList);
		 if (attValList) free(attValList);
		 if (attNameList2) free(attNameList2);
		 if (attValList2) free(attValList2);		
		 if (inlist) free(inlist);
		 free(sql_stmt);
		 return FAILURE;
	 }	 

	 free(sql_stmt);

	 if (attNameList) free(attNameList);
	 if (attValList) free(attValList);
	 if (attNameList2) free(attNameList2);
	 if (attValList2) free(attValList2);		

		// Make changes into schedd_vertical and schedd_vertical_history
	 len = 1000 + strlen(inlist) + strlen(daemonName) + strlen(lastHeardFrom);
	 sql_stmt = (char *) malloc (len);	 

		 /* if the previous lastHeardFrom doesn't match, this means the 
			daemon has been shutdown for a while, we should move everything
			into the schedd_vertical_history (with a NULL end_time)!
			if the previous lastHeardFrom matches, only move attributes that 
			don't appear in the new class ad
		 */
	 if (prevLHFInDB == prevLHFInAd) {
		 snprintf(sql_stmt, len, "INSERT INTO Schedd_Vertical_History (Name, LastHeardFrom, attr, val, endtime) SELECT name, lastheardfrom, attr, val, %s FROM Schedd_Vertical WHERE name = '%s' AND attr NOT IN %s", lastHeardFrom, daemonName, inlist);
	 } else {
		 snprintf(sql_stmt, len, "INSERT INTO Schedd_Vertical_History (Name, LastHeardFrom, attr, val) SELECT name, lastheardfrom, attr, val FROM Schedd_Vertical WHERE name = '%s'", daemonName);		 
	 }	 

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 free(sql_stmt);
	 	 if (inlist) free(inlist);
		 return FAILURE;
	 }

	 if (prevLHFInDB == prevLHFInAd) {
		 snprintf(sql_stmt, len, "DELETE FROM schedd_vertical WHERE name = '%s' AND attr NOT IN %s", daemonName, inlist);
	 } else {
		 snprintf(sql_stmt, len, "DELETE FROM schedd_vertical WHERE name = '%s'", daemonName);		 
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

		 // insert the vertical attributes
	 newClAd.startIterations();
	 while (newClAd.iterate(aName, aVal)) {
		 len = 2048 + 2*strlen(daemonName) + 2*strlen(aName.Value()) + 
			 strlen(aVal.Value()) + strlen(lastHeardFrom);

		 sql_stmt = (char *) malloc (len);

		 snprintf(sql_stmt, len, "INSERT INTO schedd_vertical (name, attr, val, lastheardfrom) SELECT '%s', '%s', '%s', %s FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM schedd_vertical WHERE name = '%s' AND attr = '%s')", daemonName, aName.Value(), aVal.Value(), lastHeardFrom, daemonName, aName.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			 free(sql_stmt);
			 return FAILURE;
		 }

		 snprintf(sql_stmt, len, "INSERT INTO schedd_vertical_history (name, lastheardfrom, attr, val, endtime) SELECT name, lastheardfrom, attr, val, %s FROM schedd_vertical WHERE name = '%s' AND attr = '%s' AND val != '%s'", lastHeardFrom, daemonName, aName.Value(), aVal.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);		
			 free(sql_stmt);
			 return FAILURE;
		 }

		 snprintf(sql_stmt, len, "UPDATE schedd_vertical SET val = '%s', lastheardfrom = %s WHERE name = '%s' AND attr = '%s' AND val != '%s'", aVal.Value(), lastHeardFrom, daemonName, aName.Value(), aVal.Value());
		 
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

QuillErrCode TTManager::insertMasterAd(AttrList *ad) {
	HashTable<MyString, MyString> newClAd(200, attHashFunction, updateDuplicateKeys); // for holding attributes going to vertical table

	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;
	
	char *attName = NULL, *attVal, 
		*attNameList = NULL, *attValList = NULL, 
		*tmpVal = NULL;
	MyString aName, aVal, temp;
	char *inlist = NULL;
	char lastHeardFrom[300] = "";
	char daemonName[300] = "";

		// previous LastHeardFrom from the current classad
		// previous LastHeardFrom from the database's machine_classad
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st, len;
	int  attr_type;
	int  num_result = 0;

		// first generate MyType='Scheduler' attribute
	attNameList = (char *) malloc (20);
	attValList = (char *) malloc (20);

	sprintf(attNameList, "(MyType");
	sprintf(attValList, "('Master'");

	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			attName = (char *)malloc(strlen(iter));
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				/* strip quotes from attVal (if any), this way we can 
				   uniformly handle all attributes which are inserted into 
				   the vertical part where attribute value is stored in 
				   text type regardless what it's original data type is
				*/
			stripquotes(attVal);

			attr_type = typeOf(attName);

			if (strcasecmp(attName, ATTR_PREV_LAST_HEARD_FROM) == 0) {
				prevLHFInAd = atoi(attVal);
			}
			
			if (strcasecmp(attName, ATTR_NAME) == 0) {
				sprintf(daemonName, "%s", attVal);
			}
			
				/* notice that the Name and LastHeardFrom are both a 
				   horizontal daemon attribute and horizontal schedd attribute,
				   therefore we need to check both seperately.
				*/
			if (isHorizontalDaemonAttr(attName)) {
				if (strcasecmp(attName, "lastheardfrom") == 0) {
					attNameList = (char *) realloc (attNameList, 
													strlen(attNameList) + 
													2*strlen(attName)+20);

					attValList = (char *) realloc (attValList, 
												   strlen(attValList) + 
												   strlen(attVal) + 200);
					
					strcat(attNameList, ", ");
					
					strcat(attNameList, 
						   "lastheardfrom, lastheardfrom_epoch");
					
				} else {
					attNameList = (char *) realloc (attNameList, 
													strlen(attNameList) + 
													strlen(attName) + 5);
					attValList = (char *) realloc (attValList, 
												   strlen(attValList) + 
												   strlen(attVal) + 
												   ((attr_type == 
													 CONDOR_TT_TYPE_TIMESTAMP)?120:8));

					strcat(attNameList, ", ");
					strcat(attNameList, attName);
				}

				strcat(attValList, ", ");

				tmpVal = (char  *) malloc(strlen(attVal) + 300);

				switch (attr_type) {

				case CONDOR_TT_TYPE_STRING:
					sprintf(tmpVal, "'%s'", attVal);
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					char *ts_expr;
					clock = atoi(attVal);

					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr == NULL) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
						if (attNameList) free(attNameList);
						if (attValList) free(attValList);	
						if (inlist) free(inlist);					
						return FAILURE;							
					}

					if (strcasecmp(attName, "lastheardfrom") == 0) { 
						sprintf(tmpVal, "%s, %s", ts_expr, attVal);
						snprintf(lastHeardFrom, 300, "%s", ts_expr);
					} else {
						sprintf(tmpVal, "%s", ts_expr);
					}
					
					free(ts_expr);

					break;
				case CONDOR_TT_TYPE_NUMBER:
					sprintf(tmpVal, "%s", attVal);
					break;
				default:
					dprintf(D_ALWAYS, "insertMasterAd: unsupported horizontal daemon attribute\n");
					if (attNameList) free(attNameList);
					if (attValList) free(attValList);		 
					if (inlist) free(inlist);					
					return FAILURE;
				}

				strcat(attValList, tmpVal);
					
				free(tmpVal);				
			} else if (strcasecmp(attName, ATTR_PREV_LAST_HEARD_FROM) != 0) {
					/* the rest of attributes go to the vertical master
					   table.
					*/
				aName = attName;
				aVal = attVal;

					// insert into new ClassAd to be inserted into DB
				newClAd.insert(aName, aVal);				

					// build an inlist of the vertical attribute names
				if (NULL == inlist) {
					inlist = (char *) malloc (strlen(attName)+5);
					sprintf(inlist, "('%s'", attName);
				} else {
					inlist = (char *) realloc (inlist, strlen(inlist) + 
											   strlen(attName) + 5);
					strcat (inlist, ",'");
					strcat (inlist, attName);
					strcat (inlist, "'");
				}			
			}

			free (attName);
			iter = classAd.GetNextToken("\n", true);
		}
			
	if (attNameList) strcat(attNameList, ")");
	if (attValList) strcat(attValList, ")");
	if (inlist) strcat(inlist, ")");

	len = 2048 + strlen(daemonName) + strlen(lastHeardFrom);
	sql_stmt = (char *) malloc (len);	

		// get the previous lastheardfrom from the database 
	snprintf(sql_stmt, len, "SELECT lastheardfrom_epoch FROM daemon_horizontal WHERE MyType = 'Master' AND Name = '%s'", daemonName);
	
	ret_st = DBObj->execQuery(sql_stmt, num_result);

	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}
	else if (ret_st == SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0, 0));		
	}

	DBObj->releaseQueryResult();

		/* move the horizontal daemon attributes tuple to history
		   set end time if the previous lastHeardFrom matches, otherwise
		   leave it as NULL (by default)	
		*/
	if (prevLHFInDB == prevLHFInAd) {
		snprintf(sql_stmt, len, "INSERT INTO Daemon_Horizontal_History (MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, endtime) SELECT MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, %s FROM Daemon_Horizontal WHERE MyType = 'Master' AND Name = '%s'", lastHeardFrom, daemonName);
	} else {
		snprintf(sql_stmt, len, "INSERT INTO Daemon_Horizontal_History (MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory) SELECT MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory FROM Daemon_Horizontal WHERE MyType = 'Master' AND Name = '%s'", daemonName);
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

	snprintf(sql_stmt, len, "DELETE FROM  Daemon_Horizontal WHERE MyType = 'Master' AND Name = '%s'", daemonName);

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

		// insert new tuple into daemon_horizontal 
	len = 100 + strlen(attNameList) + strlen(attValList);
	sql_stmt = (char *) malloc(len);
	snprintf(sql_stmt, len, "INSERT INTO daemon_horizontal %s VALUES %s", attNameList, attValList);
	
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
	
		// Make changes into master_vertical and master_vertical_history	
	len = 1000 + strlen(inlist) + strlen(daemonName) + strlen(lastHeardFrom);
	sql_stmt = (char *) malloc (len);	 

		/* if the previous lastHeardFrom doesn't match, this means the 
		   daemon has been shutdown for a while, we should move everything
		   into the schedd_vertical_history (with a NULL end_time)!
		   if the previous lastHeardFrom matches, only move attributes that 
		   don't appear in the new class ad
		*/
	 if (prevLHFInDB == prevLHFInAd) {
		 snprintf(sql_stmt, len, "INSERT INTO Master_Vertical_History (name, lastheardfrom, attr, val, endtime) SELECT name, lastheardfrom, attr, val, %s FROM Master_Vertical WHERE name = '%s' AND attr NOT IN %s", lastHeardFrom, daemonName, inlist);
	 } else {
		 snprintf(sql_stmt, len, "INSERT INTO Master_Vertical_History (Name, LastHeardFrom, attr, val) SELECT name, lastheardfrom, attr, val FROM Master_Vertical WHERE name = '%s'", daemonName);
	 } 

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 free(sql_stmt);
	 	 if (inlist) free(inlist);
		 return FAILURE;
	 }

	 if (prevLHFInDB == prevLHFInAd) {
		 snprintf(sql_stmt, len, "DELETE FROM master_vertical WHERE name = '%s' AND attr NOT IN %s", daemonName, inlist);
	 } else {
		 snprintf(sql_stmt, len, "DELETE FROM master_vertical WHERE name = '%s'", daemonName);
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

		 // insert the vertical attributes
	 newClAd.startIterations();
	 while (newClAd.iterate(aName, aVal)) {
		 len = 2048 + 2*strlen(daemonName) + 2*strlen(aName.Value()) + strlen(aVal.Value()) + strlen(lastHeardFrom);

		 sql_stmt = (char *) malloc (len);
		 
		 snprintf(sql_stmt, len, "INSERT INTO master_vertical (name, attr, val, lastheardfrom) SELECT '%s', '%s', '%s', %s FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM master_vertical WHERE name = '%s' AND attr = '%s')", daemonName, aName.Value(), aVal.Value(), lastHeardFrom, daemonName, aName.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			 free(sql_stmt);
			 return FAILURE;
		 }	 

		 snprintf(sql_stmt, len, "INSERT INTO master_vertical_history (name, lastheardfrom, attr, val, endtime) SELECT name, lastheardfrom, attr, val, %s FROM master_vertical WHERE name = '%s' AND attr = '%s' AND val != '%s'", lastHeardFrom, daemonName, aName.Value(), aVal.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);		
			 free(sql_stmt);
			 return FAILURE;
		 }

		 snprintf(sql_stmt, len, "UPDATE master_vertical SET val = '%s', lastheardfrom = %s WHERE name = '%s' AND attr = '%s' AND val != '%s'", aVal.Value(), lastHeardFrom, daemonName, aName.Value(), aVal.Value());
		 
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

QuillErrCode TTManager::insertNegotiatorAd(AttrList *ad) {
	HashTable<MyString, MyString> newClAd(200, attHashFunction, updateDuplicateKeys); // for holding attributes going to vertical table

	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;
	
	char *attName = NULL, *attVal, 
		*attNameList = NULL, *attValList = NULL, 
		*tmpVal = NULL;
	MyString aName, aVal, temp;
	char *inlist = NULL;
	char lastHeardFrom[300] = "";
	char daemonName[300] = "";

		// previous LastHeardFrom from the current classad
		// previous LastHeardFrom from the database's machine_classad
    int  prevLHFInAd = 0;
    int  prevLHFInDB = 0;
	int	 ret_st, len;
	int  attr_type;
	int  num_result = 0;
	
		// first generate MyType='Scheduler' attribute
	attNameList = (char *) malloc (20);
	attValList = (char *) malloc (20);

	sprintf(attNameList, "(MyType");
	sprintf(attValList, "('Negotiator'");

	ad->sPrint(classAd);

	classAd.Tokenize();
	iter = classAd.GetNextToken("\n", true);

	while (iter != NULL)
		{
			attName = (char *)malloc(strlen(iter));
			sscanf(iter, "%s =", attName);
			attVal = strstr(iter, "= ");
			attVal += 2;

				/* strip quotes from attVal (if any), this way we can 
				   uniformly handle all attributes which are inserted into 
				   the vertical part where attribute value is stored in 
				   text type regardless what it's original data type is
				*/
			stripquotes(attVal);

			attr_type = typeOf(attName);

			if (strcasecmp(attName, ATTR_PREV_LAST_HEARD_FROM) == 0) {
				prevLHFInAd = atoi(attVal);
			}

			if (strcasecmp(attName, ATTR_NAME) == 0) {
				sprintf(daemonName, "%s", attVal);
			}
			
				/* notice that the Name and LastHeardFrom are both a 
				   horizontal daemon attribute and horizontal schedd attribute,
				   therefore we need to check both seperately.
				*/
			if (isHorizontalDaemonAttr(attName)) {
				if (strcasecmp(attName, "lastheardfrom") == 0) {
					attNameList = (char *) realloc (attNameList, 
													strlen(attNameList) + 
													2*strlen(attName)+20);

					attValList = (char *) realloc (attValList, 
												   strlen(attValList) + 
												   strlen(attVal) + 200);
					
					strcat(attNameList, ", ");
					
					strcat(attNameList, 
						   "lastheardfrom, lastheardfrom_epoch");
					
				} else {
					attNameList = (char *) realloc (attNameList, 
													strlen(attNameList) + 
													strlen(attName) + 5);
					attValList = (char *) realloc (attValList, 
												   strlen(attValList) + 
												   strlen(attVal) + 
												   ((attr_type == 
													 CONDOR_TT_TYPE_TIMESTAMP)?120:8));

					strcat(attNameList, ", ");
					strcat(attNameList, attName);
				}

				strcat(attValList, ", ");

				tmpVal = (char  *) malloc(strlen(attVal) + 300);

				switch (attr_type) {

				case CONDOR_TT_TYPE_STRING:
					sprintf(tmpVal, "'%s'", attVal);
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					char *ts_expr;
					clock = atoi(attVal);

					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr == NULL) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built\n");
						if (attNameList) free(attNameList);
						if (attValList) free(attValList);	
						if (inlist) free(inlist);
						return FAILURE;
					}

					if (strcasecmp(attName, "lastheardfrom") == 0) {
						sprintf(tmpVal, "%s, %s", ts_expr, attVal);
						snprintf(lastHeardFrom, 300, "%s", ts_expr);
					} else {
						sprintf(tmpVal, "%s", ts_expr);
					}
					
					free(ts_expr);					

					break;
				case CONDOR_TT_TYPE_NUMBER:
					sprintf(tmpVal, "%s", attVal);
					break;
				default:
					dprintf(D_ALWAYS, "insertNegotiatorAd: unsupported horizontal daemon attribute\n");
					if (attNameList) free(attNameList);
					if (attValList) free(attValList);		 
					if (inlist) free(inlist);					
					return FAILURE;
				}

				strcat(attValList, tmpVal);
					
				free(tmpVal);				
			} else if (strcasecmp(attName, ATTR_PREV_LAST_HEARD_FROM) != 0) {
					/* the rest of attributes go to the vertical negotiator 
					   table.
					*/
				aName = attName;
				aVal = attVal;

					// insert into new ClassAd to be inserted into DB
				newClAd.insert(aName, aVal);				

					// build an inlist of the vertical attribute names
				if (NULL == inlist) {
					inlist = (char *) malloc (strlen(attName)+5);
					sprintf(inlist, "('%s'", attName);
				} else {
					inlist = (char *) realloc (inlist, strlen(inlist) + 
											   strlen(attName) + 5);
					strcat (inlist, ",'");
					strcat (inlist, attName);
					strcat (inlist, "'");
				}			
			}

			free (attName);
			iter = classAd.GetNextToken("\n", true);
		}	

	if (attNameList) strcat(attNameList, ")");
	if (attValList) strcat(attValList, ")");
	if (inlist) strcat(inlist, ")");

	len = 2048 + strlen(daemonName) + strlen(lastHeardFrom);
	sql_stmt = (char *) malloc (len);	

		// get the previous lastheardfrom from the database 
	snprintf(sql_stmt, len, "SELECT lastheardfrom_epoch FROM daemon_horizontal WHERE MyType = 'Negotiator' AND Name = '%s'", daemonName);
	
	ret_st = DBObj->execQuery(sql_stmt, num_result);

	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}
	else if (ret_st == SUCCESS && num_result > 0) {
		prevLHFInDB = atoi(DBObj->getValue(0, 0));		
	}

	DBObj->releaseQueryResult();

		/* move the horizontal daemon attributes tuple to history
		   set end time if the previous lastHeardFrom matches, otherwise
		   leave it as NULL (by default)	
		*/
	if (prevLHFInDB == prevLHFInAd) {
		snprintf(sql_stmt, len, "INSERT INTO Daemon_Horizontal_History (MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, endtime) SELECT MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory, %s FROM Daemon_Horizontal WHERE MyType = 'Negotiator' AND Name = '%s'", lastHeardFrom, daemonName);
	} else {
		snprintf(sql_stmt, len, "INSERT INTO Daemon_Horizontal_History (MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory) SELECT MyType, Name, LastHeardFrom, MonitorSelfTime, MonitorSelfCPUUsage, MonitorSelfImageSize, MonitorSelfResidentSetSize, MonitorSelfAge, UpdateSequenceNumber, UpdatesTotal, UpdatesSequenced, UpdatesLost, UpdatesHistory FROM Daemon_Horizontal WHERE MyType = 'Negotiator' AND Name = '%s'", daemonName);
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

	snprintf(sql_stmt, len, "DELETE FROM  Daemon_Horizontal WHERE MyType = 'Negotiator' AND Name = '%s'", daemonName);

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

		// insert new tuple into daemon_horizontal 
	len = 100 + strlen(attNameList) + strlen(attValList);
	sql_stmt = (char *) malloc(len);
	snprintf(sql_stmt, len, "INSERT INTO daemon_horizontal %s VALUES %s", attNameList, attValList);
	
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
	
		/* Make changes into negotiator_vertical and 
		   negotiator_vertical_history
		*/
	len = 1000 + strlen(inlist) + strlen(daemonName) + strlen(lastHeardFrom);
	sql_stmt = (char *) malloc (len);	 

		/* if the previous lastHeardFrom doesn't match, this means the 
		   daemon has been shutdown for a while, we should move everything
		   into the schedd_vertical_history (with a NULL end_time)!
		   if the previous lastHeardFrom matches, only move attributes that 
		   don't appear in the new class ad
		*/
	 if (prevLHFInDB == prevLHFInAd) {
		 snprintf(sql_stmt, len, "INSERT INTO Negotiator_Vertical_History (Name, LastHeardFrom, attr, val, endtime) SELECT name, lastheardfrom, attr, val, %s FROM Negotiator_Vertical WHERE name = '%s' AND attr NOT IN %s", lastHeardFrom, daemonName, inlist);
	 } else {
		 snprintf(sql_stmt, len, "INSERT INTO Negotiator_Vertical_History (Name, LastHeardFrom, attr, val) SELECT name, lastheardfrom, attr, val FROM Negotiator_Vertical WHERE name = '%s'", daemonName);
	 }

	 if (DBObj->execCommand(sql_stmt) == FAILURE) {
		 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		 free(sql_stmt);
	 	 if (inlist) free(inlist);
		 return FAILURE;
	 }

	 if (prevLHFInDB == prevLHFInAd) {
		 snprintf(sql_stmt, len, "DELETE FROM negotiator_vertical WHERE name = '%s' AND attr NOT IN %s", daemonName, inlist);
	 } else {
		 snprintf(sql_stmt, len, "DELETE FROM negotiator_vertical WHERE name = '%s'", daemonName);
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

		 // insert the vertical attributes
	 newClAd.startIterations();
	 while (newClAd.iterate(aName, aVal)) {
		 len = 2048 + 2*strlen(daemonName) + 2*strlen(aName.Value()) + strlen(aVal.Value()) + strlen(lastHeardFrom);
		 sql_stmt = (char *) malloc (len);

		 snprintf(sql_stmt, len, "INSERT INTO negotiator_vertical (name, attr, val, lastheardfrom) SELECT '%s', '%s', '%s', %s FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM negotiator_vertical WHERE name = '%s' AND attr = '%s')", daemonName, aName.Value(), aVal.Value(), lastHeardFrom, daemonName, aName.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
			 free(sql_stmt);
			 return FAILURE;
		 }	 

		 snprintf(sql_stmt, len, "INSERT INTO negotiator_vertical_history (name, lastheardfrom, attr, val, endtime) SELECT name, lastheardfrom, attr, val, %s FROM negotiator_vertical WHERE name = '%s' AND attr = '%s' AND val != '%s'", lastHeardFrom, daemonName, aName.Value(), aVal.Value());

		 if (DBObj->execCommand(sql_stmt) == FAILURE) {
			 dprintf(D_ALWAYS, "Executing Statement --- Error\n");
			 dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);		
			 free(sql_stmt);
			 return FAILURE;
		 }

		 snprintf(sql_stmt, len, "UPDATE negotiator_vertical SET val = '%s', lastheardfrom = %s WHERE name = '%s' AND attr = '%s' AND val != '%s'", aVal.Value(), lastHeardFrom, daemonName, aName.Value(), aVal.Value());
		 
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

	char *attName = NULL, *attVal, *attNameList = NULL, *attValList = NULL;
	int isFirst = TRUE;
	bool isMatches = FALSE, isRejects = FALSE, 
		isThrown = FALSE, isGeneric = FALSE;

	if (strcasecmp(tableName, "Matches") == 0) 
		isMatches = TRUE;

	else if (strcasecmp(tableName, "Rejects") == 0) 
		isRejects = TRUE;
        
	else if (strcasecmp(tableName, "Thrown") == 0) 
		isThrown = TRUE;

	else if (strcasecmp(tableName, "Generic") == 0) 
		isGeneric = TRUE;

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

			if ((isMatches && 
				 (strcasecmp(attName, "match_time") == 0)) ||
				(isRejects && 
				 (strcasecmp(attName, "reject_time") == 0)) ||
				(isThrown && 
				 (strcasecmp(attName, "throwtime") == 0)) ||
				(isGeneric && 
				 (strcasecmp(attName, "eventtime") == 0))) {
					/* all timestamp value must be passed as an integer of 
					   seconds from epoch time.
					*/
				time_t clock;

				clock = atoi(attVal);

				newvalue = condor_ttdb_buildts(&clock, dt);

				if (newvalue == NULL) {
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertBasic\n");
					if (attNameList) free(attNameList);
					if (attValList) free(attValList);						
					free(attName);
					return FAILURE;							
				}	
				
			} else {
					/* for other values, check if it contains escape char 
					   escape single quote if any within the value
					*/
				newvalue = fillEscapeCharacters(attVal);
		
					// change double quotes to single quote if any
				attValLen = strlen(newvalue);
 
				if (newvalue[attValLen-1] == '"')
					newvalue[attValLen-1] = '\'';

				if (newvalue[0] == '"') {
					newvalue[0] = '\'';
				}							
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

	sprintf(sql_stmt, "INSERT INTO %s %s VALUES %s", tableName, attNameList, attValList);

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

QuillErrCode TTManager::insertRuns(AttrList *ad) {
	char *sql_stmt = NULL;
	MyString classAd;
	const char *iter;	
	char *newvalue;

	char *attName = NULL, *attVal, *attNameList = NULL, *attValList = NULL;

	char *runid_expr;

		// first generate runid attribute
	attNameList = (char *) malloc (20);
	attValList = (char *) malloc (50);

	runid_expr = condor_ttdb_buildseq(dt, "SeqRunId");

	if (!runid_expr) {
		dprintf(D_ALWAYS, "Sequence expression not build in TTManager::insertRuns\n");
		if (attNameList) free(attNameList);
		if (attValList) free(attValList);			
		return FAILURE;
	}

	sprintf(attNameList, "(run_id");
	sprintf(attValList, "(%s", runid_expr);

	free(runid_expr);

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

			if ((strcasecmp(attName, "startts") == 0) || 
				(strcasecmp(attName, "endts") == 0)) {
				time_t clock;
				clock = atoi(attVal);
				newvalue = condor_ttdb_buildts(&clock, dt);

				if (newvalue == NULL) {
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertRuns\n");
					if (attNameList) free(attNameList);
					if (attValList) free(attValList);						
					free(attName);
					return FAILURE;
				}
				
			} else {

					// escape single quote if any within the value
				newvalue = fillEscapeCharacters(attVal);
		
					// change double quotes to single quote if any
				attValLen = strlen(newvalue);
 
				if (newvalue[attValLen-1] == '"')
					newvalue[attValLen-1] = '\'';
				
				if (newvalue[0] == '"') {
					newvalue[0] = '\'';
				}			
			}

				// is not the first in the list
			attNameList = (char *) realloc(attNameList, strlen(attNameList) + strlen(attName) + 5);
			attValList = (char *) realloc(attValList, strlen(attValList) + strlen(newvalue) + 5);
				
			strcat(attNameList, ", ");
			strcat(attNameList, attName);
			strcat(attValList, ", ");
			strcat(attValList, newvalue);					

			free(newvalue);
			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	if (attNameList) strcat(attNameList, ")");
	if (attValList) strcat(attValList, ")");


	sql_stmt = (char *) malloc (60 + strlen(attNameList) + strlen(attValList));

	sprintf(sql_stmt, "INSERT INTO Runs %s VALUES %s", attNameList, attValList);

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
		eventts[300] = "", messagestr[512] = "";
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

			if (strcasecmp(attName, "eventtime") == 0) {
				time_t clock;
				clock = atoi(attVal);
				newvalue = condor_ttdb_buildts(&clock, dt);

				if (newvalue == NULL) {
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertEvents\n");
					free(attName);
					return FAILURE;
				}
				
			} else {
					// escape single quote if any within the value
				newvalue = fillEscapeCharacters(attVal);

					// change double quotes to single quote if any
				attValLen = strlen(newvalue);
 
				if (newvalue[attValLen-1] == '"')
					newvalue[attValLen-1] = '\'';

				if (newvalue[0] == '"') {
					newvalue[0] = '\'';
				}
			}

			if (strcasecmp(attName, "scheddname") == 0) {
				strcpy(scheddname, newvalue);
			} else if (strcasecmp(attName, "cluster_id") == 0) {
				strcpy(cluster, newvalue);
			} else if (strcasecmp(attName, "proc") == 0) {
				strcpy(proc, newvalue);
			} else if (strcasecmp(attName, "spid") == 0) {
				strcpy(subproc, newvalue);
			} else if (strcasecmp(attName, "eventtype") == 0) {
				eventtype = atoi(newvalue);
			} else if (strcasecmp(attName, "eventtime") == 0) {
				strcpy(eventts, newvalue);
			} else if (strcasecmp(attName, "description") == 0) {
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
		sprintf(sql_stmt, "INSERT INTO events (scheddname, cluster_id, proc, eventtype, eventtime, description) VALUES (%s, %s, %s, %d, %s, %s)", 
				scheddname, cluster, proc, eventtype, eventts, messagestr);
	} else {
		sprintf(sql_stmt, "INSERT INTO events (scheddname, cluster_id, proc, runid, eventtype, eventtime, description) SELECT %s, %s, %s, run_id, %d, %s, %s  FROM runs WHERE scheddname = %s  AND cluster_id = %s and proc = %s AND spid = %s AND endtype is null", scheddname, cluster, proc, eventtype, eventts, messagestr, scheddname, cluster, proc, subproc);
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
	char *seqexpr;

	char f_name[_POSIX_PATH_MAX] = "", f_host[50] = "", 
		f_path[_POSIX_PATH_MAX] = "", f_ts[30] = "";
	int f_size;
	char pathname[_POSIX_PATH_MAX] = "";
	char hexSum[MAC_SIZE*2+1] = "", sum[MAC_SIZE+1] = "";	
	int len;
	char *tmp1, *tmpVal = NULL;
	bool fileSame = TRUE;
	struct stat file_status;
	time_t old_ts;
	char *ts_expr;

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

			if (strcasecmp(attName, "f_name") == 0) {
				strcpy(f_name, attVal);
			} else if (strcasecmp(attName, "f_host") == 0) {
				strcpy(f_host, attVal);
			} else if (strcasecmp(attName, "f_path") == 0) {
				strcpy(f_path, attVal);
			} else if (strcasecmp(attName, "f_ts") == 0) {
				strcpy(f_ts, attVal);
			} else if (strcasecmp(attName, "f_size") == 0) {
				f_size = atoi(attVal);
			}
			
			free(attName);

			iter = classAd.GetNextToken("\n", true);
		}

		/* build timestamp expression */
	old_ts = atoi(f_ts);
	ts_expr = condor_ttdb_buildts(&old_ts, dt);	

	if (ts_expr == NULL) {
		dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertFiles\n");

		return FAILURE;
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
		if (old_ts != file_status.st_mtime) {
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

	sql_stmt = (char *)malloc(2048 + 2*(strlen(f_name)+strlen(f_host)+strlen(f_path)+strlen(f_ts)) +
							  strlen(hexSum));

	seqexpr = condor_ttdb_buildseq(dt, "condor_seqfileid");

	if (!seqexpr) {
		dprintf(D_ALWAYS, "Sequence expression not built in TTManager::insertFiles\n");
		free(sql_stmt);
		return FAILURE;
	}

	sprintf(sql_stmt, 
			"INSERT INTO files (file_id, name, host, path, lastmodified, file_size, checksum) SELECT %s, '%s', %s, '%s', %s, %d, '%s' FROM dummy_single_row_table WHERE NOT EXISTS (SELECT * FROM files WHERE  name='%s' and path='%s' and host=%s and lastmodified=%s)", seqexpr, f_name, f_host, f_path, ts_expr, f_size, hexSum, f_name, f_path, f_host, ts_expr);

	free(seqexpr);
	free(ts_expr);
	
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

	time_t clock;
	char *ts_expr;
	char *onerow_expr;

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

			if (strcasecmp(attName, "f_name") == 0) {
				strcpy(f_name, attVal);
			} else if (strcasecmp(attName, "f_host") == 0) {
				strcpy(f_host, attVal);
			} else if (strcasecmp(attName, "f_path") == 0) {
				strcpy(f_path, attVal);
			} else if (strcasecmp(attName, "f_ts") == 0) {
				strcpy(f_ts, attVal);
			} else if (strcasecmp(attName, "f_size") == 0) {
				f_size = atoi(attVal);
			} else if (strcasecmp(attName, "globalJobId") == 0) {
				strcpy(globaljobid, attVal);
			} else if (strcasecmp(attName, "type") == 0) {
				strcpy(type, attVal);
			}

			free(attName);
			iter = classAd.GetNextToken("\n", true);
		}

	clock = atoi(f_ts);
	ts_expr = condor_ttdb_buildts(&clock, dt);	

	if (ts_expr == NULL) {
		dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertFileusages\n");

		return FAILURE;
	}

	onerow_expr = condor_ttdb_onerow_clause(dt);	

	if (onerow_expr == NULL) {
		dprintf(D_ALWAYS, "ERROR: LIMIT 1 expression not built in TTManager::insertFileusages\n");

		free(ts_expr);

		return FAILURE;
	}

	sql_stmt = (char *) malloc (1000+strlen(globaljobid)+strlen(type)+strlen(f_name)+
								strlen(f_path)+strlen(f_host)+strlen(ts_expr));
	sprintf(sql_stmt, 
			"INSERT INTO fileusages (globaljobid, file_id, usagetype) SELECT %s, file_id, %s FROM files WHERE  name=%s and path=%s and host=%s and lastmodified=%s %s", globaljobid, type, f_name, f_path, f_host, ts_expr, onerow_expr);
	
	free(ts_expr);
	free(onerow_expr);

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
  char       *sql_stmt2 = NULL;
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
  sql_stmt2 = (char *)malloc(1000 + 2*(strlen(scheddname) + 20));

  sprintf(sql_stmt,
          "DELETE FROM History_Horizontal WHERE scheddname = '%s' AND cluster_id = %d AND proc = %d", scheddname, cid, pid);
  sprintf(sql_stmt2,
          "INSERT INTO History_Horizontal(scheddname, cluster_id, proc, enteredhistorytable) VALUES('%s', %d, %d, current_timestamp)", scheddname, cid, pid);

  if (DBObj->execCommand(sql_stmt) == FAILURE) {
	  dprintf(D_ALWAYS, "Executing Statement --- Error\n");
	  dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
	  free(sql_stmt);
	  free(sql_stmt2);
	  return FAILURE;	  
  }

  if (DBObj->execCommand(sql_stmt2) == FAILURE) {
	  dprintf(D_ALWAYS, "Executing Statement --- Error\n");
	  dprintf(D_ALWAYS, "sql = %s\n", sql_stmt2);
	  free(sql_stmt);
	  free(sql_stmt2);
	  return FAILURE;	  
  }
  
  free(sql_stmt);
  free(sql_stmt2);

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

		  /* the following are to avoid overwriting the attr values. The hack 
			 is based on the fact that an attribute of a job ad comes before 
			 the attribute of a cluster ad. And this is because 
		     attribute list of cluster ad is chained to a job ad.
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

			  // don't strip the double quotes
/*
		  if (strcasecmp(name, "user_j") == 0) {
			  tempvalue = (char *)malloc(strlen(value));
			  strncpy(tempvalue, value+1, strlen(value)-2);
			  tempvalue[strlen(value)-2] = '\0';
			  strcpy(value, tempvalue);
			  free(tempvalue);
		  }
	
*/
  
		  sql_stmt = (char *) malloc(1000 + strlen(name) + strlen(value) + strlen(scheddname));
		  sql_stmt2 = NULL;

/*
		  if(strcasecmp(name, "qdate") == 0 || 
			 strcasecmp(name, "lastmatchtime") == 0 || 
			 strcasecmp(name, "jobstartdate") == 0 || 
			 strcasecmp(name, "jobcurrentstartdate") == 0 ||
			 strcasecmp(name, "enteredcurrentstatus") == 0 ||
			 strcasecmp(name, "completiondate") == 0
			 ) {
*/
		  if(strcasecmp(name, "lastmatchtime") == 0 || 
			 strcasecmp(name, "jobstartdate") == 0 || 
			 strcasecmp(name, "jobcurrentstartdate") == 0 ||
			 strcasecmp(name, "enteredcurrentstatus") == 0
			 ) {
				  // avoid updating with epoch time
			  if (strcmp(value, "0") == 0) {
				  free(name);
				  free(value);
				  continue;
			  } 
			
			  time_t clock;
			  char *ts_expr;
			  clock = atoi(value);
			  
			  ts_expr = condor_ttdb_buildts(&clock, dt);	
				  
			  sprintf(sql_stmt,
					  "UPDATE History_Horizontal SET %s = (%s) WHERE scheddname = '%s' and cluster_id = %d and proc = %d", name, ts_expr, scheddname, cid, pid);
			  free(ts_expr);

		  }	else {
				  //strip_double_quote(value);
			  newvalue = fillEscapeCharacters(value);
			  sprintf(sql_stmt, 
					  "UPDATE History_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cluster_id = %d and proc = %d", name, newvalue, scheddname, cid, pid);			  
			  free(newvalue);
		  }
	  } else {
			  //strip_double_quote(value);                
		  newvalue = fillEscapeCharacters(value);
		  
		  sql_stmt = (char *) malloc(1000+2*(strlen(scheddname) + strlen(name) + strlen(newvalue)));
		  sql_stmt2 = (char *) malloc(1000+2*(strlen(scheddname) + strlen(name) + strlen(newvalue)));

		  sprintf(sql_stmt, 
				  "DELETE FROM History_Vertical WHERE scheddname = '%s' AND cluster_id = %d AND proc = %d AND attr = '%s'", scheddname, cid, pid, name);
			  
		  sprintf(sql_stmt2, 
				  "INSERT INTO History_Vertical(scheddname, cluster_id, proc, attr, val) VALUES('%s', %d, %d, '%s', '%s')", scheddname, cid, pid, name, newvalue);

		  free(newvalue);
	  }	  

	  if (DBObj->execCommand(sql_stmt) == FAILURE) {
		  dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		  dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		  
		  free(name);
		  free(value);
		  free(sql_stmt);
		  if (sql_stmt2) free(sql_stmt2);
		  
		  return FAILURE;
	  }
		  
	  if (sql_stmt2 && (DBObj->execCommand(sql_stmt2) == FAILURE)) {
		  dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		  dprintf(D_ALWAYS, "sql = %s\n", sql_stmt2);
		  
		  free(name);
		  free(value);
		  free(sql_stmt);
		  if (sql_stmt2) free(sql_stmt2);
		  
		  return FAILURE;			  
	  }
	  
	  free(name);
	  name = NULL;
	  free(value);
	  value = NULL;
	  free(sql_stmt);
	  if (sql_stmt2) free(sql_stmt2);
  }  

  return SUCCESS;
}

QuillErrCode TTManager::insertTransfers(AttrList *ad) {
  char * sql_stmt = NULL;
  MyString classAd;
  const char *iter;
  char *attName = NULL, *attVal;

  char globaljobid[100];
  char src_name[_POSIX_PATH_MAX] = "", src_host[50] = "",
    src_path[_POSIX_PATH_MAX] = "";
  char dst_name[_POSIX_PATH_MAX] = "", dst_host[50] = "",
    dst_path[_POSIX_PATH_MAX] = "";
  char path[_POSIX_PATH_MAX] = "", name[_POSIX_PATH_MAX] = "";
  char pathname[2*_POSIX_PATH_MAX];
  char dst_daemon[15];
  char f_ts[30];
  time_t old_ts;
  char *last_modified;
  int transfer_size, elapsed;
  char *tmp1, *tmpVal = NULL;
  int len, f_size;
  bool fileSame = TRUE;
  struct stat file_status;
  char hexSum[MAC_SIZE*2+1] = "", sum[MAC_SIZE+1] = "";
  
  ad->sPrint(classAd);

  classAd.Tokenize();
  iter = classAd.GetNextToken("\n", true);

  while (iter != NULL) {
    int attValLen;

    // the attribute name can't be longer than the log entry line size
    attName = (char *)malloc(strlen(iter));
    sscanf(iter, "%s =", attName);
    attVal = strstr(iter, "= ");
    attVal += 2;
    attValLen = strlen(attVal);

    // change double quotes to single quote if any
    if (attVal[attValLen-1] == '"') {
      attVal[attValLen-1] = '\'';
    }
    if (attVal[0] == '"') {
      attVal[0] = '\'';
    }

    if (strcasecmp(attName, "globaljobid") == 0) {
      strncpy(globaljobid, attVal, 100);
    } else if (strcasecmp(attName, "src_name") == 0) {
      strncpy(src_name, attVal, _POSIX_PATH_MAX);
    } else if (strcasecmp(attName, "src_host") == 0) {
      strncpy(src_host, attVal, 50);
    } else if (strcasecmp(attName, "src_path") == 0) {
      strncpy(src_path, attVal, _POSIX_PATH_MAX);
    } else if (strcasecmp(attName, "dst_name") == 0) {
      strncpy(dst_name, attVal, _POSIX_PATH_MAX);
    } else if (strcasecmp(attName, "dst_host") == 0) {
      strncpy(dst_host, attVal, 50);
    } else if (strcasecmp(attName, "dst_path") == 0) {
      strncpy(dst_path, attVal, _POSIX_PATH_MAX);
    } else if (strcasecmp(attName, "dst_daemon") == 0) {
      strncpy(dst_daemon, attVal, 15);
    } else if (strcasecmp(attName, "f_ts") == 0) {
      strncpy(f_ts, attVal, 30);
    } else if (strcasecmp(attName, "transfer_size") == 0) {
      transfer_size = atoi(attVal);
    } else if (strcasecmp(attName, "elapsed") == 0) {
      elapsed = atoi(attVal);
    }

    free(attName);
    iter = classAd.GetNextToken("\n", true);
  }

  // Build timestamp expression
  old_ts = atoi(f_ts);
  last_modified = condor_ttdb_buildts(&old_ts, dt);

  if (last_modified == NULL) {
    dprintf(D_ALWAYS, "ERROR: Timestamp expression not build in TTManager::insertTransfers\n");
    return FAILURE;
  }
  
  // Compute the checksum
  // We don't want to use the file on the starter side since it is
  // a temporary file
  if(strcmp(dst_daemon, "STARTER") == 0)  {
    strncpy(path, src_path, _POSIX_PATH_MAX);
    strncpy(name, src_name, _POSIX_PATH_MAX);
  } else {
    strncpy(path, dst_path, _POSIX_PATH_MAX);
    strncpy(name, dst_name, _POSIX_PATH_MAX);
  }

  // strip the quotes from the path and name
  len = strlen(path);
  if (path[len-1] == '\'')
    path[len-1] = 0;

  if (path[0] == '\'') {
    tmpVal = (char *) malloc(strlen(path));
    tmp1 = path+1;
    strncpy(tmpVal, tmp1, strlen(path));
    strncpy(path, tmpVal, _POSIX_PATH_MAX);
    free(tmpVal);
  }

  len = strlen(name);
  if (name[len-1] == '\'')
    name[len-1] = 0;
  if (name[0] == '\'') {
    tmpVal = (char *) malloc(strlen(name));
    tmp1 = name+1;
    strncpy(tmpVal, tmp1, strlen(name));
    strncpy(name, tmpVal, _POSIX_PATH_MAX);
    free(tmpVal);
  }

  sprintf(pathname, "%s/%s", path, name);

  // Check if file is still there with same last modified time
	if (stat(pathname, &file_status) < 0) {
		dprintf(D_FULLDEBUG, "ERROR: File '%s' can not be accessed.\n", 
				pathname);
		fileSame = FALSE;
	} else {
		if (old_ts != file_status.st_mtime) {
			fileSame = FALSE;
		}
  }

  f_size = file_status.st_size;
  if (fileSame && (f_size > 0) && file_checksum(pathname, f_size, sum)) {
		int i;
    for (i = 0; i < MAC_SIZE; i++)
      sprintf(&hexSum[2*i], "%2x", sum[i]);		
    hexSum[2*MAC_SIZE] = '\0';
  }
  else
		hexSum[0] = '\0';
  
  sql_stmt = (char *)malloc(2048 + 2*(strlen(globaljobid) + strlen(src_name) + strlen(src_host) + strlen(src_path) + strlen(dst_name) + strlen(dst_host) + strlen(dst_path) + strlen(dst_daemon) + strlen(hexSum) + strlen(last_modified)));
  
  sprintf(sql_stmt,
          "INSERT INTO transfers (globaljobid, src_name, src_host, src_path, dst_name, dst_host, dst_path, transfer_size, elapsed, dst_daemon, checksum, last_modified) VALUES (%s, %s, %s, %s, %s, %s, %s, \'%s\', \'%s\', %s, %d, %d)", globaljobid, src_name, src_host, src_path, dst_name, dst_host, dst_path, transfer_size, elapsed, dst_daemon, hexSum, last_modified);

	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		free(sql_stmt);
		return FAILURE;
	}

	free(sql_stmt);

	return SUCCESS;
}

QuillErrCode TTManager::updateBasic(AttrList *info, AttrList *condition, 
									char *tableName) {
	char *sql_stmt = NULL;
	MyString classAd, classAd1;
	const char *iter;	
	char *setList=NULL, *whereList=NULL;
	char *attName = NULL, *attVal;
	char *newvalue;
	bool isRuns = FALSE;

	setList = (char *) malloc(1);
	setList[0] = '\0';
	whereList = (char *) malloc(1);
	whereList[0] = '\0';
	
	if (strcasecmp (tableName, "Runs") == 0) {
		isRuns = TRUE;
	}

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

			if (isRuns && (strcasecmp(attName, "endts") == 0)) {
				time_t clock;
				clock = atoi(attVal);
				newvalue = condor_ttdb_buildts(&clock, dt);

				if (newvalue == NULL) {
					dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertRuns\n");
					if (setList) free(setList);
					free(attName);
					return FAILURE;
				}
				
			} else {
			
					// escape single quote if any within the value
				newvalue = fillEscapeCharacters(attVal);

					// change double quotes to single quote if any
				attValLen = strlen(newvalue);
 
				if (newvalue[attValLen-1] == '"')
					newvalue[attValLen-1] = '\'';

				if (newvalue[0] == '"') {
					newvalue[0] = '\'';
				}			
			}
			
			setList = (char *) realloc(setList, 
									   strlen(setList) + strlen(attName) + 
									   strlen(newvalue) + 10);
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
				if (strcasecmp(attVal, "null") == 0) {
					whereList = (char *) realloc(whereList, 
												 strlen(whereList) + 
												 strlen(attName) + 
												 20);
					strcat(whereList, attName);
					strcat(whereList, " is null and ");

					free(attName);

					iter = classAd1.GetNextToken("\n", true);
					continue;
				}

				if (isRuns && (strcasecmp(attName, "endts") == 0)) {
					time_t clock;
					clock = atoi(attVal);
					newvalue = condor_ttdb_buildts(&clock, dt);

					if (newvalue == NULL) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not built in TTManager::insertRuns\n");
						if (setList) free(setList);
						free(attName);
						return FAILURE;
					}
					
				} else {
						// change double quotes to single quote if any
					attValLen = strlen(attVal);
					
					if (attVal[attValLen-1] == '"')
						attVal[attValLen-1] = '\'';

					if (attVal[0] == '"') {
						attVal[0] = '\'';
					}
					
					newvalue = attVal;
				}
				
				whereList = (char *) realloc(whereList, 
											 strlen(whereList) + 
											 strlen(attName) + 
											 strlen(newvalue) + 
											 20);
				strcat(whereList, attName);
				strcat(whereList, " = ");
				strcat(whereList, newvalue);
				strcat(whereList, " and ");

				free(attName);
				if (isRuns && (strcasecmp(attName, "endts") == 0)) {
					free(newvalue);
				}

				iter = classAd1.GetNextToken("\n", true);
			}
		
			// remove the last " and "
		whereList[strlen(whereList)-5] = 0;
	}

	sql_stmt = (char *) malloc (100 + strlen(tableName) + strlen(setList) + strlen(whereList));
		// build sql stmt
	sprintf(sql_stmt, "UPDATE %s SET %s WHERE %s", tableName, setList, whereList);		
	
	if (DBObj->execCommand(sql_stmt) == FAILURE) {
		dprintf(D_ALWAYS, "Executing Statement --- Error\n");
		dprintf(D_ALWAYS, "sql = %s\n", sql_stmt);
		if (setList) free(setList);
		if (whereList) free(whereList);
		free(sql_stmt);
		return FAILURE;
	}
	
	if (setList) free(setList);
	if (whereList) free(whereList);	
	free(sql_stmt);

	return SUCCESS;
}

static int 
typeOf(char *attName)
{
	if (!(strcasecmp(attName, ATTR_CKPT_SERVER) && 
		  strcasecmp(attName, "CKPT_SERVER_HOST") &&
		  strcasecmp(attName, ATTR_STATE) && 
		  strcasecmp(attName, ATTR_ACTIVITY) &&
		  strcasecmp(attName, ATTR_CPU_IS_BUSY) && 
		  strcasecmp(attName, ATTR_RANK) && 
		  strcasecmp(attName, ATTR_REQUIREMENTS) && 
		  strcasecmp(attName, ATTR_NAME) &&
		  strcasecmp(attName, ATTR_OPSYS) && 
		  strcasecmp(attName, ATTR_ARCH) &&
		  strcasecmp(attName, "GlobalJobId") &&
		  strcasecmp(attName, "username") &&
		  strcasecmp(attName, "scheddname") &&
		  strcasecmp(attName, "startdname") && 
		  strcasecmp(attName, "diagnosis") &&
		  strcasecmp(attName, "remote_user") && 
		  strcasecmp(attName, ATTR_UPDATESTATS_HISTORY)
		  )
		)
		return CONDOR_TT_TYPE_STRING;

	if (!(strcasecmp(attName, ATTR_KEYBOARD_IDLE) && 
		  strcasecmp(attName, ATTR_CONSOLE_IDLE) &&
		  strcasecmp(attName, ATTR_LOAD_AVG) && 
		  strcasecmp(attName, "CondorLoadAvg") &&
		  strcasecmp(attName, ATTR_TOTAL_LOAD_AVG) && 
		  strcasecmp(attName, ATTR_VIRTUAL_MEMORY) &&
		  strcasecmp(attName, ATTR_MEMORY ) && 
		  strcasecmp(attName, ATTR_TOTAL_VIRTUAL_MEMORY) &&
		  strcasecmp(attName, ATTR_CPU_BUSY_TIME) && 
		  strcasecmp(attName, ATTR_CURRENT_RANK) &&
		  strcasecmp(attName, ATTR_CLOCK_MIN) && 
		  strcasecmp(attName, ATTR_CLOCK_DAY) && 
		  strcasecmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) && 
		  strcasecmp(attName, ATTR_UPDATESTATS_TOTAL) &&
		  strcasecmp(attName, ATTR_UPDATESTATS_SEQUENCED) && 
		  strcasecmp(attName, ATTR_UPDATESTATS_LOST) &&
		  strcasecmp(attName, "cluster") && 
		  strcasecmp(attName, "proc") &&
		  strcasecmp(attName, ATTR_NUM_USERS) &&
		  strcasecmp(attName, ATTR_TOTAL_IDLE_JOBS) &&
		  strcasecmp(attName, ATTR_TOTAL_RUNNING_JOBS) &&
		  strcasecmp(attName, ATTR_TOTAL_JOB_ADS) &&
		  strcasecmp(attName, ATTR_TOTAL_HELD_JOBS) &&
		  strcasecmp(attName, ATTR_TOTAL_FLOCKED_JOBS) && 
		  strcasecmp(attName, ATTR_TOTAL_REMOVED_JOBS) &&
		  strcasecmp(attName, "monitorselfcpuusage") &&
		  strcasecmp(attName, "monitorselfimagesize") && 
		  strcasecmp(attName, "monitorselfresidentsetsize") && 
		  strcasecmp(attName, "monitorselfage")
		  )
		)
		return CONDOR_TT_TYPE_NUMBER;

	if (!(strcasecmp(attName, ATTR_LAST_HEARD_FROM) &&
		  strcasecmp(attName, ATTR_ENTERED_CURRENT_ACTIVITY) && 
		  strcasecmp(attName, ATTR_ENTERED_CURRENT_STATE) &&
		  strcasecmp(attName, "reject_time") &&
		  strcasecmp(attName, "match_time") && 
		  strcasecmp(attName, "MonitorSelfTime")
		  )
		)
		return CONDOR_TT_TYPE_TIMESTAMP;

	return -1;
}

/* An attribute is treated as a static one if it is not one of the following
   Notice that if you add more attributes to the following list, the horizontal
   schema for Machine_ClassAd needs to revised accordingly.
*/
static int isHorizontalMachineAttr(char *attName)
{
	return !(strcasecmp(attName, ATTR_OPSYS) && 
			strcasecmp(attName, ATTR_ARCH) &&
			strcasecmp(attName, ATTR_CKPT_SERVER) && 
			strcasecmp(attName, "CKPT_SERVER_HOST") &&
			strcasecmp(attName, ATTR_STATE) && 
			strcasecmp(attName, ATTR_ACTIVITY) &&
			strcasecmp(attName, ATTR_KEYBOARD_IDLE) && 
			strcasecmp(attName, ATTR_CONSOLE_IDLE) &&
			strcasecmp(attName, ATTR_LOAD_AVG) && 
			strcasecmp(attName, "CondorLoadAvg") &&
			strcasecmp(attName, ATTR_TOTAL_LOAD_AVG) && 
			strcasecmp(attName, ATTR_VIRTUAL_MEMORY) &&
			strcasecmp(attName, ATTR_MEMORY ) && 
			strcasecmp(attName, ATTR_TOTAL_VIRTUAL_MEMORY) &&
			strcasecmp(attName, ATTR_CPU_BUSY_TIME) && 
			strcasecmp(attName, ATTR_CPU_IS_BUSY) &&
			strcasecmp(attName, ATTR_RANK) && 
			strcasecmp(attName, ATTR_CURRENT_RANK) &&
			strcasecmp(attName, ATTR_REQUIREMENTS) && 
			strcasecmp(attName, ATTR_CLOCK_MIN) &&
			strcasecmp(attName, ATTR_CLOCK_DAY) && 
			strcasecmp(attName, ATTR_LAST_HEARD_FROM) &&
			strcasecmp(attName, ATTR_ENTERED_CURRENT_ACTIVITY) && 
			strcasecmp(attName, ATTR_ENTERED_CURRENT_STATE) &&
			strcasecmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) && 
			strcasecmp(attName, ATTR_UPDATESTATS_TOTAL) &&
			strcasecmp(attName, ATTR_UPDATESTATS_SEQUENCED) && 
			strcasecmp(attName, ATTR_UPDATESTATS_LOST) &&
			strcasecmp(attName, ATTR_NAME) && 
			strcasecmp(attName, "GlobalJobId") &&
			strcasecmp(attName, "LastHeardFrom")
			);
}

/* The following attributes are treated as horizontal daemon attributes.
   Notice that if you add more attributes to the following list, the horizontal
   schema for Daemon_Horizontal needs to revised accordingly.
*/
static int isHorizontalDaemonAttr(char *attName)
{
	return (!strcasecmp(attName, ATTR_NAME) ||
			!strcasecmp(attName, ATTR_LAST_HEARD_FROM) ||
			!strcasecmp(attName, "monitorselftime") ||
			!strcasecmp(attName, "monitorselfcpuusage") ||
			!strcasecmp(attName, "monitorselfimagesize") ||
			!strcasecmp(attName, "monitorselfresidentsetsize") ||
			!strcasecmp(attName, "monitorselfage") ||
			!strcasecmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) ||
			!strcasecmp(attName, ATTR_UPDATESTATS_TOTAL) ||
			!strcasecmp(attName, ATTR_UPDATESTATS_SEQUENCED) ||
			!strcasecmp(attName, ATTR_UPDATESTATS_LOST) ||
			!strcasecmp(attName, ATTR_UPDATESTATS_HISTORY));
}

/* The following attributes are treated as horizontal schedd attributes.
   Notice that if you add more attributes to the following list, the horizontal
   schema for Schedd_Horizontal needs to revised accordingly.
*/
static int isHorizontalScheddAttr(char *attName)
{
	return (!strcasecmp(attName, ATTR_NAME) ||
			!strcasecmp(attName, ATTR_LAST_HEARD_FROM) ||
			!strcasecmp(attName, ATTR_NUM_USERS) ||
			!strcasecmp(attName, ATTR_TOTAL_IDLE_JOBS) ||
			!strcasecmp(attName, ATTR_TOTAL_RUNNING_JOBS) ||
			!strcasecmp(attName, ATTR_TOTAL_JOB_ADS) ||
			!strcasecmp(attName, ATTR_TOTAL_HELD_JOBS) ||
			!strcasecmp(attName, ATTR_TOTAL_FLOCKED_JOBS) ||
			!strcasecmp(attName, ATTR_TOTAL_REMOVED_JOBS));
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

static void stripquotes(char *attVal)
{
	int attValLen;
	char *tmp1, *tmpVal;

	attValLen = strlen(attVal);

		/* strip enclosing double quotes or single quotes,
		   but not the unmatched quotes because they might
		   be part of an expression, e.g., requirements = attr == "a",
		   here we need to keep the double quote in the end of the expression.
		*/
	if ((attVal[attValLen-1] == '"' && attVal[0] == '"') ||
		(attVal[attValLen-1] == '\'' && attVal[0] == '\'')) 
		{
			attVal[attValLen-1] = 0;
			tmpVal = (char *) malloc(strlen(attVal));
			tmp1 = attVal+1;
			strcpy(tmpVal, tmp1);
			strcpy(attVal, tmpVal);
			free(tmpVal);			
		}
}
