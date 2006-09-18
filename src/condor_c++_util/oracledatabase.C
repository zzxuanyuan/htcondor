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
#include "oracledatabase.h"

//! constructor
ORACLEDatabase::ORACLEDatabase(const char* userName, const char* password, 
							   const char* serviceName)
{
	connected = false;
	env = NULL;
	conn = NULL;
	in_tranx = false;
	rset = NULL;
	stmt = NULL;

	if (userName != NULL) {
		this->userName = (char*)malloc(strlen(userName) + 1);
		strcpy(this->userName, userName);		
	} else {
		this->userName = (char*)malloc(1);
		this->userName[0] = '\0';
	}

	if (password != NULL) {
		this->password = (char*)malloc(strlen(password) + 1);
		strcpy(this->password, password);		
	} else {
		this->password = (char*)malloc(1);
		this->password[0] = '\0';
	}

	if (serviceName != NULL) {
		this->serviceName = (char*)malloc(strlen(serviceName) + 1);
		strcpy(this->serviceName, serviceName);		
	} else {
		this->serviceName = (char*)malloc(1);
		this->serviceName[0] = '\0';
	}

#ifdef TT_COLLECT_SQL
	sqllog_fp = fopen("trace.sql", "a");
#endif 

}

//! destructor
ORACLEDatabase::~ORACLEDatabase()
{
		// free result set and statement handle if any
	if (stmt) {
		if (rset) 
			stmt->closeResultSet (rset);
		conn->terminateStatement (stmt);
	}
	
		// free connection and environment handle 
	if (connected == true) {
		if (conn != NULL) {
			env->terminateConnection(conn);			
		}

		if (env != NULL) {
			Environment::terminateEnvironment(env);
		}

		connected = false;
	}
	
		// free string memory 
	if (userName != NULL) {
		free(userName);
	}
	
	if (password != NULL) {
		free(password);
	}

	if (serviceName != NULL) {
		free(serviceName);
	}
}

//! connect to DB
QuillErrCode
ORACLEDatabase::connectDB()
{
	try {
		env = Environment::createEnvironment(Environment::OBJECT);

		if (env == NULL) {
			dprintf(D_ALWAYS, "ERROR CREATING Environment in ORACLEDatabase::connectDB, Check if ORACLE environment variables are set correctly\n");
			return FAILURE;
		}

		conn = env->createConnection(userName, password, serviceName);
	} catch (SQLException ex) {

		dprintf(D_ALWAYS, "ERROR CREATING CONNECTION\n");
		dprintf(D_ALWAYS, "Database service name: %s, User name: %s\n", 
				serviceName, userName);
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::connectDB\n", ex.getErrorCode(), ex.getMessage().c_str());

		return FAILURE;
	}
	connected = true;	
	return SUCCESS;
}

//@ disconnect from DBMS
QuillErrCode
ORACLEDatabase::disconnectDB() 
{
	if ((connected == true) && (env != NULL) && (conn != NULL))
	{
		env->terminateConnection(conn);			
		Environment::terminateEnvironment(env);
		conn = NULL;
		env = NULL;
	}

	connected = false;
	return SUCCESS;
}

//! begin Transaction
QuillErrCode 
ORACLEDatabase::beginTransaction() 
{
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::beginTransaction\n");
		return FAILURE;
	}

	if (in_tranx) {
		dprintf(D_ALWAYS, "Can't start a tranx within a tranx in ORACLEDatabase::beginTransaction\n");
		return FAILURE;
	}

	in_tranx = true;
	return SUCCESS;
}

//! commit Transaction
QuillErrCode 
ORACLEDatabase::commitTransaction()
{	
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::commitTransaction\n");
		return FAILURE;
	}

#ifdef TT_COLLECT_SQL
	fprintf(sqllog_fp, "COMMIT;\n");
	fflush(sqllog_fp);
#endif

	try {
		conn->commit();
	}	catch (SQLException ex) {

		dprintf(D_ALWAYS, "ERROR COMMITTING TRANSACTION\n");
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::commitTransaction\n", ex.getErrorCode(), ex.getMessage().c_str());

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}

		in_tranx = false;
		return FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: COMMIT TRANSACTION\n");
	in_tranx = false;
	return SUCCESS;
}

//! abort Transaction
QuillErrCode
ORACLEDatabase::rollbackTransaction()
{
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::rollbackTransaction\n");
		return FAILURE;
	}

#ifdef TT_COLLECT_SQL
	fprintf(sqllog_fp, "ROLLBACK;\n");
	fflush(sqllog_fp);
#endif

	try {
		conn->rollback();
	}	catch (SQLException ex) {

		dprintf(D_ALWAYS, "ERROR ROLLING BACK TRANSACTION\n");
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::rollbackTransaction\n", ex.getErrorCode(), ex.getMessage().c_str());

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}

		in_tranx = false;
		return FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: ROLLBACK TRANSACTION\n");
	in_tranx = false;
	return SUCCESS;
}

/*! execute a command
 *
 *  execaute SQL which doesn't have any retrieved result, such as
 *  insert, delete, and udpate.
 *
 */
QuillErrCode 
ORACLEDatabase::execCommand(const char* sql, 
						   int &num_result)
{
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::execCommand\n");
		return FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);
	
#ifdef TT_COLLECT_SQL
	fprintf(sqllog_fp, "%s;\n", sql);
	fflush(sqllog_fp);
#endif

	try {
		stmt = conn->createStatement (sql);
		num_result = stmt->executeUpdate ();
	} catch (SQLException ex) {
		dprintf(D_ALWAYS, "ERROR EXECUTING UPDATE\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);		
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execCommand\n", ex.getErrorCode(), ex.getMessage().c_str());

		conn->terminateStatement (stmt);
		stmt = NULL;

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			 */
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}

		return FAILURE;				
	}

	conn->terminateStatement (stmt);	
	stmt = NULL;
	return SUCCESS;
}

QuillErrCode 
ORACLEDatabase::execCommand(const char* sql) 
{
	int num_result = 0;
	return execCommand(sql, num_result);
}


/*! execute a SQL query
 */
QuillErrCode
ORACLEDatabase::execQuery(const char* sql)
{
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::execQuery\n");
		return FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL Query = %s\n", sql);

#ifdef TT_COLLECT_SQL
	fprintf(sqllog_fp, "%s;\n", sql);
	fflush(sqllog_fp);
#endif
	
	try {
		stmt = conn->createStatement (sql);
		rset = stmt->executeQuery ();
	} catch (SQLException ex) {
		conn->terminateStatement (stmt);
		rset = NULL;
		stmt = NULL;
		
		dprintf(D_ALWAYS, "ERROR EXECUTING QUERY\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);		
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execQuery\n", ex.getErrorCode(), ex.getMessage().c_str());

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			 */
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}		
	
		return FAILURE;			
	}

	return SUCCESS;
}

QuillErrCode
ORACLEDatabase::fetchNext() 
{
	ResultSet::Status rs;

	if (!rset) {
		dprintf(D_ALWAYS, "no result to fetch in ORACLEDatabase::fetchNext\n");
		return FAILURE;
	}

	try {
		rs = rset->next ();
	}  catch (SQLException ex) {
		stmt->closeResultSet (rset);
		conn->terminateStatement (stmt);
		stmt = NULL;
		rset = NULL;

		dprintf(D_ALWAYS, "ERROR FETCHING NEXT\n");
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::fetchNext\n", ex.getErrorCode(), ex.getMessage().c_str());

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			 */
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}		
	
		return FAILURE;			
	}

	if (rs == ResultSet::END_OF_FETCH) {
		stmt->closeResultSet (rset);
		conn->terminateStatement (stmt); 
		rset = NULL;
		stmt = NULL;
		return FAILURE;
	} else {
		return SUCCESS;
	}
}

//! get a column from the current row for the executed query
// the string value returned must be copied out before calling getValue 
// again.
const char*
ORACLEDatabase::getValue(int col)
{
	const char *rv;

	if (!rset) {
		dprintf(D_ALWAYS, "no result to fetch in ORACLEDatabase::getValue\n");
		return NULL;
	}
	
	try {
			/* col index is 0 based, for oracle, since col index is 1 based, 
			   therefore add 1 to the column index 
			*/
		cv = rset->getString(col+1);
	} catch (SQLException ex) {
		stmt->closeResultSet (rset);
		conn->terminateStatement (stmt);
		stmt = NULL;
		rset = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getValue\n", ex.getErrorCode(), ex.getMessage().c_str());

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			 */
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}		
	
		return NULL;			
	}
	
	rv = cv.c_str();

	return rv;
}

//! get a column from the current row for the executed query as an integer
int
ORACLEDatabase::getIntValue(int col)
{
	int rv;

	if (!rset) {
		dprintf(D_ALWAYS, "no result to fetch in ORACLEDatabase::getIntValue\n");
		return 0;
	}
	
	try {
			/* col index is 0 based, for oracle, since col index is 1 based, 
			   therefore add 1 to the column index 
			*/
		rv = rset->getInt(col+1);
	} catch (SQLException ex) {
		stmt->closeResultSet (rset);
		conn->terminateStatement (stmt);
		stmt = NULL;
		rset = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getIntValue\n", ex.getErrorCode(), ex.getMessage().c_str());

			/* ORA-03113 means that the connection between Client and Server 
			   process was broken.
			 */
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}		
	
		return 0;			
	}

	return rv;
}

//! release the generic query result object
QuillErrCode
ORACLEDatabase::releaseQueryResult()
{
	if(rset != NULL) {
		if (stmt != NULL) {
			stmt->closeResultSet (rset);
			conn->terminateStatement (stmt);
			stmt = NULL;
		} else {
			dprintf(D_ALWAYS, "ERROR - statement handle is NULL while result set is not NULL in ORACLEDatabase::releaseQueryResult\n");
		}

		rset = NULL;
	}
	
	return SUCCESS;
}

/*
QuillErrCode 
ORACLEDatabase::execCommand(const char* sql, 
							int nParams,
							const dataType *paramTypes,
							const char * const *paramValues,
							const int *paramLengths,
							const int *paramFormats,
							int &num_result)
{
	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::execCommand\n");
		return FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);

	try{
		stmt=conn->createStatement (sql);

			// bind parameters
		for (int i = 0; i < nParams; i++) {
			switch (paramTypes[i]) {
			case T_INT:
				if (paramValues[i] == NULL) {
					stmt->setNull(i, OCCIINT);
				} else {
					stmt->setInt (i, atoi(paramValues[i]));
				}
				break;
			case T_STRING:
				if (paramValues[i] == NULL) {
					stmt->setNull(i, OCCISTRING);
				} else {				
					stmt->setString (i, paramValues[i]);
				}
				break;
			case T_DOUBLE:
				if (paramValues[i] == NULL) {
					stmt->setNull(i, OCCIDOUBLE);
				} else {						
					stmt->setDouble (i, atof(paramValues[i]));
				}
				break;
			case T_TIMESTAMP:
				if (paramValues[i] == NULL) {
					stmt->setNull(i, OCCITIMESTAMP);
				} else {					
					Timestamp ts;
					ts.fromText(paramValues[i], "MM/DD/YY HH:MM:SS TZ");
					stmt->setTimestamp(i, ts);
				}
				break;				
			default:
				dprintf(D_ALWAYS, "unsupported type of parameter: %d\n", paramTypes[i]);
				conn->terminateStatement (stmt);
				stmt = NULL;
				return FAILURE;
				break;
			}
		}

		stmt->executeUpdate ();

	} catch(SQLException ex) {
		conn->terminateStatement (stmt);
		stmt = NULL;

		dprintf(D_ALWAYS, "ERROR EXECUTING UPDATE\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);		
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execCommand\n", ex.getErrorCode(), ex.getMessage().c_str());

		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}
		return FAILURE;				
	}

	conn->terminateStatement (stmt);	
	stmt = NULL;
	return SUCCESS;	
}
*/

//! check if the connection is ok
QuillErrCode
ORACLEDatabase::checkConnection()
{
	if (connected) 
		return SUCCESS;
	else 
		return FAILURE;
}

//! check if the connection is ok
QuillErrCode
ORACLEDatabase::resetConnection()
{
	return connectDB();
}
