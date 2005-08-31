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
#include "classadlogentry.h"
#include "prober.h"
#include "classadlogparser.h"
#include "database.h"
#include "pgsqldatabase.h"
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
	if (jobQueueDBUser != NULL)
		free(jobQueueDBUser);
	if (jobQueueDBPasswd != NULL)
		free(jobQueueDBPasswd);

	if (jobQueueDBConn != NULL)
		free(jobQueueDBConn);
	if (multi_sql_str != NULL)
		free(multi_sql_str);
	if (scheddname != NULL)
		free(scheddname);
}

void
JobQueueDBManager::config(bool reconfig) 
{
	char *tmp;

		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the job_queue.log file
	char *spool = param("SPOOL");
	if(!spool) {
		dprintf(D_ALWAYS, 
				"Error: No SPOOL variable found in config file - exiting\n");
		exit(1);
	}
  
	jobQueueLogFile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	snprintf(jobQueueLogFile,_POSIX_PATH_MAX * sizeof(char), "%s/job_queue.log", spool);
	free(spool);

/*
		// get the file name for the log file containing job queue polling info
	pollingInfoLog = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));

	tmp = param("TT_POLLING_INFO_LOG");

	if(!tmp) {
			// use default if not specified
		tmp = param("LOG");
		if (tmp) {
			snprintf(pollingInfoLog, _POSIX_PATH_MAX * sizeof(char), "%s/TTPollingInfoLog", tmp);
			free(tmp);
		}
		else 
			strncpy(pollingInfoLog, "TTPollingInfoLog", _POSIX_PATH_MAX * sizeof(char));
	} else {
		strncpy(pollingInfoLog, tmp, _POSIX_PATH_MAX * sizeof(char));
		free(tmp);
	}
*/

		/*
		  Here we try to read the <ipaddress:port> stored in condor_config
		  if one is not specified, by default we use the local address 
		  and the default postgres port of 5432.  
		*/
	char host[64], port[64];  //used to construct the connection string
	jobQueueDBIpAddress = param("DATABASE_IPADDRESS");
	if(!jobQueueDBIpAddress) {
		jobQueueDBIpAddress = (char *) malloc(128 * sizeof(char));
		strncpy(jobQueueDBIpAddress, daemonCore->InfoCommandSinfulString(), 128 * sizeof(char));
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
	if(!jobQueueDBName) jobQueueDBName = strdup("condor");

	jobQueueDBUser = param("DATABASE_USER");
	if(!jobQueueDBUser) jobQueueDBUser = strdup("scidb");

	jobQueueDBPasswd = param("DATABASE_PASSWORD");
	if(!jobQueueDBPasswd) jobQueueDBPasswd = strdup("scidb");

	jobQueueDBConn = (char *) malloc(500);
	snprintf(jobQueueDBConn, 500, "%s %s dbname=%s user=%s password=%s", host, port, 
			jobQueueDBName, jobQueueDBUser, jobQueueDBPasswd);
  
		// read the polling period and if one is not specified use 
		// default value of 10 seconds
	char *pollingPeriod_str = param("TT_POLLING_PERIOD");
	if(pollingPeriod_str) {
		pollingPeriod = atoi(pollingPeriod_str);
		free(pollingPeriod_str);
	}
	else pollingPeriod = 10;	
	
	dprintf(D_ALWAYS, "Using Job Queue File %s\n", jobQueueLogFile);
	dprintf(D_ALWAYS, "Using Database IpAddress = %s\n", jobQueueDBIpAddress);
	dprintf(D_ALWAYS, "Using Database Name = %s\n", jobQueueDBName);
	dprintf(D_ALWAYS, "Using Database Name = %s\n", jobQueueDBUser);
	dprintf(D_ALWAYS, "Using Database Name = %s\n", jobQueueDBPasswd);
	dprintf(D_ALWAYS, "Using Database Connection String = \"%s\"\n", jobQueueDBConn);
		//dprintf(D_ALWAYS, "Using Polling Info Log = %s\n", pollingInfoLog);	

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
 
	tmp = param( "SCHEDD_NAME" );
	if( tmp ) {
		scheddname = build_valid_daemon_name( tmp );
		free(tmp);
	} else {
		scheddname = default_daemon_name();
	}

	int   len = 1024 + 2*strlen(scheddname);
	char *sql_str = (char *) malloc (len);
	int   ret_st;

	snprintf(sql_str, len, "INSERT INTO jobqueuepollinginfo SELECT '%s', 0, 0 WHERE NOT EXISTS (SELECT * FROM jobqueuepollinginfo WHERE scheddname = '%s');", scheddname, scheddname);

	ret_st = jqDatabase->connectDB(jobQueueDBConn);
	if (ret_st <= 0) {
		displayDBErrorMsg("config: unable to connect to DB--- ERROR");
		free(sql_str);
		return;
	}
		
	ret_st = jqDatabase->execCommand(sql_str);
	if (ret_st <0) {
		dprintf(D_ALWAYS, "Insert JobQueuePollInfo --- ERROR [SQL] %s\n", sql_str);
		displayDBErrorMsg("Insert JobQueuePollInfo --- ERROR");		
	}
	
	snprintf(sql_str, len, "INSERT INTO currency SELECT '%s', NULL WHERE NOT EXISTS (SELECT * FROM currency WHERE datasource = '%s');", scheddname, scheddname);

	ret_st = jqDatabase->execCommand(sql_str);
	if (ret_st <0) {
		dprintf(D_ALWAYS, "Insert Currency --- ERROR [SQL] %s\n", sql_str);
		displayDBErrorMsg("Insert Currency --- ERROR");		
	}
	
	free(sql_str);

	jqDatabase->disconnectDB();
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

		//if we are unable to get to the polling info file, smth is wrong
	if(st <= 0) {
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
	int		i, j, len;
	char   *sql_str[sqlNum];
	
 	len = 128 + strlen(scheddname);

	for (i = 0; i < sqlNum; i++) {
		sql_str[i] = (char *) malloc(len);
	}

		// we only delete job queue related information.
	snprintf(sql_str[0], len,
			"DELETE FROM clusterads_horizontal WHERE scheddname = '%s';", scheddname);
	snprintf(sql_str[1], len,
			"DELETE FROM clusterads_vertical WHERE scheddname = '%s';", scheddname);
	snprintf(sql_str[2], len,
			"DELETE FROM procads_horizontal WHERE scheddname = '%s';", scheddname);
	snprintf(sql_str[3], len,
			"DELETE FROM procads_vertical WHERE scheddname = '%s';", scheddname);

	for (i = 0; i < sqlNum; i++) {
		if (jqDatabase->execCommand(sql_str[i]) < 0) {
			displayDBErrorMsg("Clean UP ALL Data --- ERROR");
			
			for (j = 0; j < sqlNum; j++) {
				free(sql_str[i]);
			}

			return 0; // return a error code, 0
		}
	}

 	for (i = 0; i < sqlNum; i++) {
		free(sql_str[i]);
	}

	return 1;
}

/*! vacuums the job queue related tables 
 */
int
JobQueueDBManager::tuneupJobQueueTables()
{
	int		sqlNum = 4;
	int		i;
	char 	sql_str[sqlNum][128];

		// When a record is deleted, postgres only marks it
		// deleted.  Then space is reclaimed in a lazy fashion,
		// by vacuuming it, and as such we do this here.  
		// vacuuming is asynchronous, but can get pretty expensive
	snprintf(sql_str[0], 128,
			"VACUUM ANALYZE clusterads_horizontal;");
	snprintf(sql_str[1], 128,
			"VACUUM ANALYZE clusterads_vertical;");
	snprintf(sql_str[2], 128,
			"VACUUM ANALYZE procads_horizontal;");
	snprintf(sql_str[3], 128,
			"VACUUM ANALYZE procads_vertical;");

	for (i = 0; i < sqlNum; i++) {
		if (jqDatabase->execCommand(sql_str[i]) < 0) {
			displayDBErrorMsg("VACUUM Database --- ERROR");
			return 0; // return a error code, 0
		}
	}

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
		jqDatabase->commitTransaction(); // commit XACT
		xactState = NOT_IN_XACT;
	} else if (commit == ABORT_XACT) { // abort XACT
		jqDatabase->rollbackTransaction();
		xactState = NOT_IN_XACT;
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
	char	sql_str[1024];
	bool 	bFirst = true;

		//
		// Make a COPY SQL string and load it into ClusterAds_Horizontal table
		//
	jobQueue->initAllJobAdsIteration();

	while((ret_str = jobQueue->getNextClusterAd_H_CopyStr()) != NULL) {

		if ((bFirst == true)&& (ret_str != NULL)) {			
			// we need to issue the COPY command first
			snprintf(sql_str, 1024, "COPY ClusterAds_Horizontal FROM stdin;");
			if (jqDatabase->execCommand(sql_str) < 0) {
				displayDBErrorMsg("COPY ClusterAds_Horizontal --- ERROR");
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
		// Make a COPY SQL string and load it into ClusterAds_Vertical table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextClusterAd_V_CopyStr()) != NULL) {
	  		
		if ((bFirst == true)&& (ret_str != NULL)) {			
			snprintf(sql_str, 1024, "COPY ClusterAds_Vertical FROM stdin;");
			if (jqDatabase->execCommand(sql_str) < 0) {
				displayDBErrorMsg("COPY ClusterAds_Vertical --- ERROR");
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
		// Make a COPY sql string and load it into ProcAds_Horizontal table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextProcAd_H_CopyStr()) != NULL) {
		if ((bFirst == true)&& (ret_str != NULL)) {			
			snprintf(sql_str, 1024, "COPY ProcAds_Horizontal FROM stdin;");
			if (jqDatabase->execCommand(sql_str) < 0) {
				displayDBErrorMsg("COPY ProcAds_Horizontal --- ERROR");
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
		// Make a COPY sql string and load it into ProcAds_Vertical table
		//
	jobQueue->initAllJobAdsIteration();
	while((ret_str = jobQueue->getNextProcAd_V_CopyStr()) != NULL) {
		if ((bFirst == true)&& (ret_str != NULL)) {			
			snprintf(sql_str, 1024, "COPY ProcAds_Vertical FROM stdin;");
			if (jqDatabase->execCommand(sql_str) < 0) {
				displayDBErrorMsg("COPY ProcAds_Vertical --- ERROR");
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
	

#if 0

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

#endif
	
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

		// turn off sequential scan so that the incremental update always use index
		// regardless whether the statistics are correct or not.
	if (jqDatabase->execCommand("set enable_seqscan=false;") < 0) {
		displayDBErrorMsg("Turning off seq scan --- ERROR");
		return 0; // return a error code, 0
	}

		// Process ClassAd Log Entry
	while ((op_type = caLogParser->readLogEntry()) > 0) {
			//dprintf(D_ALWAYS, "processing log record\n");
		if (processLogEntry(op_type, false) == 0)
			return 0; // fail: need to abort Xact
	}

		// turn on sequential scan again
	if (jqDatabase->execCommand("set enable_seqscan=true;") < 0) {
		displayDBErrorMsg("Turning on seq scan --- ERROR");
		return 0; // return a error code, 0
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
		jqDatabase->commitTransaction(); // end XACT
		xactState = NOT_IN_XACT;
		
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
			dprintf(D_ALWAYS, "[QUILL++] New ClassAd --- ERROR\n");
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
			dprintf(D_ALWAYS, "[QUILL++] Destroy ClassAd --- ERROR\n");
			return 0; // return a error code, 0
		}
			
		break;
	}
	case CondorLogOp_SetAttribute: {	
		if (caLogParser->getSetAttributeBody(key, name, value) < 0)
			return 0;
						
			//newvalue = fillEscapeCharacters(value);
		id_sort = getProcClusterIds(key, cid, pid);

		char tmp[512];

		snprintf(tmp, 512, "%s = %s", name, value);

		if (id_sort == 1) { // Cluster Ad
			ClassAd* ad = jobQueue->findClusterAd(cid);
			if (ad != NULL) 
					//ad->Insert(ins_str);
					//ad->Assign(name, newvalue);
				ad->Insert(tmp);
			else
				dprintf(D_ALWAYS, "[QUILL++] ERROR: There is no such Cluster Ad[%s]\n", cid);
		}
		else if (id_sort == 2) { // Proc Ad
			ClassAd* ad = jobQueue->findProcAd(cid, pid);
			if (ad != NULL) 
					//ad->Insert(ins_str);
					//ad->Assign(name, newvalue);
				ad->Insert(tmp);
			else {
				dprintf(D_ALWAYS, "[QUILL++] ERROR: There is no such Proc Ad[%s.%s]\n", cid, pid);
			}
		}
		else { // case 0:  Error
			dprintf(D_ALWAYS, "[QUILL++] Set Attribute --- ERROR\n");
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
			dprintf(D_ALWAYS, "[QUILL++] Delete Attribute --- ERROR\n");
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
		printf("[QUILL++] Unsupported Job Queue Command\n");
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
		dprintf(D_ALWAYS, "[QUILL++] Unsupported Job Queue Command [%d]\n", op_type);
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
	dprintf(D_ALWAYS, "[QUILL++] %s\n", errmsg);
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
	char *sql_str;
	char  cid[512];
	char  pid[512];
	int   id_sort, len;

		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);

	switch(id_sort) {
	case 1:
		len = 1024 + strlen(scheddname) + strlen(cid);
		sql_str = (char *)malloc(len);
		snprintf(sql_str, len,
				"INSERT INTO ClusterAds_Horizontal (scheddname, cid) VALUES ('%s', '%s');", scheddname, cid);

		break;
	case 2:
		len = 1024 + strlen(scheddname) + strlen(cid) + strlen(pid);
		sql_str = (char *)malloc(len);
		snprintf(sql_str, len,
				"INSERT INTO ProcAds_Horizontal (scheddname, cid, pid) VALUES ('%s', '%s', '%s');", scheddname, cid, pid);

		break;
	case 0:
		dprintf(D_ALWAYS, "New ClassAd Processing --- ERROR\n");
		return 0; // return a error code, 0
		break;
	}


	if (exec_later == false) { // execute them now
		if (jqDatabase->execCommand(sql_str) < 0) {
			displayDBErrorMsg("New ClassAd Processing --- ERROR");
			free(sql_str);
			return 0; // return a error code, 0
		}
	}
	else {
		if (multi_sql_str != NULL) { // append them to a SQL buffer
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + strlen(sql_str) + 1);
			strcat(multi_sql_str, sql_str);
		}
		else {
			multi_sql_str = (char*)malloc(
										  strlen(sql_str) + 1);
			sprintf(multi_sql_str,"%s", sql_str);
		}
	}		

	free(sql_str);
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
	char *sql_str1; 
	char *sql_str2; 
	char cid[100];
	char pid[100];
	int  id_sort, len;

  
		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);
  
	switch(id_sort) {
	case 1:	// ClusterAds
		len = 2048+ strlen(scheddname) + strlen(cid);
		sql_str1 = (char *) malloc(len);
		snprintf(sql_str1, len,
				"DELETE FROM ClusterAds_Horizontal WHERE scheddname = '%s' and cid = %s;", scheddname, cid);
    
		sql_str2 = (char *) malloc(len);
		snprintf(sql_str2, len,
				"DELETE FROM ClusterAds_Vertical WHERE scheddname = '%s' and cid = %s;", scheddname, cid);
		break;
        case 2:

#if 0
				/* History jobs are now logged through the new SQL independent log file.
				   Therefore we don't need the following code any more for deriving history
				   jobs from the active jobs when a job is destroyed. But for future reference,
				   we are keeping the code around in case we need them.
				*/

				/* the following ugly looking, however highly efficient SQL
			   does the following:
			   a) Get all rows from the ProcAds_Vertical tables belonging to this job
			   b) Add to that all rows from ClusterAds_Vertical table belonging
			      to this job and those that aren't already in ProcAds 
			   c) Do not get attributes which belong to the horizontal schema

			   Note that while combining rows from the ProcAds_Vertical and ClusterAds_Vertical 
			   views, we do a UNION ALL instead UNION since it avoids an
			   expensive sort and the rows are distinct anyway due to the IN 
			   constraint
			   
			   Another note that although the partitioning between horizontal part
			   and vertical part is different for history, procads and clusterads,
			   the vertical part of history is a subset of both of procads and clusterads,
			   therefore the following gives correct results.
				*/

#define sql_str3_format "INSERT INTO History_Vertical \
SELECT scheddname, cid,pid,attr,val \
FROM (SELECT scheddname, cid,pid,attr,val \
      FROM ProcAds_Vertical \
      WHERE scheddname = '%s' and cid= %s and pid = %s \
      UNION ALL \
      SELECT scheddname, cid,%s,attr,val \
      FROM ClusterAds_Vertical \
      WHERE scheddname = '%s' and cid=%s and \
            attr not in (select attr from ProcAds_vertical where scheddname = '%s' and cid =%s and pid =%s) \
      ) as T \
WHERE attr not in('ClusterId','ProcId','Owner','QDate', 'GlobalJobId', 'NumCkpts', 'NumRestarts', 'NumSystemHolds', 'CondorVersion', 'CondorPlatform', 'RootDir', 'Iwd', 'JobUniverse', 'MinHosts', 'MaxHosts', 'User', 'Env', 'UserLog', 'CoreSize', 'KillSig', 'Rank', 'In', 'TransferIn', 'Out', 'TransferOut', 'Err', 'TransferErr', 'ShouldTransferFiles', 'TransferFiles', 'ExecutableSize', 'DiskUsage', 'Requirements', 'FileSystemDomain', 'Args', 'LastMatchTime', 'JobStartDate', 'JobCurrentStartDate', 'JobRunCount', 'FileReadCount', 'FileReadBytes', 'FileWriteCount', 'FileWriteBytes', 'FileSeekCount', 'TotalSuspensions', 'ExitStatus', 'LocalUserCpu', 'LocalSysCpu', 'BytesSent', 'BytesRecvd', 'RSCBytesSent', 'RSCBytesRecvd', 'ExitCode', 'EnteredCurrentStatus', 'RemoteWallClockTime','RemoteUserCpu','RemoteSysCpu','ImageSize','JobStatus','JobPrio','Cmd','CompletionDate','LastRemoteHost');"


			snprintf(sql_str3, 10240, sql_str3_format, scheddname, cid,pid,pid, scheddname,cid,scheddname, cid,pid); 

				/* the following ugly looking, however highly efficient SQL
			   does the following:
			   a) Get all rows from the ProcAds_vertical tables belonging to this job
			   b) Add to that all rows from ClusterAds_vertical table belonging
			      to this job and those that aren't already in ProcAds 
			   c) Horizontalize this schema.  This is done via case statements
			   exploiting the highly clever, however, highly postgres specific
			   feature that NULL values come before non NULL values and as such
			   the MAX gives us exactly what we want - the non NULL value
			   d) join with procads_horizontal and clusterads_horizontal to 
			   get the value of attributes stored in horizontal tables.

			   Note that while combining rows from the ProcAds and ClusterAds 
			   views, we do a UNION ALL instead UNION since it avoids an
			   expensive sort and the rows are distinct anyway due to the IN 
			   constraint
				*/

#define sql_str4_format "INSERT INTO History_Horizontal \
SELECT \
 V.scheddname,\
 V.cid,\
 V.pid,\
 CH.Qdate,\
 CH.Owner,\
 PH.GlobalJobId,\
 V.NumCkpts,\
 V.NumRestarts,\
 V.NumSystemHolds, \
 V.CondorVersion,\
 V.CondorPlatform,\
 V.RootDir, \
 V.Iwd, \
 V.JobUniverse, \
 CH.Cmd,\
 V.MinHosts,\
 V.MaxHosts,\
 (CASE WHEN V.JobPrio ISNULL THEN CH.jobprio ELSE V.JobPrio END),\
 V.User_j,\
 V.Env,\
 V.UserLog,\
 V.CoreSize,\
 V.KillSig,\
 V.Rank,\
 V.In_j,\
 V.TransferIn,\
 V.Out,\
 V.TransferOut,\
 V.Err,\
 V.TransferErr,\
 V.ShouldTransferFiles,\
 V.TransferFiles,\
 V.ExecutableSize,\
 V.DiskUsage,\
 V.Requirements,\
 V.FileSystemDomain,\
 (CASE WHEN V.Args ISNULL THEN CH.args ELSE V.Args END),\
 (('epoch'::timestamp + cast(V.LastMatchTime || ' seconds' as interval)) at time zone 'UTC'),\
 V.NumJobMatches,\
 (('epoch'::timestamp + cast(V.JobStartDate || ' seconds' as interval)) at time zone 'UTC'),\
 (('epoch'::timestamp + cast(V.JobCurrentStartDate || ' seconds' as interval)) at time zone 'UTC'),\
 V.JobRunCount,\
 V.FileReadCount,\
 V.FileReadBytes,\
 V.FileWriteCount,\
 V.FileWriteBytes,\
 V.FileSeekCount,\
 V.TotalSuspensions,\
 (CASE WHEN PH.imagesize ISNULL THEN CH.imagesize ELSE PH.imagesize END),\
 V.ExitStatus,\
 V.LocalUserCpu,\
 V.LocalSysCpu,\
 (CASE WHEN V.RemoteUserCpu ISNULL THEN CH.remoteusercpu ELSE V.RemoteUserCpu END),\
 V.RemoteSysCpu,\
 V.BytesSent,\
 V.BytesRecvd,\
 V.RSCBytesSent,\
 V.RSCBytesRecvd,\
 V.ExitCode,\
 (CASE WHEN PH.jobstatus ISNULL THEN CH.jobstatus ELSE PH.jobstatus END),\
 (('epoch'::timestamp + cast(V.EnteredCurrentStatus || ' seconds' as interval)) at time zone 'UTC'),\
 (CASE WHEN PH.remotewallclocktime ISNULL THEN CH.remotewallclocktime ELSE PH.remotewallclocktime END),\
 V.LastRemoteHost,\
 (('epoch'::timestamp + cast(V.CompletionDate || ' seconds' as interval)) at time zone 'UTC') \
FROM (select scheddname, cid, pid,\
      max(CASE WHEN attr='QDate' THEN val ELSE NULL END) AS QDate, \
      max(CASE WHEN attr='Owner' THEN val ELSE NULL END) AS Owner, \
      max(CASE WHEN attr='GlobalJobId' THEN val ELSE NULL END) AS GlobalJobId,\
      max(CASE WHEN attr='NumCkpts' THEN val ELSE NULL END) AS NumCkpts,\
      max(CASE WHEN attr='NumRestarts' THEN val ELSE NULL END) AS NumRestarts,\
      max(CASE WHEN attr='NumSystemHolds' THEN val ELSE NULL END) AS NumSystemHolds,\
      max(CASE WHEN attr='CondorVersion' THEN val ELSE NULL END) AS CondorVersion,\
      max(CASE WHEN attr='CondorPlatform' THEN val ELSE NULL END) AS CondorPlatform,\
      max(CASE WHEN attr='RootDir' THEN val ELSE NULL END) AS RootDir,\
      max(CASE WHEN attr='Iwd' THEN val ELSE NULL END) AS Iwd,\
      max(CASE WHEN attr='JobUniverse' THEN val ELSE NULL END) AS JobUniverse,\
      max(CASE WHEN attr='Cmd' THEN val ELSE NULL END) AS Cmd,\
      max(CASE WHEN attr='MinHosts' THEN val ELSE NULL END) AS MinHosts,\
      max(CASE WHEN attr='MaxHosts' THEN val ELSE NULL END) AS MaxHosts,\
      max(CASE WHEN attr='JobPrio' THEN cast(val as integer) ELSE NULL END) AS JobPrio, \
      max(CASE WHEN attr='User' THEN val ELSE NULL END) AS User_j, \
      max(CASE WHEN attr='Env' THEN val ELSE NULL END) AS Env, \
      max(CASE WHEN attr='UserLog' THEN val ELSE NULL END) AS UserLog, \
      max(CASE WHEN attr='CoreSize' THEN val ELSE NULL END) AS CoreSize, \
      max(CASE WHEN attr='KillSig' THEN val ELSE NULL END) AS KillSig, \
      max(CASE WHEN attr='Rank' THEN val ELSE NULL END) AS Rank, \
      max(CASE WHEN attr='In' THEN val ELSE NULL END) AS In_j, \
      max(CASE WHEN attr='TransferIn' THEN val ELSE NULL END) AS TransferIn, \
      max(CASE WHEN attr='Out' THEN val ELSE NULL END) AS Out, \
      max(CASE WHEN attr='TransferOut' THEN val ELSE NULL END) AS TransferOut, \
      max(CASE WHEN attr='Err' THEN val ELSE NULL END) AS Err, \
      max(CASE WHEN attr='TransferErr' THEN val ELSE NULL END) AS TransferErr, \
      max(CASE WHEN attr='ShouldTransferFiles' THEN val ELSE NULL END) AS ShouldTransferFiles, \
      max(CASE WHEN attr='TransferFiles' THEN val ELSE NULL END) AS TransferFiles, \
      max(CASE WHEN attr='ExecutableSize' THEN val ELSE NULL END) AS ExecutableSize,\
      max(CASE WHEN attr='DiskUsage' THEN val ELSE NULL END) AS DiskUsage, \
      max(CASE WHEN attr='Requirements' THEN val ELSE NULL END) AS Requirements, \
      max(CASE WHEN attr='FileSystemDomain' THEN val ELSE NULL END) AS FileSystemDomain, \
      max(CASE WHEN attr='Args' THEN val ELSE NULL END) AS Args, \
      max(CASE WHEN attr='LastMatchTime' THEN val ELSE NULL END) AS LastMatchTime, \
      max(CASE WHEN attr='NumJobMatches' THEN val ELSE NULL END) AS NumJobMatches, \
      max(CASE WHEN attr='JobStartDate' THEN val ELSE NULL END) AS JobStartDate,\
      max(CASE WHEN attr='JobCurrentStartDate' THEN val ELSE NULL END) AS JobCurrentStartDate, \
      max(CASE WHEN attr='JobRunCount' THEN val ELSE NULL END) AS JobRunCount, \
      max(CASE WHEN attr='FileReadCount' THEN val ELSE NULL END) AS FileReadCount, \
      max(CASE WHEN attr='FileReadBytes' THEN val ELSE NULL END) AS FileReadBytes, \
      max(CASE WHEN attr='FileWriteCount' THEN val ELSE NULL END) AS FileWriteCount,\
      max(CASE WHEN attr='FileWriteBytes' THEN val ELSE NULL END) AS FileWriteBytes, \
      max(CASE WHEN attr='FileSeekCount' THEN val ELSE NULL END) AS FileSeekCount, \
      max(CASE WHEN attr='TotalSuspensions' THEN val ELSE NULL END) AS TotalSuspensions, \
      max(CASE WHEN attr='ImageSize' THEN cast(val as integer) ELSE NULL END) AS ImageSize, \
      max(CASE WHEN attr='ExitStatus' THEN val ELSE NULL END) AS ExitStatus, \
      max(CASE WHEN attr='LocalUserCpu' THEN val ELSE NULL END) AS LocalUserCpu, \
      max(CASE WHEN attr='LocalSysCpu' THEN val ELSE NULL END) AS LocalSysCpu,\
      max(CASE WHEN attr='RemoteUserCpu' THEN cast(val as integer) ELSE NULL END) AS RemoteUserCpu, \
      max(CASE WHEN attr='RemoteSysCpu' THEN val ELSE NULL END) AS RemoteSysCpu, \
      max(CASE WHEN attr='BytesSent' THEN val ELSE NULL END) AS BytesSent, \
      max(CASE WHEN attr='BytesRecvd' THEN val ELSE NULL END) AS BytesRecvd, \
      max(CASE WHEN attr='RSCBytesSent' THEN val ELSE NULL END) AS RSCBytesSent, \
      max(CASE WHEN attr='RSCBytesRecvd' THEN val ELSE NULL END) AS RSCBytesRecvd, \
      max(CASE WHEN attr='ExitCode' THEN val ELSE NULL END) AS ExitCode, \
      max(CASE WHEN attr='JobStatus' THEN cast(val as integer) ELSE NULL END) AS JobStatus, \
      max(CASE WHEN attr='EnteredCurrentStatus' THEN val ELSE NULL END) AS EnteredCurrentStatus, \
      max(CASE WHEN attr='RemoteWallClockTime' THEN cast(val as integer) ELSE NULL END) AS RemoteWallClockTime, \
      max(CASE WHEN attr='LastRemoteHost' THEN val ELSE NULL END) AS LastRemoteHost,\
      max(CASE WHEN attr='CompletionDate' THEN val ELSE NULL END)  AS CompletionDate\
      FROM \
        (SELECT scheddname, cid,pid,attr,val \
         FROM ProcAds_vertical \
         WHERE scheddname='%s' and cid=%s and pid=%s \
         UNION ALL \
         SELECT scheddname, cid,%s,attr,val \
         FROM ClusterAds_vertical \
         WHERE scheddname = '%s' and cid=%s and \
               attr not in (select attr from procads_vertical where scheddname='%s' and cid =%s and pid =%s)) as T \
      GROUP BY scheddname, cid,pid) AS V, \
     ProcAds_Horizontal PH, ClusterAds_Horizontal CH \
WHERE V.scheddname = PH.scheddname and V.cid = PH.cid and V.pid = PH.pid AND \
      V.scheddname = CH.scheddname and V.cid = CH.cid;"

			snprintf(sql_str4, 20480, sql_str4_format, scheddname, cid, pid, pid, scheddname, cid, scheddname, cid, pid);

		inserthistory = true;

#endif

			/* generate SQL to remove the job from job tables */
		len = 2048 + strlen(scheddname) + strlen(cid) + strlen(pid);	
		sql_str1 = (char *) malloc(len);
		snprintf(sql_str1, len,
				"DELETE FROM ProcAds_horizontal WHERE scheddname = '%s' and cid = %s AND pid = %s;", 
				 scheddname, cid, pid);
    
		sql_str2 = (char *) malloc(len);
		snprintf(sql_str2, len,
				"DELETE FROM ProcAds_vertical WHERE scheddname = '%s' and cid = %s AND pid = %s;", 
				 scheddname, cid, pid);
		break;
	case 0:
		dprintf(D_ALWAYS, "[QUILL++] Destroy ClassAd --- ERROR\n");
		return 0; // return a error code, 0
		break;
	}
  
	if (exec_later == false) {

#if 0
		if(inserthistory) { 
			st1 = jqDatabase->execCommand(sql_str3);
			st2 = jqDatabase->execCommand(sql_str4);
		}
#endif    
	
		if (jqDatabase->execCommand(sql_str1) < 0) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			free(sql_str1);
			free(sql_str2);
			return 0; // return a error code, 0
		}

		if (jqDatabase->execCommand(sql_str2) < 0) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			free(sql_str1);
			free(sql_str2);
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
  
	free(sql_str1);
	free(sql_str2);
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
	char *sql_str_del_in;

	char cid[512];
	char pid[512];
	int  id_sort, len;
	char *tempvalue = NULL;
		//int		ret_st;
  
		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);
  
	switch(id_sort) {
	case 1:
		len = 2048 + 2*(strlen(scheddname) + strlen(cid) + strlen(name)) + strlen(value);
		sql_str_del_in = (char *) malloc(len);
		if(isHorizontalClusterAttribute(name)) {
			if (strcasecmp(name, "qdate") == 0) {
				snprintf(sql_str_del_in, len,
						 "UPDATE ClusterAds_Horizontal SET %s = (('epoch'::timestamp + '%s seconds') at time zone 'UTC') WHERE scheddname = '%s' and cid = '%s';", name, value, scheddname, cid);
			} else {
				tempvalue = (char *) malloc(strlen(value) + 1);
				strcpy(tempvalue, value);
				strip_double_quote(tempvalue);
				snprintf(sql_str_del_in, len,
						 "UPDATE ClusterAds_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cid = '%s';", name, tempvalue, scheddname, cid);				
			}
		} else {
			tempvalue = (char *) malloc(strlen(value) + 1);
			strcpy(tempvalue, value);
			strip_double_quote(tempvalue);
            snprintf(sql_str_del_in, len,
					 "DELETE FROM ClusterAds_Vertical WHERE scheddname = '%s' and cid = '%s' AND attr = '%s'; INSERT INTO ClusterAds_Vertical (scheddname, cid, attr, val) VALUES ('%s', '%s', '%s', '%s');", scheddname, cid, name, scheddname, cid, name, tempvalue);
		}

		break;
	case 2:
		len = 2048 + 2*(strlen(scheddname) + strlen(cid) + strlen(pid) + strlen(name)) + strlen(value);
		sql_str_del_in = (char *) malloc(len);
		tempvalue = (char *) malloc(strlen(value) + 1);

		if(isHorizontalProcAttribute(name)) {
			strcpy(tempvalue, value);
			strip_double_quote(tempvalue);
			snprintf(sql_str_del_in, len,
					 "UPDATE ProcAds_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cid = '%s' and pid = '%s';", name, tempvalue, scheddname, cid, pid);
		} else {
			strcpy(tempvalue, value);
			strip_double_quote(tempvalue);
			snprintf(sql_str_del_in, len,
					 "DELETE FROM ProcAds_Vertical WHERE scheddname = '%s' and cid = '%s' AND pid = '%s' AND attr = '%s'; INSERT INTO ProcAds_Vertical (scheddname, cid, pid, attr, val) VALUES ('%s', '%s', '%s', '%s', '%s');", scheddname, cid, pid, name, scheddname, cid, pid, name, tempvalue);			
		}
		
		break;
	case 0:
		dprintf(D_ALWAYS, "Set Attribute Processing --- ERROR\n");
		return 0;
		break;
	}
  
	int ret_st = 0;

	if (exec_later == false) {
		ret_st = jqDatabase->execCommand(sql_str_del_in);

		if (ret_st < 0) {
			dprintf(D_ALWAYS, "Set Attribute --- Error [SQL] %s\n", sql_str_del_in);
			displayDBErrorMsg("Set Attribute --- ERROR");      
			free(sql_str_del_in);
			if (tempvalue) free(tempvalue);
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
  
  	free(sql_str_del_in);
	if (tempvalue) free(tempvalue);

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
	char *sql_str;
	char cid[512];
	char pid[512];
	int  id_sort;
	int  ret_st, len;


		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);

	switch(id_sort) {
	case 1:
		len = 4096 + strlen(scheddname) + strlen(cid) + strlen(name);
		sql_str = (char *) malloc(len);
		if(isHorizontalClusterAttribute(name)) {
			snprintf(sql_str , len,
					 "UPDATE ClusterAds_Horizontal SET %s = NULL WHERE scheddname = '%s' and cid = '%s';", name, scheddname, cid);
		} else {
			snprintf(sql_str , len,
					 "DELETE ClusterAds_Vertical WHERE scheddname = '%s' and cid = '%s' AND attr = '%s';", scheddname, cid, name);			
		}

		break;
	case 2:
		len = 4096 + strlen(scheddname) + strlen(cid) + strlen(pid) + strlen(name);
		sql_str = (char *) malloc(len);
		if(isHorizontalProcAttribute(name)) {
			snprintf(sql_str, len,
					 "UPDATE ProcAds_Horizontal SET %s = NULL WHERE scheddname = '%s' and cid = '%s' AND pid = '%s';", name, scheddname, cid, pid);
		} else {
			snprintf(sql_str, len,
					 "DELETE FROM ProcAds_Vertical WHERE scheddname = '%s' and cid = '%s' AND pid = '%s' AND attr = '%s';", scheddname, cid, pid, name);
			
		}

		break;
	case 0:
		dprintf(D_ALWAYS, "Delete Attribute Processing --- ERROR\n");
		return 0;
		break;
	}

	if (sql_str != NULL ) {
		if (exec_later == false) {
			ret_st = jqDatabase->execCommand(sql_str);
		
			if (ret_st < 0) {
				dprintf(D_ALWAYS, "Delete Attribute --- ERROR, [SQL] %s\n", sql_str);
				displayDBErrorMsg("Delete Attribute --- ERROR");
				free(sql_str);
				return 0;
			}
		}
		else {
			if (multi_sql_str != NULL) {
				multi_sql_str = (char*)realloc(multi_sql_str, 
											   strlen(multi_sql_str) + strlen(sql_str) + 1);
				strcat(multi_sql_str, sql_str);
			}
			else {
				multi_sql_str = (char*)malloc(
											  strlen(sql_str) + 1);
				sprintf(multi_sql_str, "%s", sql_str);
			}
		}		
	}

	if (sql_str) free(sql_str);
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
JobQueueDBManager::init()
{
/*
	FILE *fp;
	fp = fopen(pollingInfoLog, "a");
	fclose (fp);
*/
	return 1;
}

//! get Last Job Queue File Polling Information
/*! \param mtime modification time
 *  \param size  file size
 *  \param lcmd  last classad log entry
 */
int
JobQueueDBManager::getJQPollingInfo(long& mtime, long& size, ClassAdLogEntry* lcmd)
{
	char 	*sql_str;
	int	ret_st, len;

	if (lcmd == NULL)
		lcmd = caLogParser->getCurCALogEntry();

	dprintf(D_FULLDEBUG, "Get JobQueue Polling Information\n");

	len = 1024 + strlen(scheddname);
	sql_str = (char *) malloc (len);

	snprintf(sql_str, len, "SELECT last_file_mtime, last_file_size, last_next_cmd_offset, last_cmd_offset, last_cmd_type, last_cmd_key, last_cmd_mytype, last_cmd_targettype, last_cmd_name, last_cmd_value from JobQueuePollingInfo where scheddname = '%s';", scheddname);

		// connect to DB
	ret_st = connectDB();

	if(ret_st <= 0)  {
		free(sql_str);
		return ret_st;
	}

	ret_st = jqDatabase->execQuery(sql_str);
		
	if (ret_st < 0) {
		dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", sql_str);
		displayDBErrorMsg("Update JobQueuePollInfo --- ERROR");
		free(sql_str);
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
	
	if (lcmd->key) free(lcmd->key);
	lcmd->key = strdup(jqDatabase->getValue(0,5)); // last_cmd_key

	if (lcmd->mytype) free(lcmd->mytype);
	lcmd->mytype = strdup(jqDatabase->getValue(0,6)); // last_cmd_mytype

	if (lcmd->targettype) free(lcmd->targettype);
	lcmd->targettype = strdup(jqDatabase->getValue(0,7)); // last_cmd_targettype

	if (lcmd->name) free(lcmd->name);
	lcmd->name = strdup(jqDatabase->getValue(0,8)); // last_cmd_name

	if (lcmd->value) free(lcmd->value);
	lcmd->value = strdup(jqDatabase->getValue(0,9)); // last_cmd_value
	
	jqDatabase->releaseQueryResult(); // release Query Result
										  // since it is no longer needed
	free(sql_str);

		// disconnect to DB
	disconnectDB();
	
#if 0
		/* the following used local file to store polling info. We reverted 
		   back to database for storing polling info so that the polling info
		   is consistent in case of failures
		*/
		   
	fp = fopen(pollingInfoLog, "r");

	if (fp == NULL)
		return 0;

	while (fscanf(fp, " %[^\n]", buf) != EOF) {
		sscanf(buf, "%s =", attName);
		attVal = strstr(buf, "= ");
		attVal += 2;
		
		if (strcmp(attName, "last_file_mtime") == 0) {
			mtime = atoi(attVal); // last_file_mtime
		} else if (strcmp(attName, "last_file_size") == 0) {
			size = atoi(attVal);
		} else if (strcmp(attName, "last_next_cmd_offset") == 0) {
			lcmd->next_offset = atoi(attVal);
		} else if (strcmp(attName, "last_cmd_offset") == 0) {
			lcmd->offset = atoi(attVal);
		} else if (strcmp(attName, "last_cmd_type") == 0) {
			lcmd->op_type = atoi(attVal);
		} else if (strcmp(attName, "last_cmd_key") == 0) {
			lcmd->key = strdup(attVal);
		} else if (strcmp(attName, "last_cmd_mytype") == 0) {
			lcmd->mytype = strdup(attVal);
		} else if (strcmp(attName, "last_cmd_targettype") == 0) {
			lcmd->targettype = strdup(attVal);
		} else if (strcmp(attName, "last_cmd_name") == 0) {
			lcmd->name = strdup(attVal);
		} else if (strcmp(attName, "last_cmd_value") == 0) {
			lcmd->value = strdup(attVal);
		}
	}

	fclose(fp);

#endif

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
 */
int
JobQueueDBManager::setJQPollingInfo(long mtime, long size, ClassAdLogEntry* lcmd)
{
	char *sql_str, *tmp;
	int   ret_st, len;
	
	if (lcmd == NULL)
		lcmd = caLogParser->getCurCALogEntry();	
	
	len = 2048 + strlen(scheddname);
	sql_str = (char *)malloc(len);
	snprintf(sql_str, len,
			"UPDATE JobQueuePollingInfo SET last_file_mtime = %ld, last_file_size = %ld, last_next_cmd_offset = %ld, last_cmd_offset = %ld, last_cmd_type = %d", mtime, size, lcmd->next_offset, lcmd->offset, lcmd->op_type);
	
	addJQPollingInfoSQL(sql_str, "last_cmd_key", lcmd->key);
	addJQPollingInfoSQL(sql_str, "last_cmd_mytype", lcmd->mytype);
	addJQPollingInfoSQL(sql_str, "last_cmd_targettype", lcmd->targettype);
	addJQPollingInfoSQL(sql_str, "last_cmd_name", lcmd->name);
	addJQPollingInfoSQL(sql_str, "last_cmd_value", lcmd->value);
	
	len = 50+strlen(scheddname);
	tmp = (char *) malloc(len);
	snprintf(tmp, len, " WHERE scheddname = '%s';", scheddname);
	strcat(sql_str, tmp);
	
	ret_st = jqDatabase->execCommand(sql_str);
	
	if (ret_st < 0) {
		dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", sql_str);
		displayDBErrorMsg("Update JobQueuePollInfo --- ERROR");
		free(sql_str);
		free(tmp);
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

#if 0

		// code that uses local file to store polling info. They are kept for
		// future reference.

	fp = fopen (pollingInfoLog, "w");
	
	if (fp == NULL)
		return 0;

		// return immediately if any of the following fails
	if  (fprintf(fp, "last_file_mtime = %ld\n", mtime) < 0 ||
		 fprintf(fp, "last_file_size = %ld\n", size) < 0 ||
		 (lcmd->next_offset && fprintf(fp, "last_next_cmd_offset = %ld\n", lcmd->next_offset) < 0) ||
		 (lcmd->offset && fprintf(fp, "last_cmd_offset = %ld\n", lcmd->offset) < 0) ||
		 (lcmd->op_type && fprintf(fp, "last_cmd_type = %d\n", lcmd->op_type) < 0) ||
		 (lcmd->key && fprintf(fp, "last_cmd_key = %s\n", lcmd->key) < 0) ||
		 (lcmd->mytype && fprintf(fp, "last_cmd_mytype = %s\n", lcmd->mytype) < 0) ||
		 (lcmd->targettype && fprintf(fp, "last_cmd_targettype = %s\n", lcmd->targettype) < 0) ||
		 (lcmd->name && fprintf(fp, "last_cmd_name = %s\n", lcmd->name) < 0) ||
		 (lcmd->value && fprintf(fp, "last_cmd_value = %s\n", lcmd->value)))
		return 0;
		
	fclose(fp);

#endif

	free(sql_str);
	free(tmp);
	return 1;
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
