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
#ifndef _QUEUEDBMANAGER_H_
#define _QUEUEDBMANAGER_H_

#include "condor_common.h"
#include "classad_collection.h"
#include "odbc.h"
//#include "../condor_daemon_core.V6/condor_daemon_core.h"
//#include "daemon.h"

//ra #include "classadlogentry.h"
//ra #include "jobqueuecollection.h"

//ra class Prober;
//ra class ClassAdLogParser;
//class JobQueueDatabase;
class StaticHashTable;

//! JobQueueDBManager
/*! \brief this class orchestrates the whole jqmond
 */
class QueueDBManager //ra : public Service
{
public:
	//! constructor	
	QueueDBManager();
	//! destructor	
	~QueueDBManager();
	
	//! initialize: currently check the DB schema
	int		init(bool initJQDB);
	bool            isInitialized() {return initialized;}
	int 		connectDB(int Xact = BEGIN_XACT);
	int		disconnectDB(int commit = COMMIT_XACT);
	int             commitTransaction();

	int             processHistoryAd(ClassAd *ad);
	int		processClassAdLogEntry(int op_type, bool exec_later);
	int	      	processNewClassAd(const char* key, const char* mytype, const char* ttype, bool exec_later = false);
	int	       	processDestroyClassAd(const char* key, bool exec_later = false);
	int	       	processSetAttribute(const char* key, const char* name, const char* value, bool exec_later = false);
	int	       	processDeleteAttribute(const char* key, const char* name, bool exec_later = false);
	int	       	processBeginTransaction();
	int	       	processEndTransaction();	//! register all timer and command handlers
	//ra void	registerAll();
	//! register all command handlers
	//ra void	registerCommands();
	//! register all timer handlers
	//ra void	registerTimers();

	//! maintain a job queue database
	//ra int  	maintain();

	//! get Last Job Queue File Polling Information 
	//ra int		getJQPollingInfo(long& mtime, long& size, ClassAdLogEntry* lcmd = NULL); 	
	//! set Current Job Queue File Polling Information 
	//ra int		setJQPollingInfo(long mtime, long size, ClassAdLogEntry* lcmd = NULL); 	

		//	
		// accessors
		//
	//ra void		        setJobQueueFileName(const char* jqName);
	const char* 		getJobQueueDBConn() { return jobQueueDBConn; }
	//ra ClassAdLogParser*	getClassAdLogParser();


		//
		// handlers
		//
	//! timer handler for each polling event
	//ra void		pollingTime();	
	//! command handler for QMGMT_CMD command from condor_q
	//ra int 		handle_q(int, Stream*);

private:
		//
		// helper functions
		// ----
		// All DB access methods here do not call Xact and connection related 
		// function within them.
		// So you must call connectDB() and disconnectDB() before and after 
		// calling them.
		//

	//! delete the current DB and process the whole job_queue.log file
	//ra int 		initJobQueueDB();
	//! process only DELTA
	//ra int 		addJobQueueDB();


	/*! build the job queue collection as reading entries in job_queue.log file
	 *  and load them into RDBMS
	 */
	//ra int 		init_read_proc_LogEntry();
	//ra int 		read_proc_LogEntry();

	//ra int			buildJobQueue(JobQueueCollection *jobQueue);
	//ra int			init_JobQueueLoad(JobQueueCollection *jobQueue);
	//ra int			processClassAdLogEntry(int op_type, JobQueueCollection *jobQueue);


	int			cleanupJobQueueDB();
	int			tuneupJobQueueDB();

	int			getProcClusterIds(const char* key, char* cid, char* pid);
	void		displayDBErrorMsg(const char* errmsg);

	int			checkSchema();

	//ra void		addJQPollingInfoSQL(char* dest, char* src_name, char* src_val);

		//
		// data
		//
	//JobQueueDatabase*	jqDatabase;		//!< Job Queue Database
	bool            initialized;
	ODBC            *db_handle;
	enum				XACT_STATE 
						{NOT_IN_XACT, BEGIN_XACT, COMMIT_XACT, ABORT_XACT}; 
	XACT_STATE			xactState;		//!< current XACT state


	//ra char*		jobQueueLogFile; 		//!< Job Queue Log File Path
	char*		jobQueueDBConn;  		//!< DB connection string

	char*		multi_sql_str;			//!< buffer for SQL

	int			addingCount;			//!< how many addJobQueue was invoked
};

#endif /* _QUEUEDBMANAGER_H_ */
