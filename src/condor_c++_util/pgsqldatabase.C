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
#include "condor_io.h"

#include "libpq-fe.h"
#include "pgsqldatabase.h"

//! constructor
PGSQLDatabase::PGSQLDatabase()
{
	connected = false;
	con_str = NULL;
}

//! constructor
PGSQLDatabase::PGSQLDatabase(const char* connect)
{
	connected = false;

	if (connect != NULL) {
		con_str = (char*)malloc(strlen(connect) + 1);
		strcpy(con_str, connect);
		con_str[strlen(connect)] = '\0';
	}
	else
		con_str = NULL;
}

//! destructor
PGSQLDatabase::~PGSQLDatabase()
{
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
		printf( "Fatal error - unable to allocate connection to DB\n");
		return 0;
	}

        if (PQstatus(connection) != CONNECTION_OK)
        {
                printf( "Connection to database '%s' failed.\n", PQdb(connection));
                printf( "%s", PQerrorMessage(connection));
                return 0;
        }
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

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "BEGIN");
		return 1;
	}
	else
		return 0;
}

//! commit Transaction
int 
PGSQLDatabase::commitTransaction()
{
	PGresult	*result;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "COMMIT");
		return 1;
	}
	else
		return 0;
}

//! abort Transaction
int 
PGSQLDatabase::rollbackTransaction()
{
	PGresult	*result;

	if (PQstatus(connection) == CONNECTION_OK)
	{
		result = PQexec(connection, "ROLLBACK");
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
	int			num_result = 0;

	//FILE *fp = fopen("/scratch/akini/logdump", "a");
	if ((result = PQexec(connection, sql)) == NULL)
	{
	  printf( 
		 "[SQL EXECUTION ERROR1] %s\n", PQerrorMessage(connection));
	  printf( 
		 "[SQL: %s]\n", sql);
	  //fprintf(fp,
	  //   	 "[SQL error1 %s: %s]\n",  PQerrorMessage(connection), sql);
	  //fclose(fp);
	  return -1;
	}
	else if ((PQresultStatus(result) != PGRES_COMMAND_OK) &&
			(PQresultStatus(result) != PGRES_COPY_IN)) {
	  printf( 
		 "[SQL EXECUTION ERROR2] %s\n", PQerrorMessage(connection));
	  printf( 
		 "[SQL: %s]\n", sql);
	  //fprintf(fp,
	  //	 "[SQL error2 %s: %s]\n",  PQerrorMessage(connection), sql);
          //fclose(fp);
	  PQclear(result);
	  return -1;
	}
	else {
          //fprintf(fp, 
	  //	  "[SQL success: %s]\n", sql);
          //fclose(fp);
	  num_result_str = PQcmdTuples(result);
	  if (num_result_str != NULL)
	    num_result = atoi(num_result_str);
	}

	PQclear(result);
	return num_result;
}

/*! execute a SQL query
 *
 *	NOTE:
 *		queryResult shouldn't be PQcleared
 *		when the query is correctly executed.
 *		It is PQcleared in case of error.
 *	\return:
 *		-1: fail
 *		 n: the number of returned tuples
 */
int 
PGSQLDatabase::execQuery(const char* sql, PGresult*& result)
{
	int resultTupleNum = 0;

	if ((result = PQexec(connection, sql)) == NULL)
	{
		fprintf(stdout, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		fprintf(stdout, 
			"[SQL: %s]\n", sql);
		return -1;
	}
	else if (PQresultStatus(result) != PGRES_TUPLES_OK) {
		fprintf(stdout, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		fprintf(stdout, 
			"[SQL: %s]\n", sql);
		PQclear(result);
		return -1;
	}

	resultTupleNum = PQntuples(result);

	return PQntuples(result);
}

//! execute a SQL query
int
PGSQLDatabase::execQuery(const char* sql) 
{
	return execQuery(sql, queryResult);
}

//! get a result for the executed query
const char*
PGSQLDatabase::getValue(int row, int col)
{
	return PQgetvalue(queryResult, row, col);
}

//! release a query result
int
PGSQLDatabase::releaseQueryResult()
{
	PQclear(queryResult);

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
PGSQLDatabase::getJobQueueDB(int& procAds_Str_num, int& procAds_Num_num, int& clusterAds_Str_num, int& clusterAds_Num_num)
{
	// Query against ProcAds_Str Table
	if ((procAds_Str_num = execQuery(
	"SELECT cid, pid, attr, val FROM ProcAds_Str ORDER BY cid, pid;", 
			procAds_Str)) < 0)
		return -1;
	// Query against ProcAds_Num Table
	if ((procAds_Num_num = execQuery(
	"SELECT cid, pid, attr, val FROM ProcAds_Num ORDER BY cid, pid;", 
			procAds_Num)) < 0)
		return -2;
	// Query against ClusterAds_Str Table
	if ((clusterAds_Str_num = execQuery(
		"SELECT cid, attr, val FROM ClusterAds_Str ORDER BY cid;", 
			clusterAds_Str)) < 0)
		return -3;
	// Query against ProcAds_Str Table
	if ((clusterAds_Num_num = execQuery(
		"SELECT cid, attr, val FROM ClusterAds_Num ORDER BY cid;", 
			clusterAds_Num)) < 0)
		return -4;

	if (clusterAds_Num_num == 0)
		return 0;

	return 1;
}

//! get a value retrieved from ProcAds_Str table
const char*
PGSQLDatabase::getJobQueueProcAds_StrValue(int row, int col)
{
	return PQgetvalue(procAds_Str, row, col);
}

//! get a value retrieved from ProcAds_Num table
const char*
PGSQLDatabase::getJobQueueProcAds_NumValue(int row, int col)
{
	return PQgetvalue(procAds_Num, row, col);
}

//! get a value retrieved from ClusterAds_Str table
const char*
PGSQLDatabase::getJobQueueClusterAds_StrValue(int row, int col)
{
	return PQgetvalue(clusterAds_Str, row, col);
}

//! get a value retrieved from ClusterAds_Num table
const char*
PGSQLDatabase::getJobQueueClusterAds_NumValue(int row, int col)
{
	return PQgetvalue(clusterAds_Num, row, col);
}

//! release the result for job queue database
int
PGSQLDatabase::releaseJobQueueDB()
{
	if (procAds_Str != NULL) {
		PQclear(procAds_Str);
		procAds_Str = NULL;
	}
	if (procAds_Num != NULL) {
		PQclear(procAds_Num);
		procAds_Num = NULL;
	}
	if (clusterAds_Str != NULL) {
		PQclear(clusterAds_Str);
		clusterAds_Str = NULL;
	}
	if (clusterAds_Num != NULL) {
		PQclear(clusterAds_Num);
		clusterAds_Num = NULL;
	}

	return 1;
}	

