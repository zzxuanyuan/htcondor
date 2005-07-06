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
#ifndef _JOBQUEUEDBMANAGER_H_
#define _JOBQUEUEDBMANAGER_H_

//#define _REMOTE_DB_CONNECTION_ 

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"

//for the collectorlist::create call
#include "dc_collector.h" 

#include "classadlogentry.h"
#include "jobqueuecollection.h"

class Prober;
class ClassAdLogParser;
class JobQueueDatabase;

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

//! JobQueueDBManager
/*! \brief this class orchestrates all of Quill.  
 */
class JobQueueDBManager : public Service
{
 public:
		//! constructor	
	JobQueueDBManager();
	
		//! destructor	
	~JobQueueDBManager();
	
		//! read all config options from condor_config file. 
		//! Also create class members instead of doing it in the constructor
	void    config(bool reconfig = false);
	
		//! initialize: currently check the DB schema
	int		init(bool initJQDB);
	
		//! register all timer and command handlers
	void	registerAll();
		//! register all command handlers
	void	registerCommands();
		//! register all timer handlers
	void	registerTimers();
	
		//! maintain the database
	int  	maintain();
	
		//! get Last Job Queue File Polling Information 
	int		getJQPollingInfo(long& mtime, long& size, ClassAdLogEntry* lcmd = NULL); 	
		//! set Current Job Queue File Polling Information 
	int		setJQPollingInfo(long mtime, long size, ClassAdLogEntry* lcmd = NULL); 	
		//	
		// accessors
		//
	void	            setJobQueueFileName(const char* jqName);
	const char*         getJobQueueDBConn() { return jobQueueDBConn; }
	ClassAdLogParser*	getClassAdLogParser();
	
	
		//
		// handlers
		//

		//! timer handler for maintaining job queue and sending SCHEDD_AD to collector
	void		pollingTime();	

		//! command handler QMGMT_CMD command; services condor_q queries
		//! posed to quill.  This is deprecated.  Instead the condor_q tool
		//! now directly talks to the database
	int 		handle_q(int, Stream*);
	
 private:
	CollectorList 	*collectors;
	ClassAd 	    *ad;
	
		//! create the SCHEDD_AD that's sent to the collector
	void 	 createClassAd(void);

		//! escape quoted strings since postgres doesn't like 
		//! unescaped single quotes
	char *   fillEscapeCharacters(char *str);
	
		//
		// helper functions
		// ----
		// All DB access methods here do not call Xact and connection related 
		// function within them.
		// So you must call connectDB() and disconnectDB() before and after 
		// calling them.
		//
	
		//! purge all job queue rows and process the entire job_queue.log file
	int 	initJobQueueTables();
		//! process only the DELTA
	int 	addJobQueueTables();
	

		/*! 
		  The following routines are related to the bulk-loading phase
		  of quill where it processes and keeps the entire job queue in 
		  memory and writes it all at once.  We do this only when 
		  a) quill first wakes up, or b) if the log is compressed, or 
		  c) there is an error and we just have to read from scratch
		  It is not called in the frequent case of incremental changes
		  to the log
		*/

		//! build the job queue collection by reading entries in 
		//! job_queue.log file and load them into RDBMS		
	int 	buildAndWriteJobQueue();

		//! does the actual building of the job queue
	int		buildJobQueue(JobQueueCollection *jobQueue);
	
		//! dumps the in-memory job queue to the database
	int		loadJobQueue(JobQueueCollection *jobQueue);
	
		//! the routine that processes each entry (insert/delete/etc.)
	int		processLogEntry(int op_type, JobQueueCollection *jobQueue);


		/* 
		   The following routines are related to handling the common case 
		   of processing incremental changes to the job_queue.log file
		   Unlike buildAndWriteJobQueue, this does not maintain 
		   an in-memory table of jobs and as so it does not consume 
		   as much memory at the expense of taking slightly longer
		   There's a processXXX for each kind of log entry
		*/

		//! incrementally read and write log entries to database
		//! calls processLogEntry on each new log entry in the job_queue.log file
	
	int 	readAndWriteLogEntries();

		//! is a wrapper over all the processXXX functions
		//! in this and all the processXXX routines, if exec_later == true, 
		//! a SQL string is returned instead of actually sending it to the DB.
		//! However, we always have exec_later = false, which means it actually
		//! writes to the database in an eager fashion
	int		processLogEntry(int op_type, bool exec_later);
	int		processNewClassAd(char* key, 
							  char* mytype, 
							  char* ttype, 
							  bool exec_later = false);
		//! in addition to deleting the classad, this routine is also 
		//! responsible for maintaining the history tables.  Thanks to
		//! this catch, we can get history for free, i.e. without having
		//! to sniff the history file
	int		processDestroyClassAd(char* key, bool exec_later = false);
	int		processSetAttribute(char* key, 
								char* name, 
								char* value, 
								bool exec_later = false);
	int		processDeleteAttribute(char* key, char* name, bool exec_later = false);
	int		processBeginTransaction(bool exec_later = false);
	int		processEndTransaction(bool exec_later = false);


		//! deletes all rows from all job queue related tables
	int		cleanupJobQueueTables();
	
		//! runs the postgres garbage collection and statistics 
		//! collection routines on the job queue related tables
	int		tuneupJobQueueTables();

		//! deletes all history rows that are older than a certain 
		//! user specified period (specified by QUILL_HISTORY_DURATION 
		//! condor_config file)
	int		purgeOldHistoryRows();

		//! split key into cid and pid
	int		getProcClusterIds(const char* key, char* cid, char* pid);

		//! utility routine to show database error mesage
	void	displayDBErrorMsg(const char* errmsg);

		//! connect and disconnect to the postgres database
	int 	connectDB(int Xact = BEGIN_XACT);
	int		disconnectDB(int commit = COMMIT_XACT);

		//! checks the database and its schema 
		//! if necessary creates the database and all requisite tables/views
	int		checkSchema();

		//! concatenates src_name=val to dest SQL string
	void	addJQPollingInfoSQL(char* dest, char* src_name, char* src_val);
	
		//
		// members
		//
	Prober*	            prober;			//!< Prober
	ClassAdLogParser*	caLogParser;	//!< ClassAd Log Parser
	JobQueueDatabase*	jqDatabase;		//!< Job Queue Database

	enum    XACT_STATE {NOT_IN_XACT, 
						BEGIN_XACT, 
						COMMIT_XACT, 
						ABORT_XACT}; 
	XACT_STATE	xactState;		    //!< current XACT state


	char*	jobQueueLogFile; 		//!< Job Queue Log File Path
	char*	jobQueueDBConn;  		//!< DB connection string
	char*	jobQueueDBIpAddress;    //!< <IP:PORT> address of DB
	char*	jobQueueDBName;         //!< DB Name

	int		purgeHistoryTimeId;		//!< timer handler id of purgeOldHistoryRows
	int		purgeHistoryDuration;	//!< number of days to keep history around
	int     historyCleaningInterval;//!< number of hours between two successive
	                                //!< successive calls to purgeOldHistoryRows

	int		pollingTimeId;			//!< timer handler id of pollingTime function
	int		pollingPeriod;			//!< polling time period in seconds

	char*	multi_sql_str;			//!< buffer for SQL

	int		numTimesPolled;			//!< used to vacuum and analyze job queue tables
};

#endif /* _JOBQUEUEDBMANAGER_H_ */
