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
#include "../condor_quill/prober.h"
#include "../condor_quill/classadlogparser.h"
#include "database.h"
#include "pgsqldatabase.h"
#include "jobqueuecollection.h"

//! constructor
JobQueueDBManager::JobQueueDBManager()
{
		//nothing here...its all done in config()
}

//! destructor
JobQueueDBManager::~JobQueueDBManager()
{
	if (prober != NULL) {
		delete prober;
	}
	if (caLogParser != NULL) {
		delete caLogParser;
	}
	if (DBObj != NULL) {
			// the destructor will disconnect the database
		delete DBObj;
	}
		// release strings
	if (jobQueueLogFile != NULL) {
		free(jobQueueLogFile);
	}
	if (jobQueueDBIpAddress != NULL) {
		free(jobQueueDBIpAddress);
	}
	if (jobQueueDBName != NULL) {
		free(jobQueueDBName);
	}
	if (jobQueueDBUser != NULL) {
		free(jobQueueDBUser);
	}
	if (jobQueueDBConn != NULL) {
		free(jobQueueDBConn);
	}
	if (multi_sql_str != NULL) {
		free(multi_sql_str);
	}
	if (scheddname != NULL) {
		free(scheddname);
	}
}

//! Gets the writer password required by the quill++
//  daemon to access the database
static char * getWritePassword(char *write_passwd_fname, 
									char *host, char *port, char *db,
									char *dbuser) {
	FILE *fp = NULL;
	char *passwd = (char *) malloc(64 * sizeof(char));
	int len;
	char *prefix;
	MyString *msbuf = 0;
	const char *buf;
	bool found = FALSE;

		// prefix is for the prefix of the entry in the .pgpass
		// it is in the format of the following:
		// host:port:db:user:password
	len = 10+strlen(host) + strlen(port) + strlen(db) + strlen(dbuser);

	prefix = (char  *) malloc (len * sizeof(char));

	snprintf(prefix, len, "%s:%s:%s:%s:", host, port, db, dbuser);

	len = strlen(prefix);

	fp = fopen(write_passwd_fname, "r");

	if(fp == NULL) {
		EXCEPT("Unable to open password file %s\n", write_passwd_fname);
	}
	
		//dprintf(D_ALWAYS, "prefix: %s\n", prefix);

	msbuf = new MyString();
	while(msbuf->readLine(fp, true)) {
		buf = msbuf->Value();

			//dprintf(D_ALWAYS, "line: %s\n", buf);

			// check if the entry matches the prefix
		if (strncmp(buf, prefix, len) == 0) {
				// extract the password
			strncpy(passwd, &buf[len], 64);
			delete msbuf;
			found = TRUE;
			break;
		}

		delete msbuf;
		msbuf = new MyString();
	}

    fclose(fp);
	if (!found) {
		EXCEPT("Unable to find password from file %s\n", write_passwd_fname);
	}

	return passwd;
}
 
void
JobQueueDBManager::config(bool reconfig) 
{
	char *tmp, *host, *port;
	int len, tmp1, tmp2, tmp3;

	if (param_boolean("QUILLPP_ENABLED", false) == false) {
		EXCEPT("Quill++ is currently disabled. Please set QUILLPP_ENABLED to "
			   "TRUE if you want this functionality and read the manual "
			   "about this feature since it requires other attributes to be "
			   "set properly.");
	}

		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the job_queue.log file
	char *spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL variable found in config file\n");
	}
  
	jobQueueLogFile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	snprintf(jobQueueLogFile,_POSIX_PATH_MAX * sizeof(char), 
			 "%s/job_queue.log", spool);

		/*
		  Here we try to read the <ipaddress:port> stored in condor_config
		  if one is not specified, by default we use the local address 
		  and the default postgres port of 5432.  
		*/
	jobQueueDBIpAddress = param("QUILLPP_DB_IP_ADDR");
	if(!jobQueueDBIpAddress) {
		EXCEPT("No QUILLPP_DB_IP_ADDR variable found in config file\n");
	}
	else {
		len = strlen(jobQueueDBIpAddress);
		host = (char *) malloc(len * sizeof(char));
		port = (char *) malloc(len * sizeof(char));

			//split the <ipaddress:port> into its two parts accordingly
		char *ptr_colon = strchr(jobQueueDBIpAddress, ':');
		strncpy(host, jobQueueDBIpAddress, 
				ptr_colon - jobQueueDBIpAddress);
			// terminate the string properly
		host[ptr_colon - jobQueueDBIpAddress] = '\0';
		strncpy(port, ptr_colon+1, len);
			// terminate the string properyly
		port[strlen(ptr_colon+1)] = '\0';
	}

		/* Here we read the database name and if one is not specified
		   use the default name - quill
		   If there are more than one quill daemons are writing to the
		   same databases, its absolutely necessary that the database
		   names be unique or else there would be clashes.  Having 
		   unique database names is the responsibility of the administrator
		*/
	jobQueueDBName = param("QUILLPP_DB_NAME");
	if(!jobQueueDBName) {
		EXCEPT("No QUILLPP_DB_NAME variable found in config file\n");
	}

	jobQueueDBUser = param("QUILLPP_DB_USER");
	if(!jobQueueDBUser) {
		EXCEPT("No QUILLPP_DB_USER variable found in config file\n");
	}

		// get the password from the .pgpass file
	char *writePasswordFile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	snprintf(writePasswordFile, _POSIX_PATH_MAX, "%s/.pgpass", spool);

	char *writePassword = getWritePassword(writePasswordFile, 
										   host, port, jobQueueDBName, 
										   jobQueueDBUser);

	tmp1 = strlen(jobQueueDBName);
	tmp2 = strlen(writePassword);

		//tmp3 is the size of dbconn - its size is estimated to be
		//(2 * len) for the host/port part, tmp1 + tmp2 for the
		//password and dbname part and 1024 as a cautiously
		//overestimated sized buffer
	tmp3 = (2 * len) + tmp1 + tmp2 + 1024;

	jobQueueDBConn = (char *) malloc(tmp3 * sizeof(char));

	snprintf(jobQueueDBConn, tmp3, 
			 "host=%s port=%s dbname=%s user=%s password=%s", 
			 host, port, jobQueueDBName, jobQueueDBUser, writePassword);
  	
	dprintf(D_ALWAYS, "Using Job Queue File %s\n", jobQueueLogFile);
	dprintf(D_ALWAYS, "Using Database IpAddress = %s\n", jobQueueDBIpAddress);
	dprintf(D_ALWAYS, "Using Database Name = %s\n", jobQueueDBName);
	dprintf(D_ALWAYS, "Using Database User = %s\n", jobQueueDBUser);
		//dprintf(D_ALWAYS, "Using Database Name = %s\n", writePassword);
		//dprintf(D_ALWAYS, "Using Database Connection String = \"%s\"\n", jobQueueDBConn);

	if(writePassword) {
		free(writePassword);
		writePassword = NULL;
	}

	if(writePasswordFile) {
		free(writePasswordFile);
		writePasswordFile = NULL;
	}

	if(spool) {
		free(spool);
		spool = NULL;
	}

	if(host) {
		free(host);
		host = NULL;
	}

	if(port) {
		free(port);
		port = NULL;
	}

		// this function is also called when condor_reconfig is issued
		// and so we dont want to recreate all essential objects
	if(!reconfig) {
		prober = new Prober();
		caLogParser = new ClassAdLogParser();

		DBObj = new PGSQLDatabase(jobQueueDBConn);
		
		xactState = NOT_IN_XACT;

		multi_sql_str = NULL;

		QuillErrCode ret_st;

		ret_st = DBObj->connectDB(jobQueueDBConn);
		if (ret_st == FAILURE) {
			displayDBErrorMsg("config: unable to connect to DB--- ERROR");
			EXCEPT("config: unable to connect to DB\n");
		}
		
		tmp = param( "SCHEDD_NAME" );
		if( tmp ) {
			scheddname = build_valid_daemon_name( tmp );
			free(tmp);
		} else {
			scheddname = default_daemon_name();
		}

			/* create an entry in jobqueuepollinginfo if this schedd is the 
			 * first time being logged to database
			 */
		len = 1024 + 2*strlen(scheddname);
		char *sql_str = (char *) malloc (len * sizeof(len));

		snprintf(sql_str, len, "INSERT INTO jobqueuepollinginfo SELECT '%s', 0, 0 WHERE NOT EXISTS (SELECT * FROM jobqueuepollinginfo WHERE scheddname = '%s');", scheddname, scheddname);
		
		ret_st = DBObj->execCommand(sql_str);
		if (ret_st == FAILURE) {
			dprintf(D_ALWAYS, "Insert JobQueuePollInfo --- ERROR [SQL] %s\n", 
					sql_str);
			displayDBErrorMsg("Insert JobQueuePollInfo --- ERROR");		
		}
	
			/* create an entry in currency table if this schedd is the first
			 * time being logged to database 
			 */
		snprintf(sql_str, len, "INSERT INTO currency SELECT '%s', NULL WHERE NOT EXISTS (SELECT * FROM currency WHERE datasource = '%s');", scheddname, scheddname);

		ret_st = DBObj->execCommand(sql_str);
		if (ret_st == FAILURE) {
			dprintf(D_ALWAYS, "Insert Currency --- ERROR [SQL] %s\n", sql_str);
			displayDBErrorMsg("Insert Currency --- ERROR");		
		}
	
		free(sql_str); 		
	}
  
		//this function assumes that certain members have been initialized
		// (specifically prober and caLogParser) and so the order is important.
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
QuillErrCode
JobQueueDBManager::maintain()
{	
	QuillErrCode st, ret_st; 
	ProbeResultType probe_st;	
	struct stat fstat;

	st = getJQPollingInfo(); // get the last polling information

		//if we are unable to get to the polling info file, then either the 
		//postgres server is down or the database is deleted.
	if(st == FAILURE) {
		return FAILURE;
	}

		// check if the job queue log exists, if not just skip polling
	if ((stat(caLogParser->getJobQueueName(), &fstat) == -1)) {
		if (errno == ENOENT) {
			return SUCCESS;
		} else {
				// otherwise there is an error accessing the log
			return FAILURE;
		}
		
	}
	
		// polling
	probe_st = prober->probe(caLogParser->getCurCALogEntry(), 
							 caLogParser->getJobQueueName());
	
		// {init|add}JobQueueDB processes the  Log and stores probing
		// information into DB documentation for how do we determine 
		// the correct state is in the Prober->probe method
	switch(probe_st) {
	case INIT_QUILL:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: INIT\n");
		ret_st = initJobQueueTables();
		break;
	case ADDITION:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: ADDED\n");
		ret_st = addJobQueueTables();
		break;
	case COMPRESSED:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: COMPRESSED\n");
		ret_st = initJobQueueTables();
		break;
	case PROBE_ERROR:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: ERROR\n");
		ret_st = initJobQueueTables();
		break;
	case NO_CHANGE:
		dprintf(D_ALWAYS, "JOB QUEUE POLLING RESULT: NO CHANGE\n");	
		ret_st = SUCCESS;
		break;
	default:
		dprintf(D_ALWAYS, "ERROR HAPPENED DURING JOB QUEUE POLLING\n");
		ret_st = FAILURE;
	}

	return ret_st;
}

/*! delete the job queue related tables
 *  \return the result status
 *			1: Success
 *			0: Fail	(SQL execution fail)
 */	
QuillErrCode
JobQueueDBManager::cleanupJobQueueTables()
{
	int		sqlNum = 4;
	int		i, j, len;
	char   *sql_str[sqlNum];
	
 	len = 128 + strlen(scheddname);

	for (i = 0; i < sqlNum; i++) {
		sql_str[i] = (char *) malloc(len * sizeof(char));
	}

		// we only delete job queue related information.
	snprintf(sql_str[0], len,
			"DELETE FROM clusterads_horizontal WHERE scheddname = '%s';", 
			 scheddname);
	snprintf(sql_str[1], len,
			"DELETE FROM clusterads_vertical WHERE scheddname = '%s';", 
			 scheddname);
	snprintf(sql_str[2], len,
			"DELETE FROM procads_horizontal WHERE scheddname = '%s';", 
			 scheddname);
	snprintf(sql_str[3], len,
			"DELETE FROM procads_vertical WHERE scheddname = '%s';", 
			 scheddname);

	for (i = 0; i < sqlNum; i++) {
		if (DBObj->execCommand(sql_str[i]) == FAILURE) {
			displayDBErrorMsg("Clean UP ALL Data --- ERROR");
			
			for (j = 0; j < sqlNum; j++) {
				free(sql_str[i]);
			}	

			return FAILURE;
		}
	}

 	for (i = 0; i < sqlNum; i++) {
		free(sql_str[i]);
	}

	return SUCCESS;
}

/*! vacuums the job queue related tables 
 */
QuillErrCode
JobQueueDBManager::tuneupJobQueueTables()
{
	int		sqlNum = 5;
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
	snprintf(sql_str[4], 128, 
			"VACUUM ANALYZE Jobqueuepollinginfo;");

	for (i = 0; i < sqlNum; i++) {
		if (DBObj->execCommand(sql_str[i]) == FAILURE) {
			displayDBErrorMsg("VACUUM Database --- ERROR");
			return FAILURE;
		}
	}

	return SUCCESS;
}

/*! build the job queue collection from job_queue.log file
 */
QuillErrCode
JobQueueDBManager::buildJobQueue(JobQueueCollection *jobQueue)
{
	int		op_type;
	FileOpErrCode st;

	st = caLogParser->readLogEntry(op_type);
	if(st == FILE_OPEN_ERROR) {
		return FAILURE;
	}

	while (st == FILE_READ_SUCCESS) {
		if (processLogEntry(op_type, jobQueue) == FAILURE) {
				// process each ClassAd Log Entry
			return FAILURE;
		}
		st = caLogParser->readLogEntry(op_type);
	}

	return SUCCESS;
}

/*! load job ads in a job queue collection into DB
 */
QuillErrCode
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
			if (DBObj->execCommand(sql_str) == FAILURE) {
				displayDBErrorMsg("COPY ClusterAds_Horizontal --- ERROR");
				return FAILURE;
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (DBObj->sendBulkData(ret_str) == FAILURE) {
				return FAILURE;
			}
		}
	  
	}
	
	if (bFirst == false) {
		if (DBObj->sendBulkDataEnd() == FAILURE) {
			return FAILURE;
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
			if (DBObj->execCommand(sql_str) == FAILURE) {
				displayDBErrorMsg("COPY ClusterAds_Vertical --- ERROR");
				return FAILURE; 
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (DBObj->sendBulkData(ret_str) == FAILURE) {
				return FAILURE;
			}	
		}		
	  
	}
	
	if (bFirst == false) {
		if (DBObj->sendBulkDataEnd() == FAILURE) {
			return FAILURE;
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
			if (DBObj->execCommand(sql_str) == FAILURE) {
				displayDBErrorMsg("COPY ProcAds_Horizontal --- ERROR");
				return FAILURE; // return a error code, 0
			}
			
			bFirst = false;
		}
	  
	  
		if (ret_str != NULL) {
			if (DBObj->sendBulkData(ret_str) == FAILURE) {
				return FAILURE;
			}
		}
	  
	}
	
	if (bFirst == false) {
	  
		if (DBObj->sendBulkDataEnd() == FAILURE) {
			return FAILURE;
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
			if (DBObj->execCommand(sql_str) == FAILURE) {
				displayDBErrorMsg("COPY ProcAds_Vertical --- ERROR");
				return FAILURE; // return a error code, 0
			}
	    
			bFirst = false;
		}
	  
		if (ret_str != NULL) {
			if (DBObj->sendBulkData(ret_str) == FAILURE) {
				return FAILURE;
			}
		}
	  
	}
	
	
	if (bFirst == false) {
		if (DBObj->sendBulkDataEnd() == FAILURE) {
			return FAILURE;
		}
	  
		bFirst = true;
	}
		
	return SUCCESS;
}


/*! build an in-memory list of jobs by reading entries in job_queue.log 
 *  file and dump them into the database.  There are various in-memory lists
 *  for live jobs, i.e. jobs not completed, and historical jobs.  All of them
 *  are written to their appropriate tables
*/
QuillErrCode
JobQueueDBManager::buildAndWriteJobQueue()
{
		//this is an array of linked lists, which keep growing, so 
		//although the array is of fixed size=2000, the ever-growing
		//linked lists can accomodate any number of jobs
	JobQueueCollection *jobQueue = new JobQueueCollection(2000);


	dprintf(D_FULLDEBUG, "Bulkloading 1st Phase: Parsing a job_queue.log "
			"file and building job collection!\n");

		//read from the beginning
	caLogParser->setNextOffset(0);

		//construct the in memory job queue and history
	if (buildJobQueue(jobQueue) == FAILURE) {
		return FAILURE;
	}

	dprintf(D_FULLDEBUG, "Bulkloading 2nd Phase: Loading jobs into DBMS!\n");

		// For job queue tables, send COPY string to RDBMS: 
	if (loadJobQueue(jobQueue) == FAILURE) {
		return FAILURE;
	}

		//  END OF SECOND PHASE OF BULKLOADING
	delete jobQueue;
	return SUCCESS;
}


/*! incrementally read and process log entries from file 
 */
QuillErrCode
JobQueueDBManager::readAndWriteLogEntries()
{
	int op_type = 0;
	FileOpErrCode st;

		// turn off sequential scan so that the incremental update always use 
		// index regardless whether the statistics are correct or not.
	if (DBObj->execCommand("set enable_seqscan=false;") == FAILURE) {
		displayDBErrorMsg("Turning off seq scan --- ERROR");
		return FAILURE; 
	}

	st = caLogParser->readLogEntry(op_type);

		// Process ClassAd Log Entry
	while (st == FILE_READ_SUCCESS) {
		if (processLogEntry(op_type, false) == FAILURE) {
			return FAILURE; 
		}
		st = caLogParser->readLogEntry(op_type);
	}

		// turn on sequential scan again
	if (DBObj->execCommand("set enable_seqscan=true;") == FAILURE) {
		displayDBErrorMsg("Turning on seq scan --- ERROR");
		return FAILURE; // return a error code, 0
	}

	return SUCCESS;
}

/*! process only DELTA, i.e. entries of the job queue log which were
 *  added since the last offset read/set by quill
 */
QuillErrCode
JobQueueDBManager::addJobQueueTables()
{
	QuillErrCode st;

		//we dont set transactions explicitly.  they are set by the 
		//schedd via the 'begin transaction' log entry

	caLogParser->setNextOffset();

	st = readAndWriteLogEntries();

		// Store a polling information into DB
	if (st == SUCCESS) {
		setJQPollingInfo();
	} else {
		// a transaction might have been began, need to be rolled back
		// otherwise subsequent SQLs will continue to fail
		DBObj->rollbackTransaction();
		xactState = NOT_IN_XACT;
	}

	return st;
}

/*! purge all job queue rows and process the entire job_queue.log file
 *  also vacuum the job queue tables
 */
QuillErrCode
JobQueueDBManager::initJobQueueTables()
{
	QuillErrCode st;

	st = DBObj->beginTransaction();

	if(st == FAILURE) {
		displayDBErrorMsg("Init Job Queue Tables unable to begin a transaction --- ERROR");
		return FAILURE; 
	}
	
	st = cleanupJobQueueTables(); // delete all job queue tables

	if(st == FAILURE) {
		displayDBErrorMsg("Init Job Queue Table unable to clean up job queue tables --- ERROR");
		return FAILURE; 
	}

	st = buildAndWriteJobQueue(); // bulk load job queue log

		// Store polling information in database
	if (st == SUCCESS) {
		setJQPollingInfo();

			// VACUUM should be called outside XACT
			// So, Commit XACT shouble be invoked beforehand.
		DBObj->commitTransaction(); // end XACT
		xactState = NOT_IN_XACT;
	} else {
		DBObj->rollbackTransaction();
		xactState = NOT_IN_XACT;
	}

	return st;	
}


/*! handle a log Entry: work with a job queue collection.
 *  (not with DBMS directry)
 */
QuillErrCode
JobQueueDBManager::processLogEntry(int op_type, JobQueueCollection* jobQueue)
{
	char *key, *mytype, *targettype, *name, *value;
	key = mytype = targettype = name = value = NULL;
	QuillErrCode st = SUCCESS;

	int job_id_type;
	char cid[512];
	char pid[512];

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here,
		// and they are at the end of the routine
	switch(op_type) {
	case CondorLogOp_NewClassAd: {
		if (caLogParser->getNewClassAdBody(key, mytype, targettype) == FAILURE)
			{
				st = FAILURE;
				break;
			}
		job_id_type = getProcClusterIds(key, cid, pid);
		ClassAd* ad = new ClassAd();
		ad->SetMyTypeName("Job");
		ad->SetTargetTypeName("Machine");

		switch(job_id_type) {
		case IS_CLUSTER_ID:
			jobQueue->insertClusterAd(cid, ad);
			break;

		case IS_PROC_ID:
			jobQueue->insertProcAd(cid, pid, ad);
			break;

		default:
			dprintf(D_ALWAYS, "[QUILL++] New ClassAd --- ERROR\n");
			st = FAILURE; 
			break;
		}

		break;
	}
	case CondorLogOp_DestroyClassAd: {
		if (caLogParser->getDestroyClassAdBody(key) == FAILURE) {
			st = FAILURE; 
			break;		
		}

		job_id_type = getProcClusterIds(key, cid, pid);

		switch(job_id_type) {
		case IS_CLUSTER_ID:
			jobQueue->removeClusterAd(cid);
			break;

		case IS_PROC_ID: {
			ClassAd *clusterad = jobQueue->find(cid);
			ClassAd *procad = jobQueue->find(cid,pid);
			if(!clusterad || !procad) {
			    dprintf(D_ALWAYS, 
						"[QUILL++] Destroy ClassAd --- Cannot find clusterad "
						"or procad in memory job queue");
				st = FAILURE; 
				break;
			}

			jobQueue->removeProcAd(cid, pid);
			
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL++] Destroy ClassAd --- ERROR\n");
			st = FAILURE; 
			break;
		}

		break;
	}
	case CondorLogOp_SetAttribute: {	
		if (caLogParser->getSetAttributeBody(key, name, value) == FAILURE) {
			st = FAILURE; 
			break;
		}
						
		job_id_type = getProcClusterIds(key, cid, pid);

		char tmp[512];

		snprintf(tmp, 512, "%s = %s", name, value);

		switch (job_id_type) {
		case IS_CLUSTER_ID: {
			ClassAd* ad = jobQueue->findClusterAd(cid);
			if (ad != NULL) {
				ad->Insert(tmp);
			}
			else {
				dprintf(D_ALWAYS, "[QUILL++] ERROR: There is no such Cluster Ad[%s]\n", cid);
			}
			break;
		}
		case IS_PROC_ID: {
			ClassAd* ad = jobQueue->findProcAd(cid, pid);
			if (ad != NULL) {
				ad->Insert(tmp);
			}
			else {
				dprintf(D_ALWAYS, "[QUILL++] ERROR: There is no such Proc Ad[%s.%s]\n", cid, pid);
			}
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL++] Set Attribute --- ERROR\n");
			st = FAILURE; 
			break;
		}
		break;
	}
	case CondorLogOp_DeleteAttribute: {
		if (caLogParser->getDeleteAttributeBody(key, name) == FAILURE) {
			st = FAILURE; 
			break;
		}

		job_id_type = getProcClusterIds(key, cid, pid);

		switch(job_id_type) {
		case IS_CLUSTER_ID: {
			ClassAd* ad = jobQueue->findClusterAd(cid);
			ad->Delete(name);
			break;
		}
		case IS_PROC_ID: {
			ClassAd* ad = jobQueue->findProcAd(cid, pid);
			ad->Delete(name);
			break;
		}
		default:
			dprintf(D_ALWAYS, "[QUILL++] Delete Attribute --- ERROR\n");
			st = FAILURE; 
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
		st = FAILURE; 
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

/*! handle ClassAd Log Entry
 *
 * is a wrapper over all the processXXX functions
 * in this and all the processXXX routines, if exec_later == true, 
 * a SQL string is returned instead of actually sending it to the DB.
 * However, we always have exec_later = false, which means it actually
 * writes to the database in an eager fashion
 */
QuillErrCode
JobQueueDBManager::processLogEntry(int op_type, bool exec_later)
{
	char *key, *mytype, *targettype, *name, *value;
	key = mytype = targettype = name = value = NULL;
	QuillErrCode	st = SUCCESS;

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here,
		//  and they are at the end of the routine
	switch(op_type) {
	case CondorLogOp_NewClassAd:
		if (caLogParser->getNewClassAdBody(key, mytype, targettype) == FAILURE)
			{
				return FAILURE; 
			}

		st = processNewClassAd(key, mytype, targettype, exec_later);
			
		break;
	case CondorLogOp_DestroyClassAd:
		if (caLogParser->getDestroyClassAdBody(key) == FAILURE)
			return FAILURE;
			
		st = processDestroyClassAd(key, exec_later);
			
		break;
	case CondorLogOp_SetAttribute:
		if (caLogParser->getSetAttributeBody(key, name, value) == FAILURE) {
			return FAILURE;
		}

		st = processSetAttribute(key, name, value, exec_later);
			
		break;
	case CondorLogOp_DeleteAttribute:
		if (caLogParser->getDeleteAttributeBody(key, name) == FAILURE) {
			return FAILURE;
		}

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
		return FAILURE;
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
	dprintf(D_ALWAYS, "\t%s\n", DBObj->getDBError());
}

/*! separate a key into Cluster Id and Proc Id 
 *  \return key type 
 *			1: when it is a cluster id
 *			2: when it is a proc id
 * 			0: it fails
 *
 *	\warning The memories of cid and pid should be allocated in advance.
 */
JobIdType
JobQueueDBManager::getProcClusterIds(const char* key, char* cid, char* pid)
{
	int key_len, i;
	long iCid;
	char*	pid_in_key;

	if (key == NULL) {
		return IS_UNKNOWN_ID;
	}

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
	if (i == key_len) {
		return IS_UNKNOWN_ID; // Error
	}

		// These two lines are for removing a leading zero.
	iCid = atol(cid);
	sprintf(cid,"%ld", iCid);


	pid_in_key = (char*)(key + (i + 1));
	strcpy(pid, pid_in_key);

	if (atol(pid) == -1) {// Cluster ID
		return IS_CLUSTER_ID;	
	}

	return IS_PROC_ID; // Proc ID
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
QuillErrCode
JobQueueDBManager::processNewClassAd(char* key, 
									 char* mytype, 
									 char* ttype, 
									 bool exec_later)
{
	char *sql_str;
	char  cid[512];
	char  pid[512];
	int   job_id_type, len;

		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);

	switch(job_id_type) {
	case IS_CLUSTER_ID:
		len = 1024 + strlen(scheddname) + strlen(cid);
		sql_str = (char *)malloc(len * sizeof(char));
		snprintf(sql_str, len,
				"INSERT INTO ClusterAds_Horizontal (scheddname, cluster) VALUES ('%s', '%s');", scheddname, cid);

		break;
	case IS_PROC_ID:
		len = 1024 + strlen(scheddname) + strlen(cid) + strlen(pid);
		sql_str = (char *)malloc(len * sizeof(char));
		snprintf(sql_str, len,
				"INSERT INTO ProcAds_Horizontal (scheddname, cluster, proc) VALUES ('%s', '%s', '%s');", scheddname, cid, pid);

		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "New ClassAd Processing --- ERROR\n");
		return FAILURE; // return a error code, 0
		break;
	}


	if (exec_later == false) { // execute them now
		if (DBObj->execCommand(sql_str) == FAILURE) {
			displayDBErrorMsg("New ClassAd Processing --- ERROR");
			free(sql_str);
			return FAILURE;
		}
	}
	else {
		if (multi_sql_str != NULL) { // append them to a SQL buffer
			len = strlen(sql_str) + 1;
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + len);
			strncat(multi_sql_str, sql_str, len);
		}
		else {
			len = strlen(sql_str) + 1;
			multi_sql_str = (char*)malloc(len * sizeof(char));
			snprintf(multi_sql_str, len, "%s", sql_str);
		}
	}		

	free(sql_str);
	return SUCCESS;
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
QuillErrCode
JobQueueDBManager::processDestroyClassAd(char* key, bool exec_later)
{
	char *sql_str1; 
	char *sql_str2; 
	char cid[100];
	char pid[100];
	int  job_id_type, len;

  
		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);
  
	switch(job_id_type) {
	case IS_CLUSTER_ID:	// ClusterAds
		len = 2048+ strlen(scheddname) + strlen(cid);
		sql_str1 = (char *) malloc(len * sizeof(char));
		snprintf(sql_str1, len,
				"DELETE FROM ClusterAds_Horizontal WHERE scheddname = '%s' and cluster = %s;", scheddname, cid);
    
		sql_str2 = (char *) malloc(len * sizeof(char));
		snprintf(sql_str2, len,
				"DELETE FROM ClusterAds_Vertical WHERE scheddname = '%s' and cluster = %s;", scheddname, cid);
		break;
        case IS_PROC_ID:
			/* generate SQL to remove the job from job tables */
		len = 2048 + strlen(scheddname) + strlen(cid) + strlen(pid);	
		sql_str1 = (char *) malloc(len * sizeof(char));
		snprintf(sql_str1, len,
				"DELETE FROM ProcAds_horizontal WHERE scheddname = '%s' and cid = %s AND pid = %s;", 
				 scheddname, cid, pid);
    
		sql_str2 = (char *) malloc(len * sizeof(char));
		snprintf(sql_str2, len,
				"DELETE FROM ProcAds_vertical WHERE scheddname = '%s' and cid = %s AND pid = %s;", 
				 scheddname, cid, pid);
		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "[QUILL++] Destroy ClassAd --- ERROR\n");
		return FAILURE; // return a error code, 0
		break;
	}
  
	if (exec_later == false) {	
		if (DBObj->execCommand(sql_str1) == FAILURE) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			free(sql_str1);
			free(sql_str2);
			return FAILURE; // return a error code, 0
		}

		if (DBObj->execCommand(sql_str2) == FAILURE) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			free(sql_str1);
			free(sql_str2);
			return FAILURE; // return a error code, 0
		}
	}
	else {
		if (multi_sql_str != NULL) {
			len = strlen(sql_str1) + strlen(sql_str2) + 1;
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + len);
			strncat(multi_sql_str, sql_str1, len);
			strncat(multi_sql_str, sql_str2, (len - strlen(sql_str1)));
		}
		else {
			len = strlen(sql_str1) + strlen(sql_str2) + 1;
			multi_sql_str = (char*)malloc(len * sizeof(char));
			snprintf(multi_sql_str, len, "%s%s", sql_str1, sql_str2);
		}
	}
  
	free(sql_str1);
	free(sql_str2);
	return SUCCESS;
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
QuillErrCode
JobQueueDBManager::processSetAttribute(char* key, 
									   char* name, 
									   char* value, 
									   bool exec_later)
{
	char *sql_str_del_in;

	char cid[512];
	char pid[512];
	int  job_id_type, len;
	char *tempvalue = NULL;
		//int		ret_st;
	char *newvalue = NULL;

		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);
  
	switch(job_id_type) {
	case IS_CLUSTER_ID:
		len = 2048 + 2*(strlen(scheddname) + strlen(cid) + strlen(name)) + strlen(value);
		sql_str_del_in = (char *) malloc(len * sizeof(char));
		if(isHorizontalClusterAttribute(name)) {
			if (strcasecmp(name, "qdate") == 0) {
				snprintf(sql_str_del_in, len,
						 "UPDATE ClusterAds_Horizontal SET %s = (('epoch'::timestamp + '%s seconds') at time zone 'UTC') WHERE scheddname = '%s' and cluster = '%s';", name, value, scheddname, cid);
			} else {
				tempvalue = (char *) malloc(strlen(value) + 1);
				strcpy(tempvalue, value);
				strip_double_quote(tempvalue);
				newvalue = fillEscapeCharacters(tempvalue);
					// escape single quote within the value
				snprintf(sql_str_del_in, len,
						 "UPDATE ClusterAds_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cluster = '%s';", name, newvalue, scheddname, cid);
				free(newvalue);

			}
		} else {
			tempvalue = (char *) malloc(strlen(value) + 1);
			strcpy(tempvalue, value);
			strip_double_quote(tempvalue);
			newvalue = fillEscapeCharacters(tempvalue);
			snprintf(sql_str_del_in, len,
					 "DELETE FROM ClusterAds_Vertical WHERE scheddname = '%s' and cluster = '%s' AND attr = '%s'; INSERT INTO ClusterAds_Vertical (scheddname, cluster, attr, val) VALUES ('%s', '%s', '%s', '%s');", scheddname, cid, name, scheddname, cid, name, newvalue);
			free(newvalue);
		}

		break;
	case IS_PROC_ID:
		len = 2048 + 2*(strlen(scheddname) + strlen(cid) + strlen(pid) + strlen(name)) + strlen(value);
		sql_str_del_in = (char *) malloc(len * sizeof(char));
		tempvalue = (char *) malloc(strlen(value) + 1);

		if(isHorizontalProcAttribute(name)) {
			strcpy(tempvalue, value);
			strip_double_quote(tempvalue);
			snprintf(sql_str_del_in, len,
					 "UPDATE ProcAds_Horizontal SET %s = '%s' WHERE scheddname = '%s' and cluster = '%s' and proc = '%s';", name, tempvalue, scheddname, cid, pid);
		} else {
			strcpy(tempvalue, value);
			strip_double_quote(tempvalue);
			newvalue = fillEscapeCharacters(tempvalue);
			snprintf(sql_str_del_in, len,
					 "DELETE FROM ProcAds_Vertical WHERE scheddname = '%s' and cluster = '%s' AND proc = '%s' AND attr = '%s'; INSERT INTO ProcAds_Vertical (scheddname, cluster, proc, attr, val) VALUES ('%s', '%s', '%s', '%s', '%s');", scheddname, cid, pid, name, scheddname, cid, pid, name, newvalue);			
			free(newvalue);
		}
		
		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "Set Attribute Processing --- ERROR\n");
		return FAILURE;
		break;
	}
  
	QuillErrCode ret_st;

	if (exec_later == false) {
		ret_st = DBObj->execCommand(sql_str_del_in);

		if (ret_st == FAILURE) {
			dprintf(D_ALWAYS, "Set Attribute --- Error [SQL] %s\n", 
					sql_str_del_in);
			displayDBErrorMsg("Set Attribute --- ERROR");      
			if (sql_str_del_in) {
				free(sql_str_del_in);
			}
			if (tempvalue) {
				free(tempvalue);
			}
			return FAILURE;
		}
	}
	else {
		if (multi_sql_str != NULL) {
				// NOTE:
				// this case is not trivial 
				// because there could be multiple insert
				// statements.
			len = strlen(sql_str_del_in) + 1;
			multi_sql_str = (char*)realloc(multi_sql_str, 
										   strlen(multi_sql_str) + len);
			strncat(multi_sql_str, sql_str_del_in, len);
		}
		else {
			len = strlen(sql_str_del_in) + 1;
			multi_sql_str = (char*)malloc(len * sizeof(char));
			strncpy(multi_sql_str, sql_str_del_in, len);
		}    
	}
  
	if(sql_str_del_in) {	
		free(sql_str_del_in);
	}
	if (tempvalue) {
		free(tempvalue);
	}

	return SUCCESS;
}


/*! process DeleteAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 */
QuillErrCode
JobQueueDBManager::processDeleteAttribute(char* key, 
										  char* name, 
										  bool exec_later)
{	
	char *sql_str;
	char cid[512];
	char pid[512];
	int  job_id_type;
	QuillErrCode  ret_st;
	int len;


		// It could be ProcAd or ClusterAd
		// So need to check
	job_id_type = getProcClusterIds(key, cid, pid);

	switch(job_id_type) {
	case IS_CLUSTER_ID:
		len = 4096 + strlen(scheddname) + strlen(cid) + strlen(name);
		sql_str = (char *) malloc(len);
		if(isHorizontalClusterAttribute(name)) {
			snprintf(sql_str , len,
					 "UPDATE ClusterAds_Horizontal SET %s = NULL WHERE scheddname = '%s' and cluster = '%s';", name, scheddname, cid);
		} else {
			snprintf(sql_str , len,
					 "DELETE ClusterAds_Vertical WHERE scheddname = '%s' and cluster = '%s' AND attr = '%s';", scheddname, cid, name);			
		}

		break;
	case IS_PROC_ID:
		len = 4096 + strlen(scheddname) + strlen(cid) + strlen(pid) + strlen(name);
		sql_str = (char *) malloc(len);
		if(isHorizontalProcAttribute(name)) {
			snprintf(sql_str, len,
					 "UPDATE ProcAds_Horizontal SET %s = NULL WHERE scheddname = '%s' and cluster = '%s' AND proc = '%s';", name, scheddname, cid, pid);
		} else {
			snprintf(sql_str, len,
					 "DELETE FROM ProcAds_Vertical WHERE scheddname = '%s' and cluster = '%s' AND proc = '%s' AND attr = '%s';", scheddname, cid, pid, name);
			
		}

		break;
	case IS_UNKNOWN_ID:
		dprintf(D_ALWAYS, "Delete Attribute Processing --- ERROR\n");
		return FAILURE;
		break;
	}

	if (sql_str != NULL ) {
		if (exec_later == false) {
			ret_st = DBObj->execCommand(sql_str);
		
			if (ret_st == FAILURE) {
				dprintf(D_ALWAYS, "Delete Attribute --- ERROR, [SQL] %s\n",
						sql_str);
				displayDBErrorMsg("Delete Attribute --- ERROR");
				if (sql_str) {
					free(sql_str);
				}
				return FAILURE;
			}
		}
		else {
			if (multi_sql_str != NULL) {
				len = strlen(sql_str) + 1;
				multi_sql_str = (char*)realloc(multi_sql_str, 
											   strlen(multi_sql_str) + len);
				strncat(multi_sql_str, sql_str, len);
			}
			else {
				len = strlen(sql_str) + 1;
				multi_sql_str = (char*)malloc(len * sizeof(char));
				snprintf(multi_sql_str, len, "%s", sql_str);
			}
		}		
	}

	if (sql_str) {
		free(sql_str);
	}
	return SUCCESS;
}

/*! process BeginTransaction command
 *  \return the result status
 */
QuillErrCode
JobQueueDBManager::processBeginTransaction(bool exec_later)
{
	xactState = BEGIN_XACT;
	if(!exec_later) {
		if (DBObj->beginTransaction() == FAILURE) 
			return FAILURE;			   				
	}
	return SUCCESS;
}

/*! process EndTransaction command
 *  \return the result status
 */
QuillErrCode
JobQueueDBManager::processEndTransaction(bool exec_later)
{
	xactState = COMMIT_XACT;
	if(!exec_later) {
		if (DBObj->commitTransaction() == FAILURE) 
			return FAILURE;			   			
	}
	return SUCCESS;
}

//! initialize: currently check the DB schema
/*! \param initJQDB initialize DB?
 */
QuillErrCode
JobQueueDBManager::init()
{
	return SUCCESS;
}

//! get Last Job Queue File Polling Information
QuillErrCode
JobQueueDBManager::getJQPollingInfo()
{
	long mtime;
	long size;
	ClassAdLogEntry* lcmd;
	char 	*sql_str;
	int	ret_st, len, num_result=0;

	lcmd = caLogParser->getCurCALogEntry();

	dprintf(D_FULLDEBUG, "Get JobQueue Polling Information\n");

	len = 1024 + strlen(scheddname);
	sql_str = (char *) malloc (len);

	snprintf(sql_str, len, "SELECT last_file_mtime, last_file_size, last_next_cmd_offset, last_cmd_offset, last_cmd_type, last_cmd_key, last_cmd_mytype, last_cmd_targettype, last_cmd_name, last_cmd_value from JobQueuePollingInfo where scheddname = '%s';", scheddname);

	ret_st = DBObj->execQuery(sql_str, num_result);
		
	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "Reading JobQueuePollInfo --- ERROR [SQL] %s\n", 
				sql_str);
		displayDBErrorMsg("Reading JobQueuePollInfo --- ERROR");
		free(sql_str);
		return FAILURE;
	}
	else if (ret_st == SUCCESS && num_result == 0) {
			// This case is a rare one since the jobqueuepollinginfo
			// table contains one tuple at all times 		
		displayDBErrorMsg("Reading JobQueuePollingInfo --- ERROR "
						  "No Rows Retrieved from JobQueuePollingInfo\n");
		free(sql_str);
		return FAILURE;
	} 

	mtime = atoi(DBObj->getValue(0,0)); // last_file_mtime
	size = atoi(DBObj->getValue(0,1)); // last_file_size

	prober->setJQFile_Last_MTime(mtime);
	prober->setJQFile_Last_Size(size);

		// last_next_cmd_offset
	lcmd->next_offset = atoi(DBObj->getValue(0,2)); 
	lcmd->offset = atoi(DBObj->getValue(0,3)); // last_cmd_offset
	lcmd->op_type = atoi(DBObj->getValue(0,4)); // last_cmd_type
	
	if (lcmd->key) {
		free(lcmd->key);
	}

	if (lcmd->mytype) {
		free(lcmd->mytype);
	}

	if (lcmd->targettype) {
		free(lcmd->targettype);
	}

	if (lcmd->name) {
		free(lcmd->name);
	}

	if (lcmd->value) {
		free(lcmd->value);
	}

	lcmd->key = strdup(DBObj->getValue(0,5)); // last_cmd_key
	lcmd->mytype = strdup(DBObj->getValue(0,6)); // last_cmd_mytype
		// last_cmd_targettype
	lcmd->targettype = strdup(DBObj->getValue(0,7)); 
	lcmd->name = strdup(DBObj->getValue(0,8)); // last_cmd_name
	lcmd->value = strdup(DBObj->getValue(0,9)); // last_cmd_value
	
	DBObj->releaseQueryResult(); // release Query Result
										  // since it is no longer needed
	free(sql_str);

	return SUCCESS;	
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
QuillErrCode
JobQueueDBManager::setJQPollingInfo()
{
	long mtime;
	long size;	
	ClassAdLogEntry* lcmd;
	char *sql_str, *tmp;
	int   len;
	QuillErrCode   ret_st;
	int            num_result=0, db_err_code=0;

	prober->incrementProbeInfo();
	mtime = prober->getJQFile_Last_MTime();
	size = prober->getJQFile_Last_Size();
	lcmd = caLogParser->getCurCALogEntry();	
	
	len = MAX_FIXED_SQL_STR_LENGTH + strlen(scheddname) + sizeof(lcmd->value);
	sql_str = (char *)malloc(len * sizeof(char));
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
	
	ret_st = DBObj->execCommand(sql_str, num_result, db_err_code);
	
	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", 
				sql_str);
		displayDBErrorMsg("Update JobQueuePollInfo --- ERROR");
	}
	else if (ret_st == SUCCESS && num_result == 0) {
			// This case is a rare one since the jobqueuepollinginfo
			// table contains one tuple at all times 

		dprintf(D_ALWAYS, "Update JobQueuePollInfo --- ERROR [SQL] %s\n", 
				sql_str);
		displayDBErrorMsg("Update JobQueuePollInfo --- ERROR");
		ret_st = FAILURE;
	} 	
	
	if (sql_str) {
		free(sql_str);
	}
	
	if (tmp) {
		free(tmp);
	}
	return ret_st;
}

char * fillEscapeCharacters(char * str) {
	int i, j;
	
	int len = strlen(str);

		//here we allocate 1024 more than the size of the 
		//old string assuming that there wouldn't be more than 1024
		//quotes in there 
	char *newstr = (char *) malloc((len + 1024) * sizeof(char));
	
	j = 0;
	for (i = 0; i < len; i++) {
		switch(str[i]) {
        case '\'':
            newstr[j] = '\\';
            newstr[j+1] = '\'';
            j += 2;
            break;
        case '\t':
            newstr[j] = '\\';
            newstr[j+1] = 't';
            j += 2;
            break;
        default:
            newstr[j] = str[i];
            j++;
            break;
		}
	}
	newstr[j] = '\0';
    return newstr;
}

