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
		result = PQexec(connection, "BEGIN");

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
			(PQresultStatus(result) != PGRES_COPY_IN)) {
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

/*
QuillErrCode 
PGSQLDatabase::execCommand(const char* sql, 
						   int nParams,
						   const dataType *paramTypes,
						   const char * const *paramValues,
						   const int *paramLengths,
						   const int *paramFormats,
						   int &num_result)
{
	PGresult 	*result;
	char*		num_result_str = NULL;
	int         db_err_code;

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);

	if ((result = PQexecParams(connection, sql, 
						 nParams, NULL,
						 paramValues,
						 paramLengths,
						 paramFormats,
						 0)) == NULL)
	{
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR1] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[SQL: %s]\n", sql);
		return FAILURE;
	}
	else if ((PQresultStatus(result) != PGRES_COMMAND_OK) &&
			(PQresultStatus(result) != PGRES_COPY_IN)) {
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

	if(result) {
		PQclear(result);		
		result = NULL;
	}

	return SUCCESS;
}
*/

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
	dprintf(D_FULLDEBUG, "SQL Query = %s\n", sql);
	if ((queryRes = PQexec(connection, sql)) == NULL)
	{
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[ERRONEOUS SQL: %s]\n", sql);
		return FAILURE;
	}
	else if (PQresultStatus(queryRes) != PGRES_TUPLES_OK) {
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR] %s\n", PQerrorMessage(connection));
		dprintf(D_ALWAYS, 
			"[ERRONEOUS SQL: %s]\n", sql);
		if(queryRes) {
			PQclear(queryRes);		
			queryRes = NULL;
		}

		return FAILURE;
	}

	num_result = PQntuples(queryRes);
	row_idx = 0;

	return SUCCESS;
}

//! execute a SQL query
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

//! get a result for the executed query
const char*
PGSQLDatabase::getValue(int col)
{
	if (row_idx > num_result) {
		dprintf(D_ALWAYS, "FATAL ERROR: no more rows to fetch\n");
		return NULL;
	} else {
		return PQgetvalue(queryRes, row_idx-1, col);
	}
}

//! get a result for the executed query as an integer
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
