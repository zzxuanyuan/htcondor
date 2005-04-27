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

//for the config_fill_ad call
#include "condor_config.h"

//for the ATTR_*** variables stuff
#include "condor_attributes.h"

#include "condor_io.h"
#include "get_daemon_name.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "jobqueuedbmanager.h"
#include "quill_dbschema_def.h"
#include "classadlogentry.h"
#include "prober.h"
#include "classadlogparser.h"
#include "jobqueuedatabase.h"
#include "pgsqldatabase.h"
#include "requestservice.h"
#include "jobqueuecollection.h"


#ifndef _DEBUG_
#define _DEBUG_
#endif

//! constructor
JobQueueDBManager::JobQueueDBManager()
{
		//nothing here...its all done in config()
}

//! destructor
JobQueueDBManager::~JobQueueDBManager()
{
		// release Objects
	if (collectors)
		delete collectors;
	if (prober != NULL)
		delete prober;
	if (caLogParser != NULL)
		delete caLogParser;
	if (jqDatabase != NULL)
		delete jqDatabase;
	
		// release strings
	if (jobQueueLogFile != NULL)
		free(jobQueueLogFile);
	if (jobQueueDBIpAddress != NULL)
		free(jobQueueDBIpAddress);
	if (jobQueueDBName != NULL)
		free(jobQueueDBName);
	if (jobQueueDBConn != NULL)
		free(jobQueueDBConn);
	if (multi_sql_str != NULL)
		free(multi_sql_str);
}

void
JobQueueDBManager::config(bool reconfig) 
{
	ad = NULL;
	pollingTimeId = -1;
	purgeHistoryTimeId = -1;


		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the job_queue.log file
	char *spool = param("SPOOL");
	if(!spool) {
		dprintf(D_ALWAYS, 
				"Error: No SPOOL variable found in config file - exiting\n");
		exit(1);
	}
  
	jobQueueLogFile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	sprintf(jobQueueLogFile, "%s/job_queue.log", spool);
	free(spool);

		/*
		  Here we try to read the <ipaddress:port> stored in condor_config
		  if one is not specified, by default we use the local address 
		  and the default postgres port of 5432.  
		*/
	char host[64], port[64];  //used to construct the connection string
	jobQueueDBIpAddress = param("DATABASE_IPADDRESS");
	if(!jobQueueDBIpAddress) {
		jobQueueDBIpAddress = (char *) malloc(128 * sizeof(char));
		strcpy(jobQueueDBIpAddress, daemonCore->InfoCommandSinfulString());
		char *ptr_colon = strchr(jobQueueDBIpAddress, ':');
		strcpy(ptr_colon, ":5432>");
		strcpy(host," ");
		strcpy(port," ");
	}
	else {
			//split the <ipaddress:port> into its two parts accordingly
		char *ptr_colon = strchr(jobQueueDBIpAddress, ':');
		strcpy(host, "host= ");
		strncat(host, 
				jobQueueDBIpAddress+1, 
				ptr_colon - jobQueueDBIpAddress - 1);
		strcpy(port, "port= ");
		strcat(port, ptr_colon+1);
		port[strlen(port)-1] = '\0';
	}

		/* Here we read the database name and if one is not specified
		   use the default name - quill
		   If there are more than one quill daemons are writing to the
		   same databases, its absolutely necessary that the database
		   names be unique or else there would be clashes.  Having 
		   unique database names is the responsibility of the administrator
		*/
	jobQueueDBName = param("DATABASE_NAME");
	if(!jobQueueDBName) jobQueueDBName = strdup("quill");
	jobQueueDBConn = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	sprintf(jobQueueDBConn, "%s %s dbname=%s", host, port, jobQueueDBName);
	
		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	char *pollingPeriod_str = param("QUILL_POLLING_PERIOD");
	if(pollingPeriod_str) {
		pollingPeriod = atoi(pollingPeriod_str);
		free(pollingPeriod_str);
	}
	else pollingPeriod = 10;
  
		// length of history to keep; 
		// use default value of 6 months = 180 days :)
	char *purgeHistoryDuration_str = param("QUILL_HISTORY_DURATION");
	if(purgeHistoryDuration_str) {
		purgeHistoryDuration = atoi(purgeHistoryDuration_str);
		free(purgeHistoryDuration_str);
	}
	else purgeHistoryDuration = 180;  //default of 6 months

		/* 
		   the following parameters specifies the frequency of calling
		   the purge history command.  
		   This is different from purgeHistoryDuration as this just 
		   calls the command, which actually deletes only those rows
		   which are older than purgeHistoryDuration.  Since
		   the SQL itself for checking old records and deleting can be
		   quite expensive, we leave the frequency of calling it upto
		   the administrator.  By default, it is 24 hours. 
		*/
		
	char *historyCleaningInterval_str = param("QUILL_HISTORY_CLEANING_INTERVAL");
	if(historyCleaningInterval_str) {
		historyCleaningInterval = atoi(historyCleaningInterval_str);
		free(historyCleaningInterval_str);
	}
	else historyCleaningInterval = 24;  //default of 24 hours
  
	dprintf(D_ALWAYS, "Using Job Queue File %s\n", jobQueueLogFile);
	dprintf(D_ALWAYS, "Using Database IpAddress = %s\n", jobQueueDBIpAddress);
	dprintf(D_ALWAYS, "Using Database Name = %s\n", jobQueueDBName);
	dprintf(D_ALWAYS, "Using Database Connection String = \"%s\"\n", jobQueueDBConn);
	dprintf(D_ALWAYS, "Using Polling Period = %d\n", pollingPeriod);
	dprintf(D_ALWAYS, "Using Purge History Duration = %d days\n", purgeHistoryDuration);
	dprintf(D_ALWAYS, "Using History Cleaning Interval = %d hours\n", historyCleaningInterval);

		// this function is also called when condor_reconfig is issued
		// and so we dont want to recreate all essential objects
	if(!reconfig) {
		prober = new Prober(this);
		caLogParser = new ClassAdLogParser();

#ifdef _POSTGRESQL_DBMS_
		jqDatabase = new PGSQLDatabase(jobQueueDBConn);
#endif
		xactState = NOT_IN_XACT;
		numTimesPolled = 0; 

		multi_sql_str = NULL;
	}
  
		//this function assumes that certain members exist and so
		//it should be done after constructing those objects
	setJobQueueFileName(jobQueueLogFile);
  
}

//! set the path to the job queue
void
JobQueueDBManager::setJobQueueFileName(const char* jqName)
{
	prober->setJobQueueName((char*)jqName);
	caLogParser->setJobQueueName((char*)jqName);
}

//! get the parser
ClassAdLogParser* 
JobQueueDBManager::getClassAdLogParser() 
{
	return caLogParser;
}

//! maintain the job queue 
int 
JobQueueDBManager::maintain()
{	
	int st, db_st; 
	
	db_st = 0;


		// timing stuff added for performance testing	
		//time_t start,end;
		//double dif;
		//time(&start);
		//end timing stuff
	
	st = prober->getProbeInfo(); // get the last polling information

		//if we are unable to get to the database, then either the 
		//postgres server is down or the database is deleted.  In 
		//any case, we call checkSchema to create a database if needed
	if(st <= 0) {
		st = checkSchema();
		if(st <= 0)
			return st;
	}
	st = prober->probe();	// polling
	
		// {init|add}JobQueueDB processes the  Log and stores probing
		// information into DB documentation for how do we determine 
		// the correct state is in the Prober->probe method
	switch(st) {
	case Prober::INIT_QUILL:
		dprintf(D_ALWAYS, "POLLING RESULT: INIT\n");
		db_st = initJobQueueTables();
		break;
	case Prober::ADDITION:
		dprintf(D_ALWAYS, "POLLING RESULT: ADDED\n");
		db_st = addJobQueueTables();
		break;
	case Prober::COMPRESSED:
		dprintf(D_ALWAYS, "POLLING RESULT: COMPRESSED\n");
		db_st = initJobQueueTables();
		break;
	case Prober::ERROR:
		dprintf(D_ALWAYS, "POLLING RESULT: ERROR\n");
		db_st = initJobQueueTables();
		break;
	case Prober::NO_CHANGE:
		dprintf(D_ALWAYS, "POLLING RESULT: NO CHANGE\n");
		break;
	default:
		dprintf(D_ALWAYS, "ERROR HAPPENED DURING POLLING\n");
	}

		//time(&end);
		//dif = difftime (end,start);
		//dprintf (D_ALWAYS,"TIMING = %.2lf seconds\n", dif );

	if (st != Prober::NO_CHANGE)
		return db_st;
	else
		return Prober::NO_CHANGE;
}

/*! delete the job queue related tables
 *  \return the result status
 *			1: Success
 *			0: Fail	(SQL execution fail)
 */	
int
JobQueueDBManager::cleanupJobQueueTables()
{
	int		sqlNum = 4;
	int		i;
	char 	sql_str[sqlNum][128];

		// we only delete job queue related information.
		// historical information is deleted based on user policy
		// see QUILL_HISTORY_DURATION in the manual for more info
	sprintf(sql_str[0], 
			"DELETE FROM ClusterAds_Str;");
	sprintf(sql_str[1], 
			"DELETE FROM ClusterAds_Num;");
	sprintf(sql_str[2], 
			"DELETE FROM ProcAds_Str;");
	sprintf(sql_str[3], 
			"DELETE FROM ProcAds_Num;");

	for (i = 0; i < sqlNum; i++) {
		if (jqDatabase->execCommand(sql_str[i]) < 0) {
			displayDBErrorMsg("Clean UP ALL Data --- ERROR");
			return 0; // return a error code, 0
		}
	}

	return 1;
}

/*! vacuums the job queue related tables 
 */
int
JobQueueDBManager::tuneupJobQueueTables()
{
	int		sqlNum = 5;
	int		i;
	char 	sql_str[sqlNum][128];

		// When a record is deleted, postgres only marks it
		// deleted.  Then space is reclaimed in a lazy fashion,
		// by vacuuming it, and as such we do this here.  
		// vacuuming is asynchronous, but can get pretty expensive
	sprintf(sql_str[0], 
			"VACUUM ANALYZE Clusterads_Str;");
	sprintf(sql_str[1], 
			"VACUUM ANALYZE Clusterads_Num;");
	sprintf(sql_str[2], 
			"VACUUM ANALYZE Procads_Str;");
	sprintf(sql_str[3], 
			"VACUUM ANALYZE Procads_Num;");
	sprintf(sql_str[4], 
			"VACUUM ANALYZE Jobqueuepollinginfo;");

	for (i = 0; i < sqlNum; i++) {
		if (jqDatabase->execCommand(sql_str[i]) < 0) {
			displayDBErrorMsg("VACUUM Database --- ERROR");
			return 0; // return a error code, 0
		}
	}

	return 1;
}


/*! purge all jobs from history according to user policy
 *  and vacuums all historical tables
 */
int
JobQueueDBManager::purgeOldHistoryRows()
{
	int		sqlNum = 4;
	int		i;
	char 	sql_str[sqlNum][256];

	sprintf(sql_str[0],
			"DELETE FROM History_Vertical WHERE cid IN (SELECT cid FROM History_Horizontal WHERE 'epoch'::timestamp with time zone + cast(text(\"CompletionDate\")||text(' seconds') as interval) < timestamp 'now' - interval '%d days');", purgeHistoryDuration);
	sprintf(sql_str[1],
			"DELETE FROM History_Horizontal WHERE 'epoch'::timestamp with time zone + cast(text(\"CompletionDate\")||text(' seconds') as interval) < 'now'::timestamp - interval '%d days';", purgeHistoryDuration);

		// When a record is deleted, postgres only marks it
		// deleted.  Then space is reclaimed in a lazy fashion,
		// by vacuuming it, and as such we do this here.  
		// vacuuming is asynchronous, but can get pretty expensive
	sprintf(sql_str[2], 
			"VACUUM ANALYZE History_Horizontal;");
	sprintf(sql_str[3], 
			"VACUUM ANALYZE History_Vertical;");

		// the delete commands are wrapped inside a transaction.  We 
		// dont want to have stuff deleted from the horizontal but not 
		//the vertical and vice versa
	if(connectDB(BEGIN_XACT) <= 0) {
		displayDBErrorMsg("Purge History Rows unable to connect--- ERROR");
		return 0; // return a error code, 0
	}

	for (i = 0; i < 2; i++) {
		if (jqDatabase->execCommand(sql_str[i]) < 0) {
			displayDBErrorMsg("Purge History Rows --- ERROR");
			disconnectDB(ABORT_XACT);
			return 0; // return a error code, 0
		}
	}
	disconnectDB(COMMIT_XACT);


		//vacuuming cannot be bound inside a transaction
	if(connectDB(NOT_IN_XACT) <= 0) {
		displayDBErrorMsg("Vacuum History Rows unable to connect--- ERROR");
		return 0; // return a error code, 0
	}

	for (i = 2; i < 4; i++) {
		if (jqDatabase->execCommand(sql_str[i]) < 0) {
			displayDBErrorMsg("Vacuum History Rows --- ERROR");
			disconnectDB(NOT_IN_XACT);
			return 0; // return a error code, 0
		}
	}
	disconnectDB(NOT_IN_XACT);
	
	return 1;
}


/*! connect to DBMS
 *  \return the result status
 *			1: Success
 * 			0: Fail (DB connection and/or Begin Xact fail)
 */	
int
JobQueueDBManager::connectDB(int  Xact)
{
	int st = 0;
	st = jqDatabase->connectDB(jobQueueDBConn);
	if (st <= 0) // connect to DB
		return st;
  
	if (Xact == BEGIN_XACT) {
		if (jqDatabase->beginTransaction() == 0) // begin XACT
			return 0;
	}
  
	return 1;
}


/*! disconnect from DBMS, and handle XACT (commit, abort, not in XACT)
 *  \param commit XACT command 
 *					0: non-Xact
 *					1: commit
 *					2: abort
 */
int
JobQueueDBManager::disconnectDB(int commit)
{
	if (commit == COMMIT_XACT) {
		if (xactState != BEGIN_XACT) {
			jqDatabase->commitTransaction(); // commit XACT
			xactState = NOT_IN_XACT;
		}
	} else if (commit == ABORT_XACT) { // abort XACT
		jqDatabase->rollbackTransaction();
	}

	jqDatabase->disconnectDB(); // disconnect from DB

	return 1;
}


/*! build the job queue collection from job_queue.log file
 */
int	
JobQueueDBManager::buildJobQueue(JobQueueCollection *jobQueue)
{
	int		op_type;

	while ((op_type = caLogParser->readLogEntry()) > 0) {
		if (processLogEntry(op_type, jobQueue) == 0) // process each ClassAd Log Entry
			return 0;
	}

	return 1;
}

/*! load job ads in a job queue collection into DB
 */
int
JobQueueDBManager::loadJobQueue(JobQueueCollection *jobQueue)
{
	char* 	ret_str;
	char*   ret_str2;
	char	sql_str[1024];
	bool 	bFirst = true;

		//
		// Make a COPY SQL string and load it into ClusterAds_Str table
		//
	jobQueue->initAllJobAdsIteration();

	while((ret_str = jobQueue->getNextClusterAd_StrCopyStr()) != NULL) {

		if ((bFirst == true)&& (ret_str != NULL)) {			
			// we need to issue the COPY command first
			sprintf(sql_str, "COPY ClusterAds_Str FROM stdin;");
			if (jqDatabase->execCommand(sql_str) < 0) {
				displayDBErrorMsg("COPY ClusterAds_Str --- ERROR");
				return 0; // return a error code, 0
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (jqDatabase->sendBulkData(ret_str) < 0) {
				return 0;
			}
		}
	  
	}
	
	if (bFirst == false) {
		if (jqDatabase->sendBulkDataEnd() < 0) {
			return 0;
		}
	  
		bFirst = true;
	}	
	
		//
		// Make a COPY SQL string and load it into ClusterAds_Num table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextClusterAd_NumCopyStr()) != NULL) {
	  		
		if ((bFirst == true)&& (ret_str != NULL)) {			
			sprintf(sql_str, "COPY ClusterAds_Num FROM stdin;");
			if (jqDatabase->execCommand(sql_str) < 0) {
				displayDBErrorMsg("COPY ClusterAds_Num --- ERROR");
				return 0; // return a error code, 0
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (jqDatabase->sendBulkData(ret_str) < 0) {
				return 0;
			}	
		}		
	  
	}
	
	if (bFirst == false) {
		if (jqDatabase->sendBulkDataEnd() < 0) {
			return 0;
		}
	  	  
		bFirst = true;
	}
	
	
	
	
	
		//
		// Make a COPY sql string and load it into ProcAds_Str table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextProcAd_StrCopyStr()) != NULL) {
		if ((bFirst == true)&& (ret_str != NULL)) {			
			sprintf(sql_str, "COPY ProcAds_Str FROM stdin;");
			if (jqDatabase->execCommand(sql_str) < 0) {
				displayDBErrorMsg("COPY ProcAds_Str --- ERROR");
				return 0; // return a error code, 0
			}
			
			bFirst = false;
		}
	  
	  
		if (ret_str != NULL) {
			if (jqDatabase->sendBulkData(ret_str) < 0) {
				return 0;
			}
		}
	  
	}
	
	if (bFirst == false) {
	  
		if (jqDatabase->sendBulkDataEnd() < 0) {
			return 0;
		}
	  	  
		bFirst = true;
	}
	
	
		//
		// Make a COPY sql string and load it into ProcAds_Num table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextProcAd_NumCopyStr()) != NULL) {
		if ((bFirst == true)&& (ret_str != NULL)) {			
			sprintf(sql_str, "COPY ProcAds_Num FROM stdin;");
			if (jqDatabase->execCommand(sql_str) < 0) {
				displayDBErrorMsg("COPY ProcAds_Num --- ERROR");
				return 0; // return a error code, 0
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (jqDatabase->sendBulkData(ret_str) < 0) {
				return 0;
			}
		}
	  
	}
	
	
	if (bFirst == false) {
		if (jqDatabase->sendBulkDataEnd() < 0) {
			return 0;
		}
	  
		bFirst = true;
	}
	

		/*
		  Make the SQL strings for both history_horizontal and 
		  vertical and execute them.  Our technique of bulk loading
		  history is different from that of the other tables. 
		  Instead of using the COPY command, we use successive INSERT
		  statements.  This code will execute rarely; it is executed
		  when quill needs to initialize its queue (see 'maintain' 
		  above) and a job gets deleted from the 
		  job queue (completed, removed, etc.) and as such needs to 
		  get into the history tables.  Quill, as part of its bulk 
		  loading routine will also make sure that jobs properly get
		  inserted into the history tables.
		*/		

	int st = 0;
	ret_str = ret_str2 = NULL;
	jobQueue->initAllHistoryAdsIteration();
	st = jobQueue->getNextHistoryAd_SqlStr(ret_str, ret_str2);

	while(st > 0 &&  ret_str != NULL) {
		st = jqDatabase->execCommand(ret_str);
		st = jqDatabase->execCommand(ret_str2);

			//these need to be nullified as the next routine expects
			//them to be so.  This is not a memory leak since these 
			//arguments are passed by reference and their actual 
			//locations are members of the JobQueueCollection class
			//and as such will be freed by it.
		ret_str = ret_str2 = NULL;
		st = jobQueue->getNextHistoryAd_SqlStr(ret_str, ret_str2);

	}
	
	return 1;
}


/*! build an in-memory list of jobs by reading entries in job_queue.log 
 *  file and dump them into the database.  There are various in-memory lists
 *  for live jobs, i.e. jobs not completed, and historical jobs.  All of them
 *  are written to their appropriate tables
*/
int 
JobQueueDBManager::buildAndWriteJobQueue()
{
		//this is an array of linked lists, which keep growing, so 
		//although the array is of fixed size=2000, the ever-growing
		//linked lists can accomodate any number of jobs
	JobQueueCollection *jobQueue = new JobQueueCollection(2000);

	struct timespec startTp;
	struct timespec endTp;


		//  START OF FIRST PHASE OF BULKLOADING
	clock_gettime(CLOCK_REALTIME, &startTp);

	dprintf(D_FULLDEBUG, "THIS IS THE 1st PHASE: PARSING a job_queue.log file and BUILDING JOB QUEUE COLLECTION!\n");

		//read from the beginning
	caLogParser->setNextOffset(0);

		//construct the in memory job queue and history
	if (buildJobQueue(jobQueue) != 1) {
		return 0;
	}

	clock_gettime(CLOCK_REALTIME, &endTp);

	dprintf(D_FULLDEBUG, "1st PHASE: %.2fms\n" ,(double)(((endTp.tv_sec - startTp.tv_sec) * 1000000000.0 - startTp.tv_nsec + endTp.tv_nsec) / 1000000.0));

	dprintf(D_FULLDEBUG, "THIS IS THE 2ND PHASE: LOADING THE JOBS INTO DBMS!\n");
		//  END OF FIRST PHASE OF BULKLOADING


		//  START OF SECOND PHASE OF BULKLOADING
	clock_gettime(CLOCK_REALTIME, &startTp);

		// For job queue tables, send COPY string to RDBMS: 
		// For history tables, send INSERT to DBMS - all part of
		// bulk loading
	if (loadJobQueue(jobQueue) != 1) {
		return 0;
	}

	clock_gettime(CLOCK_REALTIME, &endTp);
	dprintf(D_FULLDEBUG, "2nd PHASE: %.2fms\n" ,(double)(((endTp.tv_sec - startTp.tv_sec) * 1000000000.0 - startTp.tv_nsec + endTp.tv_nsec) / 1000000.0));
			//  END OF SECOND PHASE OF BULKLOADING

	delete jobQueue;
	return 1;
}


/*! incrementally read and process log entries from file 
 */
int 
JobQueueDBManager::readAndWriteLogEntries()
{
	int op_type = 0;

		// Process ClassAd Log Entry
	while ((op_type = caLogParser->readLogEntry()) > 0) {
			//dprintf(D_ALWAYS, "processing log record\n");
		if (processLogEntry(op_type, false) == 0)
			return 0; // fail: need to abort Xact
	}

	return 1; // success
}

/*! process only DELTA, i.e. entries of the job queue log which were
 *  added since the last offset read/set by quill
 */
int 
JobQueueDBManager::addJobQueueTables()
{
	int st;

		//we dont set transactions explicitly.  they are set by the 
		//schedd via the 'begin transaction' log entry
	connectDB(NOT_IN_XACT);

	caLogParser->setNextOffset();

	st = readAndWriteLogEntries();

		// Store a polling information into DB
	if (st > 0)
		prober->setProbeInfo();

	if (st == 0) 
		disconnectDB(ABORT_XACT); // abort and end Xact
	else {
			// VACUUM should be called outside XACT
			// So, commit XACT shouble be invoked beforehand.
		if (xactState != BEGIN_XACT) {
			jqDatabase->commitTransaction(); // end XACT
			xactState = NOT_IN_XACT;
		}

			//we VACUUM job queue tables twice every day, assuming that
			//quill has been up for 12 hours
		if ((numTimesPolled++ * pollingPeriod) > (60 * 60 * 12)) {
			tuneupJobQueueTables();
			numTimesPolled = 0;
		}

		disconnectDB(NOT_IN_XACT); // commit and end Xact
	}
	
	return st;
}

/*! purge all job queue rows and process the entire job_queue.log file
 *  also vacuum the job queue tables
 */
int  
JobQueueDBManager::initJobQueueTables()
{
	int st;

	connectDB(); // connect to DBMS

	cleanupJobQueueTables(); // delete all job queue tables

	st = buildAndWriteJobQueue(); // bulk load job queue log

		// Store polling information in database
	if (st > 0)
		prober->setProbeInfo();

	if (st == 0)
		disconnectDB(ABORT_XACT); // abort and end Xact
	else {
			// VACUUM should be called outside XACT
			// So, Commit XACT shouble be invoked beforehand.
		if (xactState != BEGIN_XACT) {
			jqDatabase->commitTransaction(); // end XACT
			xactState = NOT_IN_XACT;
		}
		
		tuneupJobQueueTables();
		disconnectDB(NOT_IN_XACT); // commit and end Xact
	}

	return st;	
}


/*! handle a log Entry: work with a job queue collection.
 *  (not with DBMS directry)
 */
int 
JobQueueDBManager::processLogEntry(int op_type, JobQueueCollection* jobQueue)
{
	char *key, *mytype, *targettype, *name, *value, *newvalue;
	key = mytype = targettype = name = value = newvalue = NULL;
	int	st = 1;

	int id_sort;
	char cid[512];
	char pid[512];

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here,
		// and they are at the end of the routine
	switch(op_type) {
	case CondorLogOp_NewClassAd: {
		if (caLogParser->getNewClassAdBody(key, mytype, targettype) < 0)
			return 0; 

		id_sort = getProcClusterIds(key, cid, pid);
		ClassAd* ad = new ClassAd();
		ad->SetMyTypeName("Job");
		ad->SetTargetTypeName("Machine");

		if (id_sort == 1) { // Cluster Ad
			jobQueue->insertClusterAd(cid, ad);
		}
		else if (id_sort == 2) { // Proc Ad
			jobQueue->insertProcAd(cid, pid, ad);
		}
		else { // case 0:  Error
			dprintf(D_ALWAYS, "[QUILL] New ClassAd --- ERROR\n");
			return 0; // return a error code, 0
		}

		break;
	}
	case CondorLogOp_DestroyClassAd: {
		if (caLogParser->getDestroyClassAdBody(key) < 0)
			return 0;
			
		id_sort = getProcClusterIds(key, cid, pid);

		if (id_sort == 1) { // Cluster Ad
			jobQueue->removeClusterAd(cid);
		}
		else if (id_sort == 2) { // Proc Ad
			ClassAd *clusterad = jobQueue->find(cid);
			ClassAd *procad = jobQueue->find(cid,pid);
			if(!clusterad || !procad) {
			    dprintf(D_ALWAYS, "[QUILL] Destroy ClassAd --- Cannot find clusterad or procad in memory job queue");
			    return 0;
			}
			ClassAd *clusterad_new = new ClassAd(*clusterad);
			ClassAd *historyad = new ClassAd(*procad);

			historyad->ChainToAd(clusterad_new);
			jobQueue->insertHistoryAd(cid,pid,historyad);
			jobQueue->removeProcAd(cid, pid);
		}
		else { // case 0:  Error
			dprintf(D_ALWAYS, "[QUILL] Destroy ClassAd --- ERROR\n");
			return 0; // return a error code, 0
		}
			
		break;
	}
	case CondorLogOp_SetAttribute: {	
		if (caLogParser->getSetAttributeBody(key, name, value) < 0)
			return 0;
						
		newvalue = fillEscapeCharacters(value);
		id_sort = getProcClusterIds(key, cid, pid);

		if (id_sort == 1) { // Cluster Ad
			ClassAd* ad = jobQueue->findClusterAd(cid);
			if (ad != NULL) 
					//ad->Insert(ins_str);
				ad->Assign(name, newvalue);
			else
				dprintf(D_ALWAYS, "[QUILL] ERROR: There is no such Cluster Ad[%s]\n", cid);
		}
		else if (id_sort == 2) { // Proc Ad
			ClassAd* ad = jobQueue->findProcAd(cid, pid);
			if (ad != NULL) 
					//ad->Insert(ins_str);
				ad->Assign(name, newvalue);
			else {
				dprintf(D_ALWAYS, "[QUILL] ERROR: There is no such Proc Ad[%s.%s]\n", cid, pid);
			}
		}
		else { // case 0:  Error
			dprintf(D_ALWAYS, "[QUILL] Set Attribute --- ERROR\n");
			return 0; // return a error code, 0
		}

			//free(ins_str);
		break;
	}
	case CondorLogOp_DeleteAttribute: {
		if (caLogParser->getDeleteAttributeBody(key, name) < 0)
			return 0;
		
		id_sort = getProcClusterIds(key, cid, pid);

		if (id_sort == 1) { // Cluster Ad
			ClassAd* ad = jobQueue->findClusterAd(cid);
			ad->Delete(name);
		}
		else if (id_sort == 2) { // Proc Ad
			ClassAd* ad = jobQueue->findProcAd(cid, pid);
			ad->Delete(name);
		}
		else { // case 0:  Error
			dprintf(D_ALWAYS, "[QUILL] Delete Attribute --- ERROR\n");
			return 0; // return a error code, 0
		}
		break;
	}
	case CondorLogOp_BeginTransaction:
		st = processBeginTransaction(true);
		break;
	case CondorLogOp_EndTransaction:
		st = processEndTransaction(true);
		break;
	default:
		printf("[QUILL] Unsupported Job Queue Command\n");
		return 0;
		break;
	}

		// pointers are release
	if (key != NULL) free(key);
	if (mytype != NULL) free(mytype);
	if (targettype != NULL) free(targettype);
	if (name != NULL) free(name);
	if (value != NULL) free(value);
	if (newvalue != NULL) free(newvalue);
	return st;
}






/*! handle ClassAd Log Entry
 *
 * is a wrapper over all the processXXX functions
 * in this and all the processXXX routines, if exec_later == true, 
 * a SQL string is returned instead of actually sending it to the DB.
 * However, we always have exec_later = false, which means it actually
 * writes to the database in an eager fashion
 */
int 
JobQueueDBManager::processLogEntry(int op_type, bool exec_later)
{
	char *key, *mytype, *targettype, *name, *value;
	key = mytype = targettype = name = value = NULL;
	int	st = 1;

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here,
		//  and they are at the end of the routine
	switch(op_type) {
	case CondorLogOp_NewClassAd:
		if (caLogParser->getNewClassAdBody(key, mytype, targettype) < 0)
			return 0; 

		st = processNewClassAd(key, mytype, targettype, exec_later);
			
		break;
	case CondorLogOp_DestroyClassAd:
		if (caLogParser->getDestroyClassAdBody(key) < 0)
			return 0;
			
		st = processDestroyClassAd(key, exec_later);
			
		break;
	case CondorLogOp_SetAttribute:
		if (caLogParser->getSetAttributeBody(key, name, value) < 0)
			return 0;

		st = processSetAttribute(key, name, value, exec_later);
			
		break;
	case CondorLogOp_DeleteAttribute:
		if (caLogParser->getDeleteAttributeBody(key, name) < 0)
			return 0;
			
		st = processDeleteAttribute(key, name, exec_later);
		
		break;
	case CondorLogOp_BeginTransaction:
		st = processBeginTransaction(exec_later);

		break;
	case CondorLogOp_EndTransaction:
		st = processEndTransaction(exec_later);

		break;
	default:
		dprintf(D_ALWAYS, "[QUILL] Unsupported Job Queue Command [%d]\n", op_type);
		return 0;
		break;
	}

		// pointers are release
	if (key != NULL) free(key);
	if (mytype != NULL) free(mytype);
	if (targettype != NULL) free(targettype);
	if (name != NULL) free(name);
	if (value != NULL) free(value);

	return st;
}

/*! display a verbose error message
 */
void
JobQueueDBManager::displayDBErrorMsg(const char* errmsg)
{
	dprintf(D_ALWAYS, "[QUILL] %s\n", errmsg);
	dprintf(D_ALWAYS, "\t%s\n", jqDatabase->getDBError());
}

/*! separate a key into Cluster Id and Proc Id 
 *  \return key type 
 *			1: when it is a cluster id
 *			2: when it is a proc id
 * 			0: it fails
 *
 *	\warning The memories of cid and pid should be allocated in advance.
 */
int
JobQueueDBManager::getProcClusterIds(const char* key, char* cid, char* pid)
{
	int key_len, i;
	long iCid;
	char*	pid_in_key;

	if (key == NULL) 
		return 0;

	key_len = strlen(key);

	for (i = 0; i < key_len; i++) {
		if(key[i]  != '.')	
			cid[i]=key[i];
		else {
			cid[i] = '\0';
			break;
		}
	}

		// In case the key doesn't include "."
	if (i == key_len)
		return 0; // Error

		// These two lines are for removing a leading zero.
	iCid = atol(cid);
	sprintf(cid,"%ld", iCid);


	pid_in_key = (char*)(key + (i + 1));
	strcpy(pid, pid_in_key);

	if (atol(pid) == -1) // Cluster ID
		return 1;	

	return 2; // Proc ID
}

/*! process NewClassAd command, working with DBMS
 *  \param key key
 *  \param mytype mytype
 *  \param ttype targettype
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 */
int 
JobQueueDBManager::processNewClassAd(char* key, char* mytype, char* ttype, bool exec_later)
{
	char sql_str1[409600];
	char sql_str2[409600];
	char cid[512];
	char pid[512];
	int  id_sort;

		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);

	switch(id_sort) {
	case 1:
		sprintf(sql_str1, 
				"INSERT INTO ClusterAds_Str (cid, attr, val) VALUES (%s, 'MyType', '\"%s\"');", cid, mytype);

		sprintf(sql_str2, 
				"INSERT INTO ClusterAds_Str (cid, attr, val) VALUES (%s, 'TargetType', '\"%s\"');", cid, ttype);

		break;
	case 2:
		sprintf(sql_str1, 
				"INSERT INTO ProcAds_Str (cid, pid, attr, val) VALUES (%s, %s, 'MyType', '\"Job\"');", cid, pid);

		sprintf(sql_str2, 
				"INSERT INTO ProcAds_Str (cid, pid, attr, val) VALUES (%s, %s, 'TargetType', '\"Machine\"');", cid, pid);

		break;
	case 0:
		dprintf(D_ALWAYS, "New ClassAd Processing --- ERROR\n");
		return 0; // return a error code, 0
		break;
	}


	if (exec_later == false) { // execute them now
		if (jqDatabase->execCommand(sql_str1) < 0) {
			displayDBErrorMsg("New ClassAd Processing --- ERROR");
			return 0; // return a error code, 0
		}
		if (jqDatabase->execCommand(sql_str2) < 0) {
			displayDBErrorMsg("New ClassAd Processing --- ERROR");
			return 0; // return a error code, 0
		}
	}
	else {
		if (multi_sql_str != NULL) { // append them to a SQL buffer
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + strlen(sql_str1) + strlen(sql_str2) + 1);
			strcat(multi_sql_str, sql_str1);
			strcat(multi_sql_str, sql_str2);
		}
		else {
			multi_sql_str = (char*)malloc(
										  strlen(sql_str1) + strlen(sql_str2) + 1);
			sprintf(multi_sql_str,"%s%s", sql_str1, sql_str2);
		}
	}		

	return 1;
}

/*! process DestroyClassAd command, working with DBMS
 *  Also responsible for writing history records
 * 
 *  Note: Currently we can obtain a 'fairly' accurate view of the history.
 *  'fairly' because 
 *  a) we do miss attributes which the schedd puts in the 
 *  history file itself, and the job_queue.log file is unaware of these
 *  attributes.  Since our history capturing scheme is via the job_queue.log
 *  file, we miss these attributes.  On the UWisc pool, this misses 3 
 *  attributes: LastMatchTime, NumJobMatches, and WantMatchDiagnostics
 *
 *  b) Also, some jobs do not get into the history tables  in the following 
 *  rare case:
 *  Quill is unoperational for whatever reason, jobs complete execution and
 *  get deleted from the queue, the job queue log gets truncated, and quill
 *  wakes back up.  Since the job queue log is truncated, quill is blissfully
 *  unaware of the jobs that finished while it was not operational.
 *  A possible fix for this is that the schedd should not truncate job logs
 *  when Quill is unoperational, however, we want Quill to be as independent
 *  as possible, and as such, for now, we live with this rare anomaly.
 * 
 *  \param key key
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 */
int 
JobQueueDBManager::processDestroyClassAd(char* key, bool exec_later)
{
	char sql_str1[1024]; 
	char sql_str2[1024]; 
	char sql_str3[1024];
	char sql_str4[2048];
	char cid[100];
	char pid[100];
	bool inserthistory = false;
	int  id_sort;
	int  st1,st2;

  
		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);
  
	switch(id_sort) {
	case 1:	// ClusterAds
		sprintf(sql_str1, 
				"DELETE FROM ClusterAds_Str WHERE cid = %s;", cid);
    
		sprintf(sql_str2, 
				"DELETE FROM ClusterAds_Num WHERE cid = %s;", cid);
		break;
    case 2:

			/* the following ugly looking, however highly efficient SQL
			   does the following:
			   a) Get all rows from the ProcAds tables belonging to this job
			   b) Add to that all rows from ClusterAds table belonging
			      to this job and those that aren't already in ProcAds 
			   c) Do not get attributes which belong to the horizontal schema

			   Note that while combining rows from the ProcAds and ClusterAds 
			   views, we do a UNION ALL instead UNION since it avoids an
			   expensive sort and the rows are distinct anyway due to the IN 
			   constraint
			*/
		sprintf(sql_str3, 
				"INSERT INTO History_Vertical(cid,pid,attr,val) SELECT cid,pid,attr,val FROM (SELECT cid,pid,attr,val FROM ProcAds WHERE cid= %s and pid = %s UNION ALL SELECT cid,%s,attr,val FROM ClusterAds WHERE cid=%s and attr not in (select attr from ProcAds where cid =%s and pid =%s)) as T WHERE attr not in('ClusterId','ProcId','Owner','QDate','RemoteWallClockTime','RemoteUserCpu','RemoteSysCpu','ImageSize','JobStatus','JobPrio','Cmd','CompletionDate','LastRemoteHost');"
				,cid,pid,pid,cid,cid,pid); 


			/* the following ugly looking, however highly efficient SQL
			   does the following:
			   a) Get all rows from the ProcAds tables belonging to this job
			   b) Add to that all rows from ClusterAds table belonging
			      to this job and those that aren't already in ProcAds 
			   c) Horizontalize this schema.  This is done via case statements
			   exploiting the highly clever, however, highly postgres specific
			   feature that NULL values come before non NULL values and as such
			   the MAX gives us exactly what we want - the non NULL value

			   Note that while combining rows from the ProcAds and ClusterAds 
			   views, we do a UNION ALL instead UNION since it avoids an
			   expensive sort and the rows are distinct anyway due to the IN 
			   constraint
			*/
		sprintf(sql_str4, 
				"INSERT INTO History_Horizontal(cid,pid,\"Owner\",\"QDate\",\"RemoteWallClockTime\",\"RemoteUserCpu\",\"RemoteSysCpu\",\"ImageSize\",\"JobStatus\",\"JobPrio\",\"Cmd\",\"CompletionDate\",\"LastRemoteHost\") SELECT %s,%s, max(CASE WHEN attr='Owner' THEN val ELSE NULL END), max(CASE WHEN attr='QDate' THEN cast(val as integer) ELSE NULL END), max(CASE WHEN attr='RemoteWallClockTime' THEN cast(val as integer) ELSE NULL END), max(CASE WHEN attr='RemoteUserCpu' THEN cast(val as float) ELSE NULL END), max(CASE WHEN attr='RemoteSysCpu' THEN cast(val as float) ELSE NULL END), max(CASE WHEN attr='ImageSize' THEN cast(val as integer) ELSE NULL END), max(CASE WHEN attr='JobStatus' THEN cast(val as integer) ELSE NULL END), max(CASE WHEN attr='JobPrio' THEN cast(val as integer) ELSE NULL END), max(CASE WHEN attr='Cmd' THEN val ELSE NULL END), max(CASE WHEN attr='CompletionDate' THEN cast(val as integer) ELSE NULL END), max(CASE WHEN attr='LastRemoteHost' THEN val ELSE NULL END) FROM (SELECT cid,pid,attr,val FROM ProcAds WHERE cid=%s and pid=%s UNION ALL SELECT cid,%s,attr,val FROM ClusterAds WHERE cid=%s and attr not in (select attr from procads where cid =%s and pid =%s)) as T GROUP BY cid,pid;"
				,cid,pid,cid,pid,pid,cid,cid,pid); 


		inserthistory = true;

			//now that we've inserted rows into history, we can safely 
			//delete them from the procads tables
		sprintf(sql_str1, 
				"DELETE FROM ProcAds_Str WHERE cid = %s AND pid = %s;", 
				cid, pid);
    
		sprintf(sql_str2, 
				"DELETE FROM ProcAds_Num WHERE cid = %s AND pid = %s;", 
				cid, pid);
		break;
	case 0:
		dprintf(D_ALWAYS, "[QUILL] Destroy ClassAd --- ERROR\n");
		return 0; // return a error code, 0
		break;
	}
  
	if (exec_later == false) {

		if(inserthistory) { 
			st1 = jqDatabase->execCommand(sql_str3);
			st2 = jqDatabase->execCommand(sql_str4);
		}
    
	
		if (jqDatabase->execCommand(sql_str1) < 0) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			return 0; // return a error code, 0
		}
		if (jqDatabase->execCommand(sql_str2) < 0) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			return 0; // return a error code, 0
		}
	}
	else {
		if (multi_sql_str != NULL) {
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + strlen(sql_str1) + strlen(sql_str2) + 1);
			strcat(multi_sql_str, sql_str1);
			strcat(multi_sql_str, sql_str2);
		}
		else {
			multi_sql_str = (char*)malloc(
										  strlen(sql_str1) + strlen(sql_str2) + 1);
			sprintf(multi_sql_str, "%s%s", sql_str1, sql_str2);
		}
	}
  
	return 1;
}

/*! process SetAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \param value attribute value
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 *	Note:
 *	Because this is not just update, but set. So, we need to delete and insert
 *  it.  We twiddled with an alternative way to do it (using NOT EXISTS) but
 *  found out that DELETE/INSERT works as efficiently.  the old sql is kept
 *  around in case :)
 */
int 
JobQueueDBManager::processSetAttribute(char* key, char* name, char* value, bool exec_later)
{
	char sql_str_del_in[512];

	char cid[512];
	char pid[512];
	int  id_sort;
	char*	endptr;
	char*   newvalue;
		//int		ret_st;
  
	memset(sql_str_del_in, 0, 512);
  
#ifdef _DEBUG_LOG_ENTRY
	printf("%d %s %s %s\n", CondorLogOp_SetAttribute, key, name, value);
#endif
  
	newvalue = fillEscapeCharacters(value);
		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);
  
	switch(id_sort) {
	case 1:
		if (strtod(value, &endptr) != 0) {
				/*
				  sprintf(sql_str_in, 
				  "INSERT INTO ClusterAds_Num (cid, attr, val) VALUES ('%s', '%s', %s);", cid, name, value);
				  sprintf(sql_str_up, 
				  "UPDATE ClusterAds_Num SET val = %s WHERE cid = '%s' AND attr = '%s';", value, cid, name);
				*/
      
			sprintf(sql_str_del_in, 
					"DELETE FROM ClusterAds_Num WHERE cid = %s AND attr = '%s'; INSERT INTO ClusterAds_Num (cid, attr, val) VALUES (%s, '%s', %s);", cid, name, cid, name, value);
				/*sprintf(sql_str_in, 
				  "INSERT INTO ClusterAds_Num (cid, attr, val) SELECT '%s', '%s', %s WHERE NOT EXISTS (SELECT * FROM ClusterAds_Num WHERE 
				  cid='%s' AND attr='%s');", cid, name, value, cid, name);*/
				/*sprintf(sql_str_in, 
				  "INSERT INTO ClusterAds_Num (cid, attr, val) VALUES ('%s', '%s', %s);", cid, name, value);*/
				//sprintf(sql_str_up, 
				//	      "UPDATE ClusterAds_Num SET val = %s WHERE cid = '%s' AND attr = '%s';", value, cid, name);

		}
		else {
			if (value != endptr) { // the string means number zero.
					/*
					  sprintf(sql_str_in, 
					  "INSERT INTO ClusterAds_Num (cid, attr, val) VALUES ('%s', '%s', %s);", cid, name, value);
					  sprintf(sql_str_up, 
					  "UPDATE ClusterAds_Num SET val = %s WHERE cid = '%s' AND attr = '%s';", value, cid, name);
					*/
	
				sprintf(sql_str_del_in, 
						"DELETE FROM ClusterAds_Num WHERE cid = %s AND attr = '%s'; INSERT INTO ClusterAds_Num (cid, attr, val) VALUES (%s, '%s', %s);", cid, name, cid, name, value);
					/*sprintf(sql_str_in, 
					  "INSERT INTO ClusterAds_Num (cid, attr, val) SELECT '%s', '%s', %s WHERE NOT EXISTS (SELECT * FROM ClusterAds_Num WHERE 
					  cid='%s' AND attr='%s');", cid, name, value, cid, name);*/
					/*sprintf(sql_str_in, 
					  "INSERT INTO ClusterAds_Num (cid, attr, val) VALUES ('%s', '%s', %s);", cid, name, value);*/
					//sprintf(sql_str_up, 
					//	      "UPDATE ClusterAds_Num SET val = %s WHERE cid = '%s' AND attr = '%s';", value, cid, name);
	
			}
			else if (value == endptr) { // the string is not a number.
					/*
					  sprintf(sql_str_in, 
					  "INSERT INTO ClusterAds_Str (cid, attr, val) VALUES ('%s', '%s', '%s');", cid, name, value);
					  sprintf(sql_str_up, 
					  "UPDATE ClusterAds_Str SET val = '%s' WHERE cid = '%s' AND attr = '%s';", value, cid, name);
					*/
	
	
				sprintf(sql_str_del_in, 
						"DELETE FROM ClusterAds_Str WHERE cid = %s AND attr = '%s'; INSERT INTO ClusterAds_Str (cid, attr, val) VALUES (%s, '%s', '%s');", cid, name, cid, name, newvalue);

					/*sprintf(sql_str_in, 
					  "INSERT INTO ClusterAds_Str (cid, attr, val) SELECT '%s', '%s', '%s' WHERE NOT EXISTS (SELECT * FROM ClusterAds_Str 
					  WHERE cid='%s' AND attr='%s');", cid, name, value, cid, name);*/
					/*  sprintf(sql_str_in, 
						"INSERT INTO ClusterAds_Str (cid, attr, val) VALUES ('%s', '%s', '%s');", cid, name, value);*/
					//sprintf(sql_str_up, 
					//	      "UPDATE ClusterAds_Num SET val = %s WHERE cid = '%s' AND attr = '%s';", value, cid, name);
			}
			break;
		case 2:
			if (strtod(value, &endptr) != 0) {
					/*
					  sprintf(sql_str_in, 
					  "INSERT INTO ProcAds_Num (cid, pid, attr, val) VALUES ('%s', '%s', '%s', %s);", cid, pid, name, value);
					  sprintf(sql_str_up, 
					  "UPDATE ProcAds_Num SET val = %s WHERE cid = '%s' AND pid = '%s' AND attr = '%s';", value, cid, pid, name);
					*/
      
      
				sprintf(sql_str_del_in, 
						"DELETE FROM ProcAds_Num WHERE cid = %s AND pid = %s AND attr = '%s'; INSERT INTO ProcAds_Num (cid, pid, attr, val) VALUES (%s, %s, '%s', %s);", cid, pid, name, cid, pid, name, value);
					/*sprintf(sql_str_in, 
					  "INSERT INTO ProcAds_Num (cid, pid, attr, val) SELECT '%s', '%s', '%s',%s WHERE NOT EXISTS (SELECT * FROM ProcAds_Num 
					  WHERE cid='%s' AND pid='%s' AND attr='%s');", cid, pid, name, value, cid, pid, name);*/
					/*sprintf(sql_str_in, 
					  "INSERT INTO ProcAds_Num (cid, pid, attr, val) VALUES ('%s', '%s', '%s', %s);", cid, pid, name, value);*/

			}
			else {
				if (value != endptr) { // the string means number zero.
						/*
						  sprintf(sql_str_in, 
						  "INSERT INTO ProcAds_Num (cid, pid, attr, val) VALUES ('%s', '%s', '%s', %s);", cid, pid, name, value);
						  sprintf(sql_str_up, 
						  "UPDATE ProcAds_Num SET val = %s WHERE cid = '%s' AND pid = '%s' AND attr = '%s';", value, cid, pid, name);
						*/
	
					sprintf(sql_str_del_in, 
							"DELETE FROM ProcAds_Num WHERE cid = %s AND pid = %s AND attr = '%s'; INSERT INTO ProcAds_Num (cid, pid, attr, val) VALUES (%s, %s, '%s', %s);", cid, pid, name, cid, pid, name, value);
						/*sprintf(sql_str_in, 
						  "INSERT INTO ProcAds_Num (cid, pid, attr, val) SELECT '%s', '%s', '%s',%s WHERE NOT EXISTS (SELECT * FROM ProcAds_Num 
						  WHERE cid='%s' AND pid='%s' AND attr='%s');", cid, pid, name, value, cid, pid, name);*/
						/*sprintf(sql_str_in, 
						  "INSERT INTO ProcAds_Num (cid, pid, attr, val) VALUES ('%s', '%s', '%s', %s);", cid, pid, name, value);*/


				}
				else if (value == endptr) { // the string is not a number.
						/*
						  sprintf(sql_str_in, 
						  "INSERT INTO ProcAds_Str (cid, pid, attr, val) VALUES ('%s', '%s', '%s', '%s');", cid, pid, name, value);
						  sprintf(sql_str_up, 
						  "UPDATE ProcAds_Str SET val = '%s' WHERE cid = '%s' AND pid = '%s' AND attr = '%s';", value, cid, pid, name);
						*/
	
					sprintf(sql_str_del_in, 
							"DELETE FROM ProcAds_Str WHERE cid = %s AND pid = %s AND attr = '%s'; INSERT INTO ProcAds_Str (cid, pid, attr, val) VALUES (%s, %s, '%s', '%s');", cid, pid, name, cid, pid, name, newvalue);
						/*sprintf(sql_str_in, 
						  "INSERT INTO ProcAds_Str (cid, pid, attr, val) SELECT '%s', '%s', '%s','%s' WHERE NOT EXISTS (SELECT * FROM ProcAds_Str 
						  WHERE cid='%s' AND pid='%s' AND attr='%s');", cid, pid, name, value, cid, pid, name);*/
						/*  sprintf(sql_str_in, 
							"INSERT INTO ProcAds_Str (cid, pid, attr, val) VALUES ('%s', '%s', '%s', '%s');", cid, pid, name, value);*/

	
				}
			}

			break;
		case 0:
			dprintf(D_ALWAYS, "Set Attribute Processing --- ERROR\n");
			return 0;
			break;
		}
	}
  
	int ret_st = 0;

	if (exec_later == false) {
		ret_st = jqDatabase->execCommand(sql_str_del_in);

		if (ret_st < 0) {
			dprintf(D_ALWAYS, "Set Attribute --- Error [SQL] %s\n", sql_str_del_in);
			displayDBErrorMsg("Set Attribute --- ERROR");      
			return 0;
		}
	}
	else {
		if (multi_sql_str != NULL) {
				// NOTE:
				// this case is not trivial 
				// because there could be multiple insert
				// statements.
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + strlen(sql_str_del_in) + 1);
			strcat(multi_sql_str, sql_str_del_in);
		}
		else {
			multi_sql_str = (char*)malloc(
										  strlen(sql_str_del_in) + 1);
			strcpy(multi_sql_str, sql_str_del_in);
		}    
	}
  
	if(newvalue != NULL) free(newvalue);
	return 1;
}


/*! process DeleteAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 */
int 
JobQueueDBManager::processDeleteAttribute(char* key, char* name, bool exec_later)
{
	char sql_str1[512];
	char sql_str2[512];
	char cid[512];
	char pid[512];
	int  id_sort;
	int		ret_st;

	memset(sql_str1, 0, 512);
	memset(sql_str2, 0, 512);

// Debugging purpose
// printf("%d %s %s\n", CondorLogOp_DeleteAttribute, key, name);

		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);

	switch(id_sort) {
	case 1:
		sprintf(sql_str1, 
				"DELETE FROM ClusterAds_Str WHERE cid = %s AND attr = '%s';", cid, name);
		sprintf(sql_str2, 
				"DELETE FROM ClusterAds_Num WHERE cid = %s AND attr = '%s';", cid, name);

		break;
	case 2:
		sprintf(sql_str1, 
				"DELETE FROM ProcAds_Str WHERE cid = %s AND pid = %s AND attr = '%s';", cid, pid, name);
		sprintf(sql_str2, 
				"DELETE FROM ProcAds_Num WHERE cid = %s AND pid = %s AND attr = '%s';", cid, pid, name);

		break;
	case 0:
		dprintf(D_ALWAYS, "Delete Attribute Processing --- ERROR\n");
		return 0;
		break;
	}

	if (sql_str1 != NULL && sql_str2 != NULL) {
		if (exec_later == false) {
			ret_st = jqDatabase->execCommand(sql_str1);
		
			if (ret_st < 0) {
				dprintf(D_ALWAYS, "Delete Attribute --- ERROR, [SQL] %s\n", sql_str1);
				displayDBErrorMsg("Delete Attribute --- ERROR");
				return 0;
			}
			else if (ret_st == 0) {
				ret_st = jqDatabase->execCommand(sql_str2);
			
				if (ret_st < 0) {
					dprintf(D_ALWAYS, "Delete Attribute --- ERROR [SQL] %s\n", sql_str2);
					displayDBErrorMsg("Delete Attribute --- ERROR");
					return 0;
				}
			}
		}
		else {
			if (multi_sql_str != NULL) {
				multi_sql_str = (char*)realloc(multi_sql_str, 
											   strlen(multi_sql_str) + strlen(sql_str1) + strlen(sql_str2) + 1);
				strcat(multi_sql_str, sql_str1);
				strcat(multi_sql_str, sql_str2);
			}
			else {
				multi_sql_str = (char*)malloc(
											  strlen(sql_str1) + strlen(sql_str2) + 1);
				sprintf(multi_sql_str, "%s%s", sql_str1, sql_str2);
			}
		}		
	}

	return 1;
}

/*! process BeginTransaction command
 *  \return the result status
 *			1: success
 */
int 
JobQueueDBManager::processBeginTransaction(bool exec_later)
{
	xactState = BEGIN_XACT;
	if(!exec_later) {
		if (jqDatabase->beginTransaction() == 0) // this conn is not in Xact,
			return 0;			   				 // so begin Xact!
	}
	return 1;
}

/*! process EndTransaction command
 *  \return the result status
 *			1: success
 */
int 
JobQueueDBManager::processEndTransaction(bool exec_later)
{
	xactState = COMMIT_XACT;
	if(!exec_later) {
		if (jqDatabase->commitTransaction() == 0) // this conn is not in Xact,
			return 0;			   				 // so begin Xact!
	}
	return 1;
}

//! initialize: currently check the DB schema
/*! \param initJQDB initialize DB?
 */
int
JobQueueDBManager::init(bool initJQDB)
{
	if (initJQDB == true) { // initialize Job Queue DB
		if (checkSchema() == 0)
			return 0;
		prober->probe();
		return initJobQueueTables();
	}
	else
		return checkSchema();
}


//! get Last Job Queue File Polling Information
/*! \param mtime modification time
 *  \param size  file size
 *  \param lcmd  last classad log entry
 */
int
JobQueueDBManager::getJQPollingInfo(long& mtime, long& size, ClassAdLogEntry* lcmd)
{
	char 	sql_str[1024];
	int		ret_st;

	memset(sql_str, 0, 1024);

	if (lcmd == NULL)
		lcmd = caLogParser->getCurCALogEntry();


	dprintf(D_FULLDEBUG, "Get JobQueue Polling Information\n");
	
	sprintf(sql_str, "SELECT last_file_mtime, last_file_size, last_next_cmd_offset, last_cmd_offset, last_cmd_type, last_cmd_key, last_cmd_mytype, last_cmd_targettype, last_cmd_name, last_cmd_value from JobQueuePollingInfo;");

		// connect to DB
	ret_st = connectDB();

	if(ret_st <= 0) 
		return ret_st;
	
	if (sql_str != NULL) {
		ret_st = jqDatabase->execQuery(sql_str);
		
		if (ret_st < 0) {
			dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", sql_str);
			displayDBErrorMsg("Update JobQueuePollInfo --- ERROR");
			return 0;
		}
		else if (ret_st == 0) {
				// This case shouldn't happen
				// since the tuple should be inserted 
				// when this table is initialized.
				// So... what?
				// This is the ERROR!
			
			dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", sql_str);
			displayDBErrorMsg("Update JobQueuePollInfo --- ERROR");
		} 

		mtime = atoi(jqDatabase->getValue(0,0)); // last_file_mtime
		size = atoi(jqDatabase->getValue(0,1)); // last_file_size
		lcmd->next_offset = atoi(jqDatabase->getValue(0,2)); // last_next_cmd_offset
		lcmd->offset = atoi(jqDatabase->getValue(0,3)); // last_cmd_offset
		lcmd->op_type = atoi(jqDatabase->getValue(0,4)); // last_cmd_type
		lcmd->key = strdup(jqDatabase->getValue(0,5)); // last_cmd_key
		lcmd->mytype = strdup(jqDatabase->getValue(0,6)); // last_cmd_mytype
		lcmd->targettype = strdup(jqDatabase->getValue(0,7)); // last_cmd_targettype
		lcmd->name = strdup(jqDatabase->getValue(0,8)); // last_cmd_name
		lcmd->value = strdup(jqDatabase->getValue(0,9)); // last_cmd_value

		jqDatabase->releaseQueryResult(); // release Query Result
										  // since it is no longer needed
	}

		// disconnect to DB
	disconnectDB();
	return 1;	
}

void 
JobQueueDBManager::addJQPollingInfoSQL(char* dest, char* src_name, char* src_val)
{
	if (src_name != NULL && src_val != NULL) {
		strcat(dest, ", ");
		strcat(dest, src_name);
		strcat(dest, " = '");
		strcat(dest, src_val);
		strcat(dest, "'");
	}
}

//! set Current Job Queue File Polling Information
/*! \param mtime modification time
 *  \param size  file size
 *  \param lcmd  last classad log entry
 *  \warning This method must be called between connectDB and disconnectDB
 *           ,which means this method doesn't invoke thoses two methods
 */
int
JobQueueDBManager::setJQPollingInfo(long mtime, long size, ClassAdLogEntry* lcmd)
{
	char 	sql_str[1024];
	int		ret_st;


	if (lcmd == NULL)
		lcmd = caLogParser->getCurCALogEntry();	

	memset(sql_str, 0, 1024);

	sprintf(sql_str, 
			"UPDATE JobQueuePollingInfo SET last_file_mtime = %ld, last_file_size = %ld, last_next_cmd_offset = %ld, last_cmd_offset = %ld, last_cmd_type = %d", mtime, size, lcmd->next_offset, lcmd->offset, lcmd->op_type);

	addJQPollingInfoSQL(sql_str, "last_cmd_key", lcmd->key);
	addJQPollingInfoSQL(sql_str, "last_cmd_mytype", lcmd->mytype);
	addJQPollingInfoSQL(sql_str, "last_cmd_targettype", lcmd->targettype);
	addJQPollingInfoSQL(sql_str, "last_cmd_name", lcmd->name);
	addJQPollingInfoSQL(sql_str, "last_cmd_value", lcmd->value);
	strcat(sql_str, ";");

	if (sql_str != NULL) {
		ret_st = jqDatabase->execCommand(sql_str);
		
		if (ret_st < 0) {
			dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", sql_str);
			displayDBErrorMsg("Update JobQueuePollInfo --- ERROR");
			return 0;
		}
		else if (ret_st == 0) {
				// This case shouldn't happen
				// since the tuple should be inserted 
				// when this table is initialized.
				// So... what?
				// This is the ERROR!
			
			dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", sql_str);
			displayDBErrorMsg("Update JobQueuePollInfo --- ERROR");
		} 
	}

	return 1;
}

/*! check the DB schema
 *  \return the result status
 *			0: error
 *			1: success
 */
int
JobQueueDBManager::checkSchema()
{
	char 	sql_str[1024]; 
	
	int	ret_st;
	
		//
		// DB schema check should be done here 
		//
		// 1. check the number of tables 
		// 2. check the list of tables
		// 3. check the one tuple of JobQueuePollingInfo table 
	ret_st = connectDB(NOT_IN_XACT);
	
		// no database or no server connection
	if(ret_st <= 0) { 
		
			//10 related to strlen("template1")
		char *tmp_conn = (char *) malloc(strlen(jobQueueDBConn) + 10); 
		
		strcpy(tmp_conn,jobQueueDBConn);
		char *tmp_found = strstr(tmp_conn, "dbname=");
		strcpy(tmp_found, "dbname=template1");
		dprintf(D_ALWAYS, "tmp = %s\n", tmp_conn);	  
		sprintf(sql_str, "CREATE DATABASE \"%s\"", jobQueueDBName);
		JobQueueDatabase *tmp_jqdb = new PGSQLDatabase(tmp_conn);
		int tmp_st = 0;
		tmp_st = tmp_jqdb->connectDB(tmp_conn);
	  
		if (tmp_st <= 0) { // connect to template1 databae
			dprintf(D_ALWAYS, "Error: Failed while trying to create database %s.\n", jobQueueDBName);
			return 0;
		}
		tmp_st = tmp_jqdb->execCommand(sql_str);
		if (tmp_st < 0) { // executing the create command
			dprintf(D_ALWAYS, "Error: Failed while trying to create database %s.\n", jobQueueDBName);
			return 0;
		}
		tmp_jqdb->disconnectDB();
		delete tmp_jqdb;
		free(tmp_conn);	  
	}

	strcpy(sql_str, SCHEMA_CHECK_STR); // SCHEMA_CHECK_STR is defined in quill_dbschema_def.h

		// execute DB schema check!
	ret_st = jqDatabase->execQuery(sql_str);

	if (ret_st == SCHEMA_SYS_TABLE_NUM) {
		dprintf(D_ALWAYS, "Schema Check OK!\n");
		disconnectDB(NOT_IN_XACT);
	}
	else if (ret_st == 0) { // Schema is not defined in DB
		dprintf(D_ALWAYS, "Schema is not defined!\n");
		dprintf(D_ALWAYS, "Create DB Schema for quill!\n");
		if (jqDatabase->beginTransaction() == 0) // this conn is not in Xact,
			return 0;			   				 // so begin Xact!

			//
			// Here, Create DB Schema:
			//
		strcpy(sql_str, SCHEMA_CREATE_PROCADS_TABLE_STR);
		ret_st = jqDatabase->execCommand(sql_str);
		if(ret_st < 0) {
			disconnectDB(ABORT_XACT);
			return 0;
		}

		strcpy(sql_str, SCHEMA_CREATE_CLUSTERADS_TABLE_STR);
		ret_st = jqDatabase->execCommand(sql_str);
		if(ret_st < 0) {
			disconnectDB(ABORT_XACT);
			return 0;
		}

		strcpy(sql_str, SCHEMA_CREATE_HISTORY_TABLE_STR);
		ret_st = jqDatabase->execCommand(sql_str);
		if(ret_st < 0) {
			disconnectDB(ABORT_XACT);
			return 0;
		}

		strcpy(sql_str, SCHEMA_CREATE_JOBQUEUEPOLLINGINFO_TABLE_STR);
		ret_st = jqDatabase->execCommand(sql_str);
		if(ret_st < 0) {
			disconnectDB(ABORT_XACT);
			return 0;
		}

		disconnectDB(COMMIT_XACT);		
	}
	else { // Unknown error
		dprintf(D_ALWAYS, "Schema Check Unknown Error!\n");
		disconnectDB(NOT_IN_XACT);

		return 0;
	}

	return 1;
}


//! register all timer and command handlers
void
JobQueueDBManager::registerAll()
{
	registerCommands();
	registerTimers();
}

//! register all command handlers
void
JobQueueDBManager::registerCommands()
{
		// register a handler for QMGMT_CMD command from condor_q
	daemonCore->Register_Command(QMGMT_CMD, "QMGMT_CMD",
								 (CommandHandlercpp)&JobQueueDBManager::handle_q,
								 "handle_q", NULL, READ, D_FULLDEBUG);
}

//! register all timer handlers
void
JobQueueDBManager::registerTimers()
{
		// clear previous timers
	if (pollingTimeId >= 0)
		daemonCore->Cancel_Timer(pollingTimeId);
	if (purgeHistoryTimeId >= 0)
		daemonCore->Cancel_Timer(purgeHistoryTimeId);

		// register timer handlers
	pollingTimeId = daemonCore->Register_Timer(0, 
											   pollingPeriod,
											   (Eventcpp)&JobQueueDBManager::pollingTime, 
											   "pollingTime", this);
	purgeHistoryTimeId = daemonCore->Register_Timer(historyCleaningInterval * 3600, 
													historyCleaningInterval * 3600,
													(Eventcpp)&JobQueueDBManager::purgeOldHistoryRows, 
													"purgeOldHistoryRows", this);
}


//! create the SCHEDD_AD sent to the collector
/*! This method reads all quill-related configuration options from the 
 *  config file and creates a classad which can be sent to the collector
 */

void JobQueueDBManager::createClassAd(void) {
	char expr[1000];
	char *mysockname;
	char *schedd_name, *tmp;

	ad = new ClassAd();
	ad->SetMyTypeName(SCHEDD_ADTYPE);
	ad->SetTargetTypeName("");
  
	config_fill_ad(ad);

		// schedd name is needed to identify which schedd is this quill corresponding to
	tmp = param( "SCHEDD_NAME" );
	if( tmp ) {
		schedd_name = build_valid_daemon_name( tmp );
	} else {
		schedd_name = default_daemon_name();
	}  
  
	char *quill_name = param("QUILL_NAME");
	if(!quill_name) {
		dprintf(D_ALWAYS, "Error: Cannot find variable QUILL_NAME in condor_config file - exiting\n");
		exit(1);
	}
	dprintf(D_ALWAYS, "Advertising under name %s\n", quill_name);

	char *is_remotely_queryable = param("QUILL_IS_REMOTELY_QUERYABLE");
	if(!is_remotely_queryable) {
		is_remotely_queryable = strdup("1");
	}
  
	sprintf( expr, "%s = %s", "IsRemotelyQueryable", is_remotely_queryable );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_NAME, quill_name );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_SCHEDD_NAME, schedd_name );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() ); 
	ad->Insert(expr);
  
		// Put in our sinful string.  Note, this is never going to
		// change, so we only need to initialize it once.
	mysockname = strdup( daemonCore->InfoCommandSinfulString() );

	sprintf( expr, "%s = \"%s\"", ATTR_SCHEDD_IP_ADDR, mysockname );
	ad->Insert(expr);
	sprintf( expr, "%s = \"%s\"", ATTR_MY_ADDRESS, mysockname );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", "DatabaseIpAddr", jobQueueDBIpAddress );
	ad->Insert(expr);

	sprintf( expr, "%s = \"%s\"", "DatabaseName", jobQueueDBName );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_NUM_USERS, 1 );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_MAX_JOBS_RUNNING, 500 );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_VIRTUAL_MEMORY, 1967884 );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_TOTAL_IDLE_JOBS, 0 );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_TOTAL_RUNNING_JOBS, 0 );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_TOTAL_JOB_ADS, 0 );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_TOTAL_HELD_JOBS, 0 );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_TOTAL_FLOCKED_JOBS, 0 );
	ad->Insert(expr);

	sprintf( expr, "%s = %d", ATTR_TOTAL_REMOVED_JOBS, 0 );
	ad->Insert(expr);

	collectors = CollectorList::create();
  
	if(tmp) free(tmp);
	if(is_remotely_queryable) free(is_remotely_queryable);
	if(quill_name) free(quill_name);
	if(mysockname) free(mysockname);
}


//! timer handler for each polling event
void
JobQueueDBManager::pollingTime()
{
	dprintf(D_ALWAYS, "******** Start of Probing Job Queue Log File ********\n");

		/*
		  instead of exiting on error, we simply return 
		  this means that quill will not usually exit on loss of 
		  database connectivity, and that it will keep polling
		  and trying to connect until database is back up again 
		  and then resume execution 
		*/
	if (maintain() == 0) {
		dprintf(D_ALWAYS, 
				">>>>>>>> Fail: Probing Job Queue Log File <<<<<<<<\n");
		return;
			//DC_Exit(1);
	}

	dprintf(D_ALWAYS, "********* End of Probing Job Queue Log File *********\n");

	dprintf(D_ALWAYS, "++++++++ Sending schedd ad to collector ++++++++\n");

	if(!ad) createClassAd();
	int num_updates = collectors->sendUpdates ( UPDATE_SCHEDD_AD, ad );
	
	dprintf(D_ALWAYS, "++++++++ Sent schedd ad to collector ++++++++\n");
	

}

//! command handler for QMGMT_CMD command from condor_q
/*!
 *  Much portion of this code was borrowed from handle_q in qmgmt.C which is
 *  part of schedd source code.
 */
int
JobQueueDBManager::handle_q(int, Stream* sock) {
//	---------------- Start of Borrowed Code -------------------
	int rval;

	dprintf(D_ALWAYS, "******** Start of Handling condor_q query ********\n");
#ifdef _REMOTE_DB_CONNECTION_
	dprintf(D_ALWAYS, "querying remote database\n");
	RequestService *rs = new RequestService("host=ad16.cs.wisc.edu port=5432 dbname=quill");
#else
	RequestService *rs = new RequestService("dbname=quill");
#endif

//---    JobQueue->BeginTransaction();

		// store the cluster num so when we commit the transaction, we can easily
		// see if new clusters have been submitted and thus make links to cluster ads
//---    old_cluster_num = next_cluster_num;

		// initialize per-connection variables.  back in the day this
		// was essentially InvalidateConnection().  of particular 
		// importance is setting Q_SOCK... this tells the rest of the QMGMT
		// code the request is from an external user instead of originating
		// from within the schedd itself.
		//Q_SOCK = (ReliSock *)sock;
		//Q_SOCK->unAuthenticate();

//---    active_cluster_num = -1;

    do {
			/* Probably should wrap a timer around this */
        rval = rs->service((ReliSock *)sock);
    } while(rval >= 0);


		// reset the per-connection variables.  of particular 
		// importance is setting Q_SOCK back to NULL. this tells the rest of 
		// the QMGMT code the request originated internally, and it should
		// be permitted (i.e. we only call OwnerCheck if Q_SOCK is not NULL).
		//Q_SOCK->unAuthenticate();
		// note: Q_SOCK is static...
		//Q_SOCK = NULL;

//---    dprintf(D_FULLDEBUG, "QMGR Connection closed\n");

		// Abort any uncompleted transaction.  The transaction should
		// be committed in CloseConnection().
//---    if ( JobQueue->AbortTransaction() ) {
        /*  If we made it here, a transaction did exist that was not
            committed, and we now aborted it.  This would happen if 
            somebody hit ctrl-c on condor_rm or condor_status, etc,
            or if any of these client tools bailed out due to a fatal error.
            Because the removal of ads from the queue has been backed out,
            we need to "back out" from any changes to the ClusterSizeHashTable,
            since this may now contain incorrect values.  Ideally, the size of
            the cluster should just be kept in the cluster ad -- that way, it 
            gets committed or aborted as part of the transaction.  But alas, 
            it is not; same goes a bunch of other stuff: removal of ckpt and 
            ickpt files, appending to the history file, etc.  Sigh.  
            This should be cleaned up someday, probably with the new schedd.
            For now, to "back out" from changes to the ClusterSizeHashTable, we
            use brute force and blow the whole thing away and recompute it. 
            -Todd 2/2000
        */
//---        ClusterSizeHashTable->clear();
//---        ClassAd *ad;
//---        HashKey key;
//---        const char *tmp;
//---        int     *numOfProcs = NULL;
//---        int cluster_num;
//---        JobQueue->StartIterateAllClassAds();
//---        while (JobQueue->IterateAllClassAds(ad,key)) {
//---            tmp = key.value();
//---            if ( *tmp == '0' ) continue;    // skip cluster & header ads
//---            if ( (cluster_num = atoi(tmp)) ) {
		// count up number of procs in cluster, update ClusterSizeHashTable
//---                if ( ClusterSizeHashTable->lookup(cluster_num,numOfProcs) == -1 ) {
		// First proc we've seen in this cluster; set size to 1
//---                    ClusterSizeHashTable->insert(cluster_num,1);
//---                } else {
		// We've seen this cluster_num go by before; increment proc count
//---                    (*numOfProcs)++;
//---                }

//---            }
//---        }
//---    }   // end of if JobQueue->AbortTransaction == True



//	---------------- End of Borrowed Code -------------------




	delete rs;

	dprintf(D_ALWAYS, "******** End   of Handling condor_q query ********\n");

	return TRUE;
}


char * 
JobQueueDBManager::fillEscapeCharacters(char *str) {
	char *newstr;
	char **quotesloc;
	int  quoteslocindex = 0;
	int i=0;

	quotesloc = (char **) malloc(10 * sizeof(char *));
	quotesloc[quoteslocindex] = NULL;

	quotesloc[quoteslocindex] = strchr(str,'\'');
	while (quotesloc[quoteslocindex] != NULL)
		{
				//printf ("found at %d\n",quotesloc[quoteslocindex]-str+1);
			quotesloc[quoteslocindex+1]=strchr(quotesloc[quoteslocindex]+1,'\'');
			quoteslocindex++;
		}

	int strlength = strlen(str);
	newstr = (char *) malloc(strlength+quoteslocindex+1 * sizeof(char));
	int newstrstart=0,strstart=0;

	for(i=0; i < quoteslocindex; i++) {
			//printf("%d %s\n", quotesloc[i]-str+1, quotesloc[i]);
		strncpy(newstr+newstrstart, str+strstart, quotesloc[i]-str-strstart);
		newstrstart = quotesloc[i]-str+i;
		newstr[newstrstart] = '\\';
		newstrstart++;
		strstart = quotesloc[i] - str;
	}
	strncpy(newstr+newstrstart, str+strstart, strlength-strstart);

	int newstrlength = strlength + quoteslocindex;
	newstr[newstrlength] = '\0';

	free(quotesloc);
	return newstr;
    
}
