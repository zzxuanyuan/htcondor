/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "ManagedDatabase.h"
#include "dbms_utils.h"
#include "condor_config.h"
#include "pgsqldatabase.h"

#undef ATTR_VERSION
#include "oracledatabase.h"

ManagedDatabase::ManagedDatabase() {
	char *tmp;

	if (param_boolean("QUILL_ENABLED", false) == false) {
		EXCEPT("Quill++ is currently disabled. Please set QUILL_ENABLED to "
			   "TRUE if you want this functionality and read the manual "
			   "about this feature since it requires other attributes to be "
			   "set properly.");
	}

		//bail out if no SPOOL variable is defined since its used to 
		//figure out the location of the quillWriter password file
	char *spool = param("SPOOL");
	if(!spool) {
		EXCEPT("No SPOOL variable found in config file\n");
	}	

		/*
		  Here we try to read the database parameters in config
		  the db ip address format is <ipaddress:port> 
		*/
	dt = getConfigDBType();
	dbIpAddress = param("QUILL_DB_IP_ADDR");
	dbName = param("QUILL_DB_NAME");
	dbUser = param("QUILL_DB_USER");

	dbConnStr = getDBConnStr(dbIpAddress,
							 dbName,
							 dbUser,
							 spool);

	dprintf(D_ALWAYS, "Using Database Type = %s\n",
			(dt == T_ORACLE)?"ORACLE":"Postgres");
	dprintf(D_ALWAYS, "Using Database IpAddress = %s\n", 
			dbIpAddress?dbIpAddress:"");
	dprintf(D_ALWAYS, "Using Database Name = %s\n", 
			dbName?dbName:"");
	dprintf(D_ALWAYS, "Using Database User = %s\n", 
			dbUser?dbUser:"");

	if (spool) {
		free(spool);
	}

	switch (dt) {				
	case T_ORACLE:
		DBObj = new ORACLEDatabase(dbConnStr);
		break;
	case T_PGSQL:
		DBObj = new PGSQLDatabase(dbConnStr);
		break;
	default:
		break;
	}		

	tmp = param("Quill_RESOURCE_HISTORY_DURATION");
	if (tmp) {
		resourceHistoryDuration= atoi(tmp);
		free(tmp);
	} else {
			/* default to a week of resource history info */
		resourceHistoryDuration = 7;
	}

	tmp = param("Quill_RUN_HISTORY_DURATION");
	if (tmp) {
		runHistoryDuration= atoi(tmp);
		free(tmp);
	} else {
			/* default to a week of job run information */
		runHistoryDuration = 7;
	}

	tmp = param("Quill_JOB_HISTORY_DURATION");	
	if (tmp) {
		jobHistoryDuration= atoi(tmp);
		free(tmp);
	} else {
			/* default to 10 years of job history */
		jobHistoryDuration = 3650;
	}

	tmp = param("Quill_DBSIZE_LIMIT");	
	if (tmp) {
		dbSizeLimit= atoi(tmp);
		free(tmp);
	} else {
			/* default to 20 GB */
		dbSizeLimit = 20;
	}
		
}

ManagedDatabase::~ManagedDatabase() {
	if (dbIpAddress) {
		free(dbIpAddress);
		dbIpAddress = NULL;
	}

	if (dbName) {
		free(dbName);
		dbName = NULL;
	}

	if (dbUser) {
		free(dbUser);
		dbUser = NULL;
	}

	if (dbConnStr) {
		free(dbConnStr);
		dbConnStr = NULL;
	}

	if (DBObj) {
		delete DBObj;
	}
}

void ManagedDatabase::PurgeDatabase() {
	QuillErrCode ret_st;
	int len;
	int dbsize;
	int num_result;

	ret_st = DBObj->connectDB();

		/* call the puging routine */
	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "ManagedDatabase::PurgeDatabase: unable to connect to DB--- ERROR\n");
		return;
	}	

	len = 2048;
	char *sql_str = (char *) malloc (len * sizeof(char));

	switch (dt) {				
	case T_ORACLE:
		snprintf(sql_str, len, "EXECUTE quill_purgeHistory(%d, %d, %d)", 
				 resourceHistoryDuration,
				 runHistoryDuration,
				 jobHistoryDuration);
		break;
	case T_PGSQL:
		snprintf(sql_str, len, "select quill_purgeHistory(%d, %d, %d)", 
				 resourceHistoryDuration,
				 runHistoryDuration,
				 jobHistoryDuration);
		break;
	default:
			// can't have this case
		free(sql_str);
		ASSERT(0);
		break;
	}

	ret_st = DBObj->execCommand(sql_str);
	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "ManagedDatabase::PurgeDatabase --- ERROR [SQL] %s\n", 
				sql_str);
	}

		// release the result structure in case it is created (e.g. pgsql)*/
	DBObj->releaseQueryResult();
	
		/* query the space usage and if it is above some threshold, send
		  a warning to the administrator 
		*/
	
	snprintf(sql_str, len, "SELECT dbsize FROM quillDBMonitor");
	ret_st = DBObj->execQuery(sql_str, num_result);

	if ((ret_st == SUCCESS) && 
		(num_result == 1)) {
		dbsize = atoi(DBObj->getValue(0, 0));		
		
			/* if dbsize is bigger than 75% the dbSizeLimit, send a 
			   warning to the administrator with information about 
			   the situation and suggestion for looking at the table 
			   sizes using the following sql statement and tune down
			   the *HistoryDuration parameters accordingly.

			   This is an oracle version of sql for examining the 
			   table sizes in descending order, sql for other 
			   databases can be similarly constructed:
			   SELECT NUM_ROWS*AVG_ROW_LEN, table_name
			   FROM USER_TABLES
			   ORDER BY  NUM_ROWS*AVG_ROW_LEN DESC;
			*/
		if (dbsize/1024 > dbSizeLimit) {
				/* notice that dbsize is stored in unit of MB, but
				   dbSizeLimit is stored in unit of GB */
			dprintf(D_ALWAYS, "Current database size (> %d MB) is bigger than 75 percent of the limit (%d GB).", 
					dbsize, dbSizeLimit);

				// send a warning email to admin 
				// add this later
		} 

	} else {
		dprintf(D_ALWAYS, "Reading quillDBMonitor --- ERROR or returned # of rows is not exactly one [SQL] %s\n", 
				sql_str);		
	}
	
	free(sql_str); 		

	DBObj->releaseQueryResult();

	ret_st = DBObj->disconnectDB();
	if (ret_st == FAILURE) {
		dprintf(D_ALWAYS, "ManagedDatabase::disconnectDB: unable to disconnect --- ERROR\n");
		return;
	}		
}
