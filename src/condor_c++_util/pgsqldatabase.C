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

#ifdef _POSTGRESQL_DBMS_

#include "condor_common.h"
#include "condor_io.h"

#include "pgsqldatabase.h"

//! constructor
PGSQLDatabase::PGSQLDatabase()
{
  connected = false;
  con_str = NULL;
  procAdsStrRes = procAdsNumRes = clusterAdsStrRes = clusterAdsNumRes = queryRes = historyHorRes = historyVerRes = NULL;  
}

//! constructor
PGSQLDatabase::PGSQLDatabase(const char* connect)
{
  connected = false;
  procAdsStrRes = procAdsNumRes = clusterAdsStrRes = clusterAdsNumRes = queryRes = historyHorRes = historyVerRes = NULL;  
  if (connect != NULL) {
    con_str = (char*)malloc(strlen(connect) + 1);
    strcpy(con_str, connect);
  }
  else
    con_str = NULL;
  
}

//! destructor
PGSQLDatabase::~PGSQLDatabase()
{
  procAdsStrRes = procAdsNumRes = clusterAdsStrRes = clusterAdsNumRes = queryRes = historyHorRes = historyVerRes = NULL;  
	if ((connected == true) && (connection != NULL))
	{
		PQfinish(connection);
		connected = false;
		connection = NULL;
	}

	if (con_str != NULL) free(con_str);
	
}

//! connect to DB
int
PGSQLDatabase::connectDB()
{
	return connectDB(con_str);
}

//! connect to DB
/*! \param connect DB connect string
 */
int
PGSQLDatabase::connectDB(const char* connect)
{
	if ((connection = PQconnectdb(connect)) == NULL)
	{
		dprintf(D_ALWAYS, "Fatal error - unable to allocate connection to DB\n");
		return 0;
	}
	
	if (PQstatus(connection) != CONNECTION_OK)
		{
			dprintf(D_ALWAYS, "Connection to database '%s' failed.\n", PQdb(connection));
		  	dprintf(D_ALWAYS, "%s", PQerrorMessage(connection));
			
			dprintf(D_ALWAYS, "Deallocating connection resources to database '%s'\n", PQdb(connection));
			PQfinish(connection);
			connection = NULL;
			return 0;
        }
		//dprintf(D_ALWAYS, "right after calling PQconnectdb\n");
	connected = true;
	
	return 1;
}

//! get a DBMS error message
char*
PGSQLDatabase::getDBError()
{
	return PQerrorMessage(connection);
}

//@ disconnect from DBMS
int 
PGSQLDatabase::disconnectDB() 
{
	if ((connected == true) && (connection != NULL))
	{
		PQfinish(connection);
		connection = NULL;
	}

	connected = false;
	return 1;
}

//! begin Transaction
int 
PGSQLDatabase::beginTransaction() 
{
	PGresult	*result;
		//FILE *fp;
	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "BEGIN");
		//dprintf(D_ALWAYS, "STARTING NEW TRANSACTION\n");
		//fp = fopen("/scratch/akini/logdump", "a");
		//fprintf(fp, "BEGIN;\n");
		//fclose(fp);
		if(result) {
			PQclear(result);		
			result = NULL;
		}
		return 1;
	}
	else {
		dprintf(D_ALWAYS, "ERROR STARTING NEW TRANSACTION\n");
		return 0;
	}
}

//! commit Transaction
int 
PGSQLDatabase::commitTransaction()
{
	PGresult	*result;
		//FILE *fp;
	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "COMMIT");
		//dprintf(D_ALWAYS, "COMMITTING TRANSACTION\n");
		//fp = fopen("/scratch/akini/logdump", "a");
		//fprintf(fp, "COMMIT;\n");
		//fclose(fp);

		if(result) {
			PQclear(result);		
			result = NULL;
		}
		return 1;
	}
	else {
		dprintf(D_ALWAYS, "ERROR COMMITTING TRANSACTION\n");
		return 0;
	}
}

//! abort Transaction
int 
PGSQLDatabase::rollbackTransaction()
{
	PGresult	*result;

		//FILE *fp;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "ROLLBACK");
	  	//dprintf(D_ALWAYS, "ROLLBACKING TRANSACTION\n");
		//fp = fopen("/scratch/akini/logdump", "a");
		//fprintf(fp, "ROLLBACK;\n");
		//fclose(fp);
		if(result) {
			PQclear(result);		
			result = NULL;
		}

		return 1;
	}
	else
		return 0;
}

/*! execute a command
 *
 *  execaute SQL which doesn't have any retrieved result, such as
 *  insert, delete, and udpate.
 *
 *	\return: 1: success, -1: fail
 */
int 
PGSQLDatabase::execCommand(const char* sql)
{
	PGresult 	*result;
	char*		num_result_str = NULL;
	int		num_result = 0, error_code = 0;
		//FILE *fp;

	/*fp = fopen("/scratch/akini/logdump", "a");
	fprintf(fp, "%s\n", sql);
	fclose(fp);*/

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);
	if ((result = PQexec(connection, sql)) == NULL)
	{
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR1] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[SQL: %s]\n", sql);
		return -1;
	}
	else if ((PQresultStatus(result) != PGRES_COMMAND_OK) &&
			(PQresultStatus(result) != PGRES_COPY_IN)) {
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR2] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[SQL: %s]\n", sql);
		error_code =  atoi(PQresultErrorField(result, PG_DIAG_SQLSTATE));
		dprintf(D_ALWAYS, 
			"[SQLERRORCODE: %d]\n", error_code);
		PQclear(result);
		return -1 * error_code;
	}
	else {
		num_result_str = PQcmdTuples(result);
		if (num_result_str != NULL)
			num_result = atoi(num_result_str);
	}
	
	if(result) {
		PQclear(result);		
		result = NULL;
	}

	/*analyzeCounter++;
	  if(analyzeCounter == 1000) {
	  result = PQexec(connection,"ANALYZE;");
	  if(result == NULL || (PQresultStatus(result) == PGRES_COMMAND_OK)) {
	  fp = fopen("/scratch/akini/logdump", "a");
	  fprintf(fp, "ANALYZE\n");
	  fclose(fp);
	  }
	  else {
	  fp = fopen("/scratch/akini/logdump", "a");
	  fprintf(fp, "ANALYZE problem\n");
	  fclose(fp);
	  }
	  
	  PQclear(result);
	  analyzeCounter = 0;
	  }
	*/
	return num_result;
}

/*! execute a SQL query
 *
 *	NOTE:
 *		queryRes shouldn't be PQcleared
 *		when the query is correctly executed.
 *		It is PQcleared in case of error.
 *	\return:
 *		-1: fail
 *		 n: the number of returned tuples
 */
int 
PGSQLDatabase::execQuery(const char* sql, PGresult*& result)
{
	dprintf(D_FULLDEBUG, "SQL Query = %s\n", sql);
	if ((result = PQexec(connection, sql)) == NULL)
	{
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[ERRONEOUS SQL: %s]\n", sql);
		return -1;
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

		return -1;
	}

	return PQntuples(result);
}

//! execute a SQL query
int
PGSQLDatabase::execQuery(const char* sql) 
{
	return execQuery(sql, queryRes);
}

//! get the field name at given column index
const char *
PGSQLDatabase::getHistoryHorFieldName(int col) 
{
  return PQfname(historyHorRes, col);
}

//! get number of fields returned in result
const int 
PGSQLDatabase::getHistoryHorNumFields() 
{
  return PQnfields(historyHorRes);
}

//! get a result for the executed query
const char*
PGSQLDatabase::getValue(int row, int col)
{
	return PQgetvalue(queryRes, row, col);
}

//! release the generic query result object
int
PGSQLDatabase::releaseQueryResult()
{
	if(queryRes != NULL) 
	   PQclear(queryRes);
	
	queryRes = NULL;

	return 1;
}

//! release all history results
int
PGSQLDatabase::releaseHistoryResults()
{
	if(historyHorRes != NULL) 
	   PQclear(historyHorRes);
	historyHorRes = NULL;

	if(historyVerRes != NULL) 
	   PQclear(historyVerRes);
	historyVerRes = NULL;

	return 1;
}


/*! get the whole job queue database
 *
 *	\return
 *		0: There is no Job in Job Queue
 *		1: There is some
 *		-1, -2, -3, -4: Error
 */
int
PGSQLDatabase::getJobQueueDB(int cluster, int proc, char *owner, bool isfullscan,
			     int& procAdsStrRes_num, int& procAdsNumRes_num, 
			     int& clusterAdsStrRes_num, int& clusterAdsNumRes_num)
{
  
  char *procAds_str_query, *procAds_num_query, *clusterAds_str_query, *clusterAds_num_query;
  char *clusterpredicate, *procpredicate;

  procAds_str_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  procAds_num_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  clusterAds_str_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  clusterAds_num_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));

  if(isfullscan) {
    strcpy(procAds_str_query, "SELECT cid, pid, attr, val FROM ProcAds_Str ORDER BY cid, pid;");
    strcpy(procAds_num_query, "SELECT cid, pid, attr, val FROM ProcAds_Num ORDER BY cid, pid;");
    strcpy(clusterAds_str_query, "SELECT cid, attr, val FROM ClusterAds_Str ORDER BY cid;");
    strcpy(clusterAds_num_query, "SELECT cid, attr, val FROM ClusterAds_Num ORDER BY cid;");	   
  }

  else {
    clusterpredicate = (char *) malloc(1024 * sizeof(char));
    strcpy(clusterpredicate, "  ");
    procpredicate = (char *) malloc(1024 * sizeof(char));
    strcpy(procpredicate, "  ");

    if(cluster != -1) {
      sprintf(clusterpredicate, " %s %d ", "WHERE cid =", cluster);
    }
    if(proc != -1) {
      sprintf(procpredicate, " %s %d ", "and pid =", proc);
    }

    sprintf(procAds_str_query, "%s %s %s %s", 
	    "SELECT cid, pid, attr, val FROM ProcAds_Str", 
	    clusterpredicate,
	    procpredicate,
	    "ORDER BY cid, pid;");
    sprintf(procAds_num_query, "%s %s %s %s", 
	    "SELECT cid, pid, attr, val FROM ProcAds_Num",
	    clusterpredicate,
	    procpredicate,
	    "ORDER BY cid, pid;");
    sprintf(clusterAds_str_query, "%s %s %s", 
	    "SELECT cid, attr, val FROM ClusterAds_Str",
	    clusterpredicate,
	    "ORDER BY cid;");
    sprintf(clusterAds_num_query, "%s %s %s",
	    "SELECT cid, attr, val FROM ClusterAds_Num",
	    clusterpredicate,
	    "ORDER BY cid;");	   

    free(clusterpredicate);
    free(procpredicate);

  }
  // Query against ProcAds_Str Table
  if ((procAdsStrRes_num = execQuery(procAds_str_query, procAdsStrRes)) < 0)
    return -1;
  // Query against ProcAds_Num Table
  if ((procAdsNumRes_num = execQuery(procAds_num_query, procAdsNumRes)) < 0)
    return -2;
  // Query against ClusterAds_Str Table
  if ((clusterAdsStrRes_num = execQuery(clusterAds_str_query, clusterAdsStrRes)) < 0)
    return -3;
  // Query against ProcAds_Str Table
  if ((clusterAdsNumRes_num = execQuery(clusterAds_num_query, clusterAdsNumRes)) < 0)
    return -4;
  
  free(procAds_str_query);
  free(procAds_num_query);
  free(clusterAds_str_query);
  free(clusterAds_num_query);

  if (clusterAdsNumRes_num == 0 && clusterAdsStrRes_num == 0)
    return 0;
  
  return 1;
}

/*! get the whole history database
 *
 *	\return
 *		0: There is no Job in history
 *		1: There is some
 *		-1, -2, -3, -4: Error
 */
int
PGSQLDatabase::queryHistoryDB(SQLQuery *queryhor, 
			      SQLQuery *queryver, 
			      bool longformat, 
			      int& historyads_hor_num, 
			      int& historyads_ver_num)
{  
  if ((historyads_hor_num = execQuery(queryhor->getQuery(), historyHorRes)) < 0)
    return -1;
  if (longformat && (historyads_ver_num = execQuery(queryver->getQuery(), historyVerRes)) < 0)
    return -1;
  
  if (historyads_hor_num == 0)
    return 0;
  
  return 1;
}

//! get a value retrieved from History_Horizontal table
const char*
PGSQLDatabase::getHistoryHorValue(int row, int col)
{
	return PQgetvalue(historyHorRes, row, col);
}

//! get a value retrieved from History_Vertical table
const char*
PGSQLDatabase::getHistoryVerValue(int row, int col)
{
	return PQgetvalue(historyVerRes, row, col);
}

//! get a value retrieved from ProcAds_Str table
const char*
PGSQLDatabase::getJobQueueProcAds_StrValue(int row, int col)
{
	return PQgetvalue(procAdsStrRes, row, col);
}

//! get a value retrieved from ProcAds_Num table
const char*
PGSQLDatabase::getJobQueueProcAds_NumValue(int row, int col)
{
	return PQgetvalue(procAdsNumRes, row, col);
}

//! get a value retrieved from ClusterAds_Str table
const char*
PGSQLDatabase::getJobQueueClusterAds_StrValue(int row, int col)
{
	return PQgetvalue(clusterAdsStrRes, row, col);
}

//! get a value retrieved from ClusterAds_Num table
const char*
PGSQLDatabase::getJobQueueClusterAds_NumValue(int row, int col)
{
	return PQgetvalue(clusterAdsNumRes, row, col);
}

//! release the result for job queue database
int
PGSQLDatabase::releaseJobQueueDB()
{
	if (procAdsStrRes != NULL) {
		PQclear(procAdsStrRes);
		procAdsStrRes = NULL;
	}
	if (procAdsNumRes != NULL) {
		PQclear(procAdsNumRes);
		procAdsNumRes = NULL;
	}
	if (clusterAdsStrRes != NULL) {
		PQclear(clusterAdsStrRes);
		clusterAdsStrRes = NULL;
	}
	if (clusterAdsNumRes != NULL) {
		PQclear(clusterAdsNumRes);
		clusterAdsNumRes = NULL;
	}

	return 1;
}	


//! put a bulk data into DBMS
int
PGSQLDatabase::sendBulkData(char* data)
{
  dprintf(D_FULLDEBUG, "bulk copy data = %s\n\n", data);
  
  if (PQputCopyData(connection, data, strlen(data)) <= 0)
    {
      dprintf(D_ALWAYS, 
	      "[Bulk Data Sending ERROR] %s\n", PQerrorMessage(connection));
      dprintf(D_ALWAYS, 
	      "[Data: %s]\n", data);
      return -1;
    }
  
  return 1;
}

//! put an end flag for bulk loading
int
PGSQLDatabase::sendBulkDataEnd()
{
	PGresult* result;

	if (PQputCopyEnd(connection, NULL) < 0)
	{
		dprintf(D_ALWAYS, 
			"[Bulk Data End Sending ERROR] %s\n", PQerrorMessage(connection));
		return -1;
	}

	
	if ((result = PQgetResult(connection)) != NULL) {
		if (PQresultStatus(result) != PGRES_COMMAND_OK) {
			dprintf(D_ALWAYS, 
				"[Bulk Last Data Sending ERROR] %s\n", PQerrorMessage(connection));
			PQclear(result);
			return -1;
		}
	}

	if(result) {
		PQclear(result);		
		result = NULL;
	}

	return 1;
}

#endif // _POSTGRESQL_DBMS_
