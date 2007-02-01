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
#include "condor_io.h"
#include "pgsqldatabase.h"

const int QUILLPP_HistoryHorFieldNum = 64;
const char *QUILLPP_HistoryHorFields[] ={"ScheddName", "ClusterId", "ProcId", "QDate", "Owner", "GlobalJobId", "NumCkpts", "NumRestarts", "NumSystemHolds", "CondorVersion", "CondorPlatform", "RootDir", "Iwd", "JobUniverse", "Cmd", "MinHosts", "MaxHosts", "JobPrio", "User", "Env", "UserLog", "CoreSize", "KillSig", "Rank", "In", "TransferIn", "Out", "TransferOut", "Err", "TransferErr", "ShouldTransferFiles", "TransferFiles", "ExecutableSize", "DiskUsage", "Requirements", "FileSystemDomain", "Args", "LastMatchTime", "NumJobMatches", "JobStartDate", "JobCurrentStartDate", "JobRunCount", "FileReadCount", "FileReadBytes", "FileWriteCount", "FileWriteBytes", "FileSeekCount", "TotalSuspensions", "ImageSize", "ExitStatus", "LocalUserCpu", "LocalSysCpu", "RemoteUserCpu", "RemoteSysCpu", "BytesSent", "BytesRecvd", "RSCBytesSent", "RSCBytesRecvd", "ExitCode", "JobStatus", "EnteredCurrentStatus", "RemoteWallClockTime", "LastRemoteHost", "CompletionDate", 0};

/* NOTE - we project out a few column names, so this only has the results
   AFTER the select  - ie the "schedd name" field from the database is not
   listed here */

const int proc_field_num = 10;
const char *proc_field_names [] = { "Cluster", "Proc", "JobStatus", "ImageSize", "RemoteUserCpu", "RemoteWallClockTime", "RemoteHost", "GlobalJobId", "JobPrio", "Args" };

const int cluster_field_num = 10;
const char *cluster_field_names [] = { "Cluster", "Owner", "JobStatus", "JobPrio", "ImageSize", "QDate", "RemoteUserCpu", "RemoteWallClockTime", "Cmd", "Args" };

//! constructor
PGSQLDatabase::PGSQLDatabase(const char* connect)
{
	connected = false;
	queryRes = NULL;  

	if (connect != NULL) {
		con_str = (char*)malloc(strlen(connect) + 1);
		strcpy(con_str, connect);
	}
	else {
		con_str = NULL;
	}

	procAdsHorRes = procAdsVerRes = clusterAdsHorRes = clusterAdsVerRes = 
		historyHorRes = historyVerRes = NULL;
}

//! destructor
PGSQLDatabase::~PGSQLDatabase()
{
	queryRes = NULL;  
	if ((connected == true) && (connection != NULL)) {
		PQfinish(connection);
		connected = false;
		connection = NULL;
	}
	
	if (con_str != NULL) {
		free(con_str);
	}

	procAdsHorRes = procAdsVerRes = clusterAdsHorRes = clusterAdsVerRes = 
		historyHorRes = historyVerRes = NULL;  
	
}

//! connect to DB
/*! \param connect DB connect string
 */
QuillErrCode
PGSQLDatabase::connectDB()
{
	if ((connection = PQconnectdb(con_str)) == NULL)
	{
		dprintf(D_ALWAYS, "Fatal error - unable to allocate connection to DB\n");
		return FAILURE;
	}
	
	if (PQstatus(connection) != CONNECTION_OK)
		{
			char *dbname;
			dbname = PQdb(connection);

			dprintf(D_ALWAYS, "Connection to database '%s' failed.\n", dbname);
		  	dprintf(D_ALWAYS, "%s", PQerrorMessage(connection));
			
			dprintf(D_ALWAYS, "Deallocating connection resources to database '%s'\n", dbname);
			PQfinish(connection);
			connection = NULL;
			return FAILURE;
        }

	connected = true;
	
	return SUCCESS;
}

//@ disconnect from DBMS
QuillErrCode
PGSQLDatabase::disconnectDB() 
{
	if ((connected == true) && (connection != NULL))
	{
		PQfinish(connection);
		connection = NULL;
	}

	connected = false;
	return SUCCESS;
}

//! begin Transaction
QuillErrCode 
PGSQLDatabase::beginTransaction() 
{
	PGresult	*result;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "BEGIN;");

		if(result) {
			PQclear(result);		
			result = NULL;
		}

		dprintf(D_FULLDEBUG, "SQL COMMAND: BEGIN TRANSACTION\n");
		return SUCCESS;
	}
	else {
		dprintf(D_ALWAYS, "ERROR STARTING NEW TRANSACTION\n");
		return FAILURE;
	}
}

//! commit Transaction
QuillErrCode 
PGSQLDatabase::commitTransaction()
{
	PGresult	*result;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "COMMIT");

		if(result) {
			PQclear(result);		
			result = NULL;
		}
		dprintf(D_FULLDEBUG, "SQL COMMAND: COMMIT TRANSACTION\n");
		return SUCCESS;
	}
	else {
		dprintf(D_ALWAYS, "ERROR COMMITTING TRANSACTION\n");
		return FAILURE;
	}
}

//! abort Transaction
QuillErrCode
PGSQLDatabase::rollbackTransaction()
{
	PGresult	*result;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "ROLLBACK");

		if(result) {
			PQclear(result);		
			result = NULL;
		}

		dprintf(D_FULLDEBUG, "SQL COMMAND: ROLLBACK TRANSACTION\n");
		return SUCCESS;
	}
	else {
		return FAILURE;
	}
}

/*! execute a command
 *
 *  execaute SQL which doesn't have any retrieved result, such as
 *  insert, delete, and udpate.
 *
 */
QuillErrCode 
PGSQLDatabase::execCommand(const char* sql, 
						   int &num_result)
{
	PGresult 	*result;
	char*		num_result_str = NULL;
	int         db_err_code;
	struct timeval tvStart, tvEnd;

#ifdef TT_TIME_SQL
	gettimeofday( &tvStart, NULL );
#endif	

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);
	if ((result = PQexec(connection, sql)) == NULL)
	{
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR1] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[SQL: %s]\n", sql);
		return FAILURE;
	}
	else if ((PQresultStatus(result) != PGRES_COMMAND_OK) &&
			(PQresultStatus(result) != PGRES_COPY_IN) && 
			(PQresultStatus(result) != PGRES_TUPLES_OK)) {
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR2] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[SQL: %s]\n", sql);
		db_err_code =  atoi(PQresultErrorField(result, PG_DIAG_SQLSTATE));
		dprintf(D_ALWAYS, 
			"[SQLERRORCODE: %d]\n", db_err_code);
		PQclear(result);
		return FAILURE;
	}
	else {
		num_result_str = PQcmdTuples(result);
		if (num_result_str != NULL) {
			num_result = atoi(num_result_str);
		}
	}

#ifdef TT_TIME_SQL
	gettimeofday( &tvEnd, NULL );

	dprintf(D_FULLDEBUG, "Execution time: %d\n", 
			(tvEnd.tv_sec - tvStart.tv_sec)*1000 + 
			(tvEnd.tv_usec - tvStart.tv_usec)/1000);
#endif
	
	if(result) {
		PQclear(result);		
		result = NULL;
	}

	return SUCCESS;
}

QuillErrCode 
PGSQLDatabase::execCommand(const char* sql) 
{
	int num_result = 0;
	return execCommand(sql, num_result);
}


/*! execute a SQL query
 *
 *	NOTE:
 *		queryRes shouldn't be PQcleared
 *		when the query is correctly executed.
 *		It is PQcleared in case of error.
 */
QuillErrCode
PGSQLDatabase::execQuery(const char* sql, 
						 PGresult *&result,
						 int &num_result)
{
	dprintf(D_FULLDEBUG, "SQL Query = %s\n", sql);
	if ((result = PQexec(connection, sql)) == NULL)
	{
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[ERRONEOUS SQL: %s]\n", sql);
		return FAILURE;
	}
	else if (PQresultStatus(result) != PGRES_TUPLES_OK) {
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[ERRONEOUS SQL: %s]\n", sql);
		if(result) {
			PQclear(result);		
			result = NULL;
		}

		return FAILURE;
	}

	num_result = PQntuples(result);

	return SUCCESS;
}


//! execute a SQL query
QuillErrCode
PGSQLDatabase::execQuery(const char* sql, int &num_result) 
{
        return execQuery(sql, queryRes, num_result);
}


/*! execute a SQL query
 *
 *	NOTE:
 *		queryRes shouldn't be PQcleared
 *		when the query is correctly executed.
 *		It is PQcleared in case of error.
 */
QuillErrCode
PGSQLDatabase::execQuery(const char* sql)
{
	int num_result = 0;
	return execQuery(sql, queryRes, num_result);
}

//! execute a SQL query
/*
QuillErrCode
PGSQLDatabase::fetchNext() 
{
	if (row_idx < num_result) {
		row_idx++;
		return SUCCESS;
	}
	else {
		return FAILURE;
	}
}
*/

//! get a result for the executed query
const char*
PGSQLDatabase::getValue(int row, int col)
{
	return PQgetvalue(queryRes, row, col);
}

//! get a result for the executed query as an integer
/*
int
PGSQLDatabase::getIntValue(int col)
{
	if (row_idx > num_result) {
		dprintf(D_ALWAYS, "FATAL ERROR: no more rows to fetch\n");
		return 0; 
	} else {
		return atoi(PQgetvalue(queryRes, row_idx-1, col));
	}
}
*/

//! release the generic query result object
QuillErrCode
PGSQLDatabase::releaseQueryResult()
{
	if(queryRes != NULL) {
	   PQclear(queryRes);
	}
	
	queryRes = NULL;

	return SUCCESS;
}

//! check if the connection is ok
QuillErrCode
PGSQLDatabase::checkConnection()
{
	if (PQstatus(connection) == CONNECTION_OK) {
		dprintf(D_FULLDEBUG, "DB Connection Ok\n");
		return SUCCESS;
	}
	else {
		dprintf(D_FULLDEBUG, "DB Connection BAD\n");
		return FAILURE;
	}
}

//! check if the connection is ok
QuillErrCode
PGSQLDatabase::resetConnection()
{
	PQreset(connection);

	if (PQstatus(connection) == CONNECTION_OK) {
		dprintf(D_FULLDEBUG, "DB Connection Ok\n");
		return SUCCESS;
	}
	else {
		dprintf(D_FULLDEBUG, "DB Connection BAD\n");
		return FAILURE;
	}
}

//! get the field name at given column index from the cluster ads
const char *
PGSQLDatabase::getJobQueueClusterHorFieldName(int col) 
{
	return cluster_field_names[col];

		/* we don't use the column names as in table schema because they 
		   are not exactly what's needed for a classad
		*/
		//return PQfname(clusterAdsHorRes, col);
}

//! get number of fields returned in the horizontal cluster ads
const int 
PGSQLDatabase::getJobQueueClusterHorNumFields() 
{
  return PQnfields(clusterAdsHorRes);
}

//! get the field name at given column index from proc ads
const char *
PGSQLDatabase::getJobQueueProcHorFieldName(int col) 
{
		//return PQfname(procAdsHorRes, col);
	return proc_field_names[col];
}

//! get number of fields in the proc ad horizontal
const int 
PGSQLDatabase::getJobQueueProcHorNumFields() 
{
  return PQnfields(procAdsHorRes);
}

//! get the field name at given column index
const char *
PGSQLDatabase::getHistoryHorFieldName(int col) 
{
	if (col >= QUILLPP_HistoryHorFieldNum) {
		dprintf(D_ALWAYS, "column index %d exceeds max column num %d in PGSQLDatabase::getHistoryHorFieldName.\n", col, QUILLPP_HistoryHorFieldNum);
		return NULL;
	} else {
		return QUILLPP_HistoryHorFields[col];		
	}
		//  return PQfname(historyHorRes, col);
}

//! get number of fields returned in result
const int 
PGSQLDatabase::getHistoryHorNumFields() 
{
		// return PQnfields(historyHorRes);
	return QUILLPP_HistoryHorFieldNum;
}

//! release all history results
QuillErrCode
PGSQLDatabase::releaseHistoryResults()
{
	if(historyHorRes != NULL) {
	   PQclear(historyHorRes);
	}
	historyHorRes = NULL;

	if(historyVerRes != NULL) {
	   PQclear(historyVerRes);
	}
	historyVerRes = NULL;

	return SUCCESS;
}

/*! get the job queue
 *
 *	\return 
 *		JOB_QUEUE_EMPTY: There is no job in the queue
 *      FAILURE_QUERY_* : error querying table *
 *		SUCCESS: There is some job in the queue and query was successful
 *
 *		
 */
QuillErrCode
PGSQLDatabase::getJobQueueDB( int *clusterarray, int numclusters, 
							  int *procarray, int numprocs, 
							  bool isfullscan,
							  const char *scheddname,
							  int& procAdsHorRes_num, int& procAdsVerRes_num,
							  int& clusterAdsHorRes_num, 
							  int& clusterAdsVerRes_num
							  )
{
 
	MyString procAds_hor_query, procAds_ver_query;
	MyString clusterAds_hor_query, clusterAds_ver_query; 
	MyString clusterpredicate, procpredicate, temppredicate;

	QuillErrCode st;
	int i;

	if(isfullscan) {
		procAds_hor_query.sprintf("SELECT cluster_id, proc_id, jobstatus, imagesize, remoteusercpu, remotewallclocktime, remotehost, globaljobid, jobprio,  args  FROM procads_horizontal WHERE scheddname=\'%s\' ORDER BY cluster_id, proc_id;", scheddname);
		procAds_ver_query.sprintf("SELECT cluster_id, proc_id, attr, val FROM procads_vertical WHERE scheddname=\'%s\' ORDER BY cluster_id, proc_id;", scheddname);

		clusterAds_hor_query.sprintf("SELECT cluster_id, owner, jobstatus, jobprio, imagesize, extract(epoch from qdate) as qdate, remoteusercpu, remotewallclocktime, cmd, args FROM clusterads_horizontal WHERE scheddname=\'%s\' ORDER BY cluster_id;", scheddname);

		clusterAds_ver_query.sprintf("SELECT cluster_id, attr, val FROM clusterads_vertical WHERE scheddname=\'%s\' ORDER BY cluster_id;", scheddname);
	}

	/* OK, this is a little confusing.
	 * cluster and proc array are tied together - you can ask for a cluster,
     * or a cluster and a proc, but never just a proc
     * think cluster and procarrays as a an array like this:
     *
     *  42, 1
     *  43, -1
     *  44, 5
     *  44, 6
     *  45, -1
     * 
     * this means return job 42.1, all jobs for cluster 43, only 44.5 and 44.6,
     * and all of cluster 45
     *
     * there is no way to say 'give me proc 5 of every cluster'

		numprocs is never used. numclusters may have redundant information:
		querying for jobs 31.20, 31.21, 31.22..31.25   gives queries likes

		cluster ads hor:  SELECT cluster, owner, jobstatus, jobprio, imagesize, 
           qdate, remoteusercpu, remotewallclocktime, cmd, args  i
            FROM clusterads_horizontal WHERE 
                scheddname='epaulson@swingline.cs.wisc.edu'  AND 
                (cluster = 31) OR (cluster = 31)  OR (cluster = 31)  
                OR (cluster = 31)  OR (cluster = 31)  ORDER BY cluster;

         cluster ads ver: SELECT cluster, attr, val FROM clusterads_vertical 
                     WHERE scheddname='epaulson@swingline.cs.wisc.edu'  AND 
                     (cluster = 31) OR (cluster = 31)  OR (cluster = 31)  OR 
                     (cluster = 31)  OR (cluster = 31)  ORDER BY cluster;

         proc ads hor SELECT cluster, proc, jobstatus, imagesize, remoteusercpu,
             remotewallclocktime, remotehost, globaljobid, jobprio,  args  
            FROM procads_horizontal WHERE 
                     scheddname='epaulson@swingline.cs.wisc.edu'  AND 
                  (cluster = 31 AND proc = 20) OR (cluster = 31 AND proc = 21) 
                 OR (cluster = 31 AND proc = 22)  
                 OR (cluster = 31 AND proc = 23)  
                 OR (cluster = 31 AND proc = 24)  ORDER BY cluster, proc;

         proc ads ver SELECT cluster, proc, attr, val FROM procads_vertical 
             WHERE scheddname='epaulson@swingline.cs.wisc.edu'  
               AND (cluster = 31 AND proc = 20) OR (cluster = 31 AND proc = 21)
            OR (cluster = 31 AND proc = 22)  OR (cluster = 31 AND proc = 23) 
             OR (cluster = 31 AND proc = 24)  ORDER BY cluster, proc;

      --erik, 7.24,2006

	 */


	else {
	    if(numclusters > 0) {
			// build up the cluster predicate
			clusterpredicate.sprintf("%s%d)", 
					" AND ( (cluster = ",clusterarray[0]);
			for(i=1; i < numclusters; i++) {
				clusterpredicate.sprintf_cat( 
				"%s%d) ", " OR (cluster = ", clusterarray[i] );
      		}

			// now build up the proc predicate string. 	
			// first decide how to open it
			 if(procarray[0] != -1) {
					procpredicate.sprintf("%s%d%s%d)", 
							" AND ( (cluster = ", clusterarray[0], 
							" AND proc = ", procarray[0]);
	 		} else {  // no proc for this entry, so only have cluster
					procpredicate.sprintf( "%s%d)", 
								" AND ( (cluster = ", clusterarray[0]);
	 		}
	
			// fill in the rest of hte proc predicate 
	 		// note that we really want to iterate till numclusters and not 
			// numprocs because procarray has holes and clusterarray does not
			for(i=1; i < numclusters; i++) {
				if(procarray[i] != -1) {
					procpredicate.sprintf_cat( "%s%d%s%d) ", 
					" OR (cluster = ",clusterarray[i]," AND proc = ",procarray[i]);
				} else { 
					procpredicate.sprintf_cat( "%s%d) ", 
						" OR (cluster = ", clusterarray[i]);
				}
			} //end offor loop

			// balance predicate strings, since it needs to get
			// and-ed with the schedd name below
			clusterpredicate += " ) ";
			procpredicate += " ) ";
		} // end of numclusters > 0


		procAds_hor_query.sprintf( 
			"SELECT cluster_id, proc_id, jobstatus, imagesize, remoteusercpu, remotewallclocktime, remotehost, globaljobid, jobprio, args FROM procads_horizontal WHERE scheddname=\'%s\' %s ORDER BY cluster_id, proc_id;", scheddname, procpredicate.Value() );

		procAds_ver_query.sprintf(
	"SELECT cluster_id, proc_id, attr, val FROM procads_vertical WHERE scheddname=\'%s\' %s ORDER BY cluster_id, proc_id;", 
			scheddname, procpredicate.Value() );

		clusterAds_hor_query.sprintf(
			"SELECT cluster_id, owner, jobstatus, jobprio, imagesize, extract(epoch from qdate) as qdate, remoteusercpu, remotewallclocktime, cmd, args FROM clusterads_horizontal WHERE scheddname=\'%s\' %s ORDER BY cluster_id;", scheddname, clusterpredicate.Value());

		clusterAds_ver_query.sprintf(
		"SELECT cluster_id, attr, val FROM clusterads_vertical WHERE scheddname=\'%s\' %s ORDER BY cluster_id;", scheddname, clusterpredicate.Value());	
	}

	/*dprintf(D_ALWAYS, "clusterAds_hor_query = %s\n", clusterAds_hor_query.Value());
	dprintf(D_ALWAYS, "clusterAds_ver_query = %s\n", clusterAds_ver_query.Value());
	dprintf(D_ALWAYS, "procAds_hor_query = %s\n", procAds_hor_query.Value());
	dprintf(D_ALWAYS, "procAds_ver_query = %s\n", procAds_ver_query.Value()); */

	  // Query against ClusterAds_Hor Table
  if ((st = execQuery(clusterAds_hor_query.Value(), clusterAdsHorRes, 
					clusterAdsHorRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_HOR;
  }
	  // Query against ClusterAds_Ver Table
  if ((st = execQuery(clusterAds_ver_query.Value(), clusterAdsVerRes, 
					clusterAdsVerRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_VER;
  }
	  // Query against procAds_Hor Table
  if ((st = execQuery(procAds_hor_query.Value(), procAdsHorRes, 
									procAdsHorRes_num)) == FAILURE) {
	  return FAILURE_QUERY_PROCADS_HOR;
  }
	  // Query against procAds_ver Table
  if ((st = execQuery(procAds_ver_query.Value(), procAdsVerRes, 
									procAdsVerRes_num)) == FAILURE) {
	  return FAILURE_QUERY_PROCADS_VER;
  }
  
  if (clusterAdsVerRes_num == 0 && clusterAdsHorRes_num == 0) {
    return JOB_QUEUE_EMPTY;
  }

  return SUCCESS;
}

/*! get the historical information
 *
 *	\return
 *		SUCCESS: declare cursor succeeded 
 *		FAILURE_QUERY_*: query failed
 */
QuillErrCode
PGSQLDatabase::openCursorsHistory(SQLQuery *queryhor, 
								  SQLQuery *queryver,
								  bool longformat)

{  
	QuillErrCode st = SUCCESS;

	if ((st = execCommand(queryhor->getDeclareCursorStmt())) == FAILURE) {
		dprintf(D_ALWAYS, "error opening Jobs_Horizontal_History cursor\n");
		return FAILURE_QUERY_HISTORYADS_HOR;
	}
	if (longformat && 
		(st = execCommand(queryver->getDeclareCursorStmt())) == FAILURE) {
		dprintf(D_ALWAYS, "error opening Jobs_Vertical_History cursor\n");
		return FAILURE_QUERY_HISTORYADS_VER;
	}

		// initialize watermarks
	historyHorNumRowsFetched = 0;
	historyVerNumRowsFetched = 0;

		// initialize current read positions
	historyHorFirstRowIndex = 0;
	historyVerFirstRowIndex = 0;

	return SUCCESS;
}

QuillErrCode
PGSQLDatabase::closeCursorsHistory(SQLQuery *queryhor, 
								   SQLQuery *queryver,
								   bool longformat)

{  
	QuillErrCode st;
	if ((st = execCommand(queryhor->getCloseCursorStmt())) == FAILURE) {
		return FAILURE_QUERY_HISTORYADS_HOR;
	}
	if (longformat 
		&& (st = execCommand(queryver->getCloseCursorStmt())) == FAILURE) {
		return FAILURE_QUERY_HISTORYADS_VER;
	}

	return SUCCESS;
}

//! get a value retrieved from Jobs_Horizontal_History table
QuillErrCode
PGSQLDatabase::getHistoryHorValue(SQLQuery *queryhor, 
								  int row, int col, const char **value)
{
	QuillErrCode st;
	
	if(row < historyHorFirstRowIndex) {
		dprintf(D_ALWAYS, "ERROR: Trying to access Jobs_Horizontal_History\n"); 
		dprintf(D_ALWAYS, "before the start of the current range.\n");
		dprintf(D_ALWAYS, "Backwards iteration is currently not supported\n");
		return FAILURE_QUERY_HISTORYADS_HOR;
	}

	else if(row >= historyHorFirstRowIndex + historyHorNumRowsFetched) {
			// fetch more rows into historyHorRes
		if(historyHorRes != NULL) {
			PQclear(historyHorRes);
			historyHorRes = NULL;
		}
		historyHorFirstRowIndex += historyHorNumRowsFetched;

		if ((st = execQuery(queryhor->getFetchCursorStmt(), 
							historyHorRes, 
							historyHorNumRowsFetched)) == FAILURE) {
			return FAILURE_QUERY_HISTORYADS_HOR;
		}
		if(historyHorNumRowsFetched == 0) {
			return DONE_HISTORY_HOR_CURSOR;
		}
	}

	*value = PQgetvalue(historyHorRes, row - historyHorFirstRowIndex, col);
	return SUCCESS;
}

//! get a value retrieved from Jobs_Vertical_History table
QuillErrCode
PGSQLDatabase::getHistoryVerValue(SQLQuery *queryver, 
								  int row, int col, const char **value)
{
	QuillErrCode st;

	if(row < historyVerFirstRowIndex) {
		dprintf(D_ALWAYS, "ERROR: Trying to access Jobs_Vertical_History\n"); 
		dprintf(D_ALWAYS, "before the start of the current range.\n");
		dprintf(D_ALWAYS, "Backwards iteration is currently not supported\n");
		return FAILURE_QUERY_HISTORYADS_VER;
	}

	else if(row >= historyVerFirstRowIndex + historyVerNumRowsFetched) {
			// fetch more rows into historyVerRes
		if(historyVerRes != NULL) {
			PQclear(historyVerRes);
			historyVerRes = NULL;
		}
		historyVerFirstRowIndex += historyVerNumRowsFetched;

		if((st = execQuery(queryver->getFetchCursorStmt(), 
						   historyVerRes, 
						   historyVerNumRowsFetched)) == FAILURE) {
			return FAILURE_QUERY_HISTORYADS_VER;
		}
		if(historyVerNumRowsFetched == 0) {
			return DONE_HISTORY_VER_CURSOR;
		}
	
	}
	*value = PQgetvalue(historyVerRes, row - historyVerFirstRowIndex, col);

	return SUCCESS;
}

//! get a value retrieved from ProcAds_Hor table
const char*
PGSQLDatabase::getJobQueueProcAds_HorValue(int row, int col)
{
	return PQgetvalue(procAdsHorRes, row, col);
}

//! get a value retrieved from ProcAds_Ver table
const char*
PGSQLDatabase::getJobQueueProcAds_VerValue(int row, int col)
{
	return PQgetvalue(procAdsVerRes, row, col);
}

//! get a value retrieved from ClusterAds_Hor table
const char*
PGSQLDatabase::getJobQueueClusterAds_HorValue(int row, int col)
{
	return PQgetvalue(clusterAdsHorRes, row, col);
}

//! get a value retrieved from ClusterAds_Ver table
const char*
PGSQLDatabase::getJobQueueClusterAds_VerValue(int row, int col)
{
	return PQgetvalue(clusterAdsVerRes, row, col);
}

//! release the result for job queue database
QuillErrCode
PGSQLDatabase::releaseJobQueueResults()
{
	if (procAdsHorRes != NULL) {
		PQclear(procAdsHorRes);
		procAdsHorRes = NULL;
	}
	if (procAdsVerRes != NULL) {
		PQclear(procAdsVerRes);
		procAdsVerRes = NULL;
	}
	if (clusterAdsHorRes != NULL) {
		PQclear(clusterAdsHorRes);
		clusterAdsHorRes = NULL;
	}
	if (clusterAdsVerRes != NULL) {
		PQclear(clusterAdsVerRes);
		clusterAdsVerRes = NULL;
	}

	return SUCCESS;
}

//! get a DBMS error message
char*
PGSQLDatabase::getDBError()
{
	return PQerrorMessage(connection);
}
