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

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

static int QUILL_HistoryHorFieldNum = 14;
static char *QUILL_HistoryHorFields[] ={"CID", "PID", "EnteredHistoryTable", "Owner", "QDate", "RemoteWallClockTime", "RemoteUserCpu", "RemoteSysCpu", "ImageSize", "JobStatus", "JobPrio", "Cmd", "CompletionDate", "LastRemoteHost", 0};

//! constructor
ORACLEDatabase::ORACLEDatabase(const char* connect)
{
	/* local variables */
	char *temp;
 	char host[100]="";
	char port[10]="";
	char dbname[100]="";
	char userName[100]="";
	char password[100]="";
	int  len1, len2, len3;
	char *token;

	/* initialization of class variables */
	connected = false;
	env = NULL;
	conn = NULL;
	historyHorRes = historyVerRes = procAdsStrRes = procAdsNumRes = 
		clusterAdsStrRes = clusterAdsNumRes = queryRes = NULL;

	
	stmt = queryStmt = historyHorStmt = historyVerStmt = 
		procAdsStrStmt = procAdsNumStmt = clusterAdsStrStmt = 
		clusterAdsNumStmt = NULL;

	if (connect != NULL) {
			/* make a copy of connect so that we can tokenize it */
		temp = strdup(connect);

			/* parse the connect string, it's assumed to be the following
			   format and there is no space within the values of any 
			   of the following parameters: 
			   host=%s port=%s user=%s password=%s dbname=%s
			*/
		token = strtok(temp, " ");
		if(token) {
			sscanf(token, "host=%s", host);
		}

		token = strtok(NULL, " ");
		if(token) {
			sscanf(token, "port=%s", port);
		}

		token = strtok(NULL, " ");
		if(token) {
			sscanf(token, "user=%s", userName);
		}

		token = strtok(NULL, " ");
		if(token) {
			sscanf(token, "password=%s", password);
		}

		token = strtok(NULL, " ");
		if(token) {
			sscanf(token, "dbname=%s", dbname);
		}

			/* now we have all parameters, put them into userName, password 
			   and connectString if available, 
			   the connectString has this format: [host[:port]][/database] 
			*/
		len1 = strlen(userName);

		this->userName = (char*)malloc(len1 + 1);
		strcpy(this->userName, userName);		

		len1 = strlen(password);
		this->password = (char*)malloc(len1+1);
		strcpy(this->password, password);

		len1 = strlen(host);
		len2 = strlen(port);
		len3 = strlen(dbname);

		this ->connectString = (char *)malloc(len1 + len2 + len3 + 5);
		this->connectString[0] = '\0';
		if (len1 > 0) {
				/* host is not empty */
			strcat(this->connectString, host);
			if (len2 > 0) {
					/* port is not empty */
				strcat (this->connectString, ":");
				strcat(this->connectString, port);
			}
		}

		if (len3 > 0) {
				/* dbname is not empty */
			strcat(this->connectString, "/");
			strcat(this->connectString, dbname);
		}
    } else {
		this->userName = (char *)malloc(1);
		this->userName[0] = '\0'; 
		this->password = (char *)malloc(1);
		this->password[0] = '\0'; 
		this->connectString = (char *)malloc(1);
		this->connectString[0] = '\0'; 
    }
}

//! destructor
ORACLEDatabase::~ORACLEDatabase()
{
    if (connected && conn != NULL) { 
        // free result set and statement handle if any
	if (queryStmt) {
		if (queryRes) 
			queryStmt->closeResultSet (queryRes);
		conn->terminateStatement (queryStmt);
	}
        
	if (historyHorStmt) {
		if (historyHorRes) 
			historyHorStmt->closeResultSet (historyHorRes);
		conn->terminateStatement (historyHorStmt);
	}
	
	if (historyVerStmt) {
		if (historyVerRes) 
			historyVerStmt->closeResultSet (historyVerRes);
		conn->terminateStatement (historyVerStmt);
	}        
        
	if (procAdsStrStmt) {
		if (procAdsStrRes) 
			procAdsStrStmt->closeResultSet (procAdsStrRes);
		conn->terminateStatement (procAdsStrStmt);
	}
	
	if (procAdsNumStmt) {
		if (procAdsNumRes) 
			procAdsNumStmt->closeResultSet (procAdsNumRes);
		conn->terminateStatement (procAdsNumStmt);
	}
	
	if (clusterAdsStrStmt) {
		if (clusterAdsStrRes) 
			clusterAdsStrStmt->closeResultSet (clusterAdsStrRes);
		conn->terminateStatement (clusterAdsStrStmt);
	}
	
	if (clusterAdsNumStmt) {
		if (clusterAdsNumRes) 
			clusterAdsNumStmt->closeResultSet (clusterAdsNumRes);
		conn->terminateStatement (clusterAdsNumStmt);
	}
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

	if (connectString != NULL) {
		free(connectString);
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
		
		conn = env->createConnection(userName, password, connectString);
	} catch (SQLException ex) {
		
		dprintf(D_ALWAYS, "ERROR CREATING CONNECTION\n");
		dprintf(D_ALWAYS, "Database connect string: %s, User name: %s\n",  connectString, userName);
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
	if ((connected == true) && (env != NULL) && (conn != NULL)) {
		env->terminateConnection(conn);                 
		Environment::terminateEnvironment(env);
		conn = NULL;
		env = NULL;
	}
	
	connected = false;
	return SUCCESS;
}

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

//! begin Transaction
QuillErrCode 
ORACLEDatabase::beginTransaction() 
{
		/* this func is essentially a no-op, any update stmt will automatically
		   begins a tranx 
		*/

	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::beginTransaction\n");
		return FAILURE;
	}

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

	try {
		conn->commit();
	}       catch (SQLException ex) {
		
		dprintf(D_ALWAYS, "ERROR COMMITTING TRANSACTION\n");
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::commitTransaction\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}
		
		return FAILURE;
	}
	
	dprintf(D_FULLDEBUG, "SQL COMMAND: COMMIT TRANSACTION\n");
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

	try {
		conn->rollback();
	}       catch (SQLException ex) {
		
		dprintf(D_ALWAYS, "ERROR ROLLING BACK TRANSACTION\n");
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::rollbackTransaction\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}
		
		return FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL COMMAND: ROLLBACK TRANSACTION\n");
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
        
	try {
		stmt = conn->createStatement (sql);
		num_result = stmt->executeUpdate ();
	} catch (SQLException ex) {
		dprintf(D_ALWAYS, "ERROR EXECUTING UPDATE\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);         
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execCommand\n", ex.getErrorCode(), ex.getMessage().c_str());

		conn->terminateStatement (stmt);
		stmt = NULL;

			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
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
ORACLEDatabase::execQuery(const char* sql,
			  ResultSet *&result,
			  Statement *&stmt,
			  int &num_result)
{
	ResultSet *res;
	ResultSet::Status rs;
	int temp=0;

	if (!connected) {
		dprintf(D_ALWAYS, "Not connected to database in ORACLEDatabase::execQuery\n");
		num_result = -1;
		return FAILURE;
	}

	dprintf(D_FULLDEBUG, "SQL Query = %s\n", sql);

	try {
		stmt = conn->createStatement (sql);
		res = stmt->executeQuery ();
		
			/* fetch from res to count the number of rows */
		while ((rs = res->next()) != ResultSet::END_OF_FETCH) {
			temp++;  	
		}

		stmt->closeResultSet (res);
		
			/* query again to get new result structure to pass back */
		result = stmt->executeQuery();
		num_result = temp;	
		
	} catch (SQLException ex) {
		conn->terminateStatement (stmt);
		result = NULL;
		stmt = NULL;
		
		dprintf(D_ALWAYS, "ERROR EXECUTING QUERY\n");
		dprintf(D_ALWAYS,  "[SQL: %s]\n", sql);         
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::execQuery\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}               
     
		num_result = -1;

		return FAILURE;                 
	}

	return SUCCESS;
}

/*! execute a SQL query
 */
QuillErrCode
ORACLEDatabase::execQuery(const char* sql)
{
  int num_result;
  queryResCursor = -1;
  return execQuery(sql, queryRes, queryStmt, num_result);
}

/*! execute a SQL query
 */
QuillErrCode
ORACLEDatabase::execQuery(const char* sql, int &num_result)
{
  queryResCursor = -1;
  return execQuery(sql, queryRes, queryStmt, num_result);
}

//! get a column from the specified  row for the executed query
// the string value returned must be copied out before calling getValue 
// again.
const char*
ORACLEDatabase::getValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!queryRes) {
		dprintf(D_ALWAYS, "no result to fetch in ORACLEDatabase::getValue\n");
		return NULL;
	}
        
	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < queryResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (queryResCursor < row) {
			rs = queryRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				queryStmt->closeResultSet (queryRes);
				conn->terminateStatement (queryStmt); 
				queryRes = NULL;
				queryStmt = NULL;
				return NULL;
			}

			queryResCursor++;	
		}

			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		cv = queryRes->getString(col+1);
	} catch (SQLException ex) {
		queryStmt->closeResultSet (queryRes);
		conn->terminateStatement (queryStmt);
		queryStmt = NULL;
		queryRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}               
        
		return NULL;                    
	}
	
        
	rv = cv.c_str();

	return rv;
}

// get the field name at given column index
const char *
ORACLEDatabase::getHistoryHorFieldName(int col)
{

/* 
the following are saved codes which uses Oracle API to get column 
metadata, but haven't been made to work due to some error 
"undefined reference to `std::vector<oracle::occi::MetaData ..."
*/

/*
	OCCI_STD_NAMESPACE::vector<MetaData>listOfColumns;

	if (!historyHorRes) {
		dprintf(D_ALWAYS, "no result retrieved in ORACLEDatabase::historyHorRes\n");
		return NULL;
	}	

	try {
		listOfColumns = historyHorRes->getColumnListMetaData();			
	} catch (SQLException ex) { 			
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getHistoryHorNumFields\n", ex.getErrorCode(), ex.getMessage().c_str());
		return NULL;
	}	

	if (col >= listOfColumns.size()) {
		dprintf(D_ALWAYS, "column index %d exceeds max column num %d in ORACLEDatabase::getHistoryHorFieldName.\n", col, listOfColumns.size());
		return NULL;
	} else {
		cv = listOfColumns[col].getString(MetaData::ATTR_NAME);
		return cv.c_str();;
	}
*/

	if (col >= QUILL_HistoryHorFieldNum) {
		dprintf(D_ALWAYS, "column index %d exceeds max column num %d in ORACLEDatabase::getHistoryHorFieldName.\n", col, QUILL_HistoryHorFieldNum);
		return NULL;
	} else {
		return QUILL_HistoryHorFields[col];		
	}

}

// get the number of fields returned in result
const int
ORACLEDatabase::getHistoryHorNumFields()
{

/* 
the following are saved codes which uses Oracle API to get column 
metadata, but haven't been made to work due to some error 
"undefined reference to `std::vector<oracle::occi::MetaData ..."
*/

/*
	OCCI_STD_NAMESPACE::vector<MetaData>listOfColumns;

	if (!historyHorRes) {
		dprintf(D_ALWAYS, "no result retrieved in ORACLEDatabase::historyHorRes\n");
		return -1;
	}	

	try {
		listOfColumns = historyHorRes->getColumnListMetaData();			
	} catch (SQLException ex) { 			
		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getHistoryHorNumFields\n", ex.getErrorCode(), ex.getMessage().c_str());
		return -1;
	}

	return listOfColumns.size();
*/
	return QUILL_HistoryHorFieldNum;
}

//! release the history query result object
QuillErrCode
ORACLEDatabase::releaseHistoryResults()
{
        if(historyHorRes != NULL) {
                if (historyHorStmt != NULL) {
                        historyHorStmt->closeResultSet (historyHorRes);
                        conn->terminateStatement (historyHorStmt);
                        historyHorStmt = NULL;
                } else {
                        dprintf(D_ALWAYS, "ERROR - statement handle is NULL while historyHorRes is not NULL in ORACLEDatabase::releaseHistoryResults\n");
                }

                historyHorRes = NULL;
        }
        
        if(historyVerRes != NULL) {
                if (historyVerStmt != NULL) {
                        historyVerStmt->closeResultSet (historyVerRes);
                        conn->terminateStatement (historyVerStmt);
                        historyVerStmt = NULL;
                } else {
                        dprintf(D_ALWAYS, "ERROR - statement handle is NULL while historyVerRes is not NULL in ORACLEDatabase::releaseHistoryResults\n");
                }

                historyVerRes = NULL;
        }

        return SUCCESS;
}

//! release the job queue query result object
QuillErrCode
ORACLEDatabase::releaseJobQueueResults()
{
        if(procAdsStrRes != NULL) {
                if (procAdsStrStmt != NULL) {
                        procAdsStrStmt->closeResultSet (procAdsStrRes);
                        conn->terminateStatement (procAdsStrStmt);
                        procAdsStrStmt = NULL;
                } else {
                        dprintf(D_ALWAYS, "ERROR - statement handle is NULL while procAdsStrRes is not NULL in ORACLEDatabase::releaseJobQueueResult\n");
                }

                procAdsStrRes = NULL;
        }

		if(procAdsNumRes != NULL) {
			if (procAdsNumStmt != NULL) {
				procAdsNumStmt->closeResultSet (procAdsNumRes);
				conn->terminateStatement (procAdsNumStmt);
				procAdsNumStmt = NULL;
			} else {
				dprintf(D_ALWAYS, "ERROR - statement handle is NULL while procAdsNumRes is not NULL in ORACLEDatabase::releaseJobQueueResult\n");
			}

			procAdsNumRes = NULL;
		}
        
		if(clusterAdsNumRes != NULL) {
			if (clusterAdsNumStmt != NULL) {
				clusterAdsNumStmt->closeResultSet (clusterAdsNumRes);
				conn->terminateStatement (clusterAdsNumStmt);
				clusterAdsNumStmt = NULL;
			} else {
				dprintf(D_ALWAYS, "ERROR - statement handle is NULL while clusterAdsNumRes is not NULL in ORACLEDatabase::releaseJobQueueResult\n");
			}
			
			clusterAdsNumRes = NULL;
        }
        
 		if(clusterAdsStrRes != NULL) {
			if (clusterAdsStrStmt != NULL) {
				clusterAdsStrStmt->closeResultSet (clusterAdsStrRes);
				conn->terminateStatement (clusterAdsStrStmt);
				clusterAdsStrStmt = NULL;
			} else {
				dprintf(D_ALWAYS, "ERROR - statement handle is NULL while clusterAdsStrRes is not NULL in ORACLEDatabase::releaseJobQueueResult\n");
			}
			
			clusterAdsStrRes = NULL;
        }
      
        return SUCCESS;
}

//! release the generic query result object
QuillErrCode
ORACLEDatabase::releaseQueryResult()
{
        if(queryRes != NULL) {
                if (queryStmt != NULL) {
                        queryStmt->closeResultSet (queryRes);
                        conn->terminateStatement (queryStmt);
                        queryStmt = NULL;
                }

                queryRes = NULL;
        }
        
        return SUCCESS;
}

//! get a DBMS error message
char*
ORACLEDatabase::getDBError()
{
	return "DB Error message unknown\n";
}

/*! get the historical information
 *
 *	\return
 *		HISTORY_EMPTY: There is no Job in history
 *		SUCCESS: history is not empty and query succeeded
 *		FAILURE_QUERY_*: query failed
 */
QuillErrCode
ORACLEDatabase::queryHistoryDB(SQLQuery *queryhor, 
			      SQLQuery *queryver, 
			      bool longformat, 
			      int& historyads_hor_num, 
			      int& historyads_ver_num)
{
	QuillErrCode st;
	if ((st = execQuery(queryhor->getQuery(), historyHorRes, historyHorStmt, historyads_hor_num)) == FAILURE) {
		return FAILURE_QUERY_HISTORYADS_HOR;
	}

	if (longformat && (st = execQuery(queryver->getQuery(), historyVerRes, historyVerStmt, historyads_ver_num)) == FAILURE) {
		return FAILURE_QUERY_HISTORYADS_VER;
	}
  
	if (historyads_hor_num == 0) {
		return HISTORY_EMPTY;
	}
	
	historyHorResCursor = historyVerResCursor = -1;

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
ORACLEDatabase::getJobQueueDB(int *clusterarray, int numclusters, 
							  int *procarray, int numprocs,
							  char *owner, bool isfullscan,
							  int& procAdsStrRes_num, 
							  int& procAdsNumRes_num, 
							  int& clusterAdsStrRes_num, 
							  int& clusterAdsNumRes_num)
{
  char *procAds_str_query, *procAds_num_query, *clusterAds_str_query, 
	  *clusterAds_num_query;
  char *clusterpredicate, *procpredicate, *temppredicate;
  QuillErrCode st;
  int i;

  procAds_str_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  procAds_num_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  clusterAds_str_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * 
										 sizeof(char));
  clusterAds_num_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * 
										 sizeof(char));

  if(isfullscan) {
	  strcpy(procAds_str_query, "SELECT cid, pid, attr, val FROM quillwriter.ProcAds_Str ORDER BY cid, pid");
	  strcpy(procAds_num_query, "SELECT cid, pid, attr, val FROM quillwriter.ProcAds_Num ORDER BY cid, pid");
	  strcpy(clusterAds_str_query, "SELECT cid, attr, val FROM quillwriter.ClusterAds_Str ORDER BY cid");
	  strcpy(clusterAds_num_query, "SELECT cid, attr, val FROM quillwriter.ClusterAds_Num ORDER BY cid");	   
  } else { 
	  clusterpredicate = (char *) malloc(1024 * sizeof(char));
	  strcpy(clusterpredicate, "  ");
	  procpredicate = (char *) malloc(1024 * sizeof(char));
	  strcpy(procpredicate, "  ");
	  temppredicate = (char *) malloc(1024 * sizeof(char));
	  strcpy(temppredicate, "  ");
	  
	  if(numclusters > 0) {
		  sprintf(clusterpredicate, "%s%d)", " WHERE (cid = ", 
				  clusterarray[0]);
		  for(i=1; i < numclusters; i++) {
			  sprintf(temppredicate, "%s%d) ", " OR (cid = ", clusterarray[i]);
			  strcat(clusterpredicate, temppredicate); 	 
		  }	  
	  
		  if(procarray[0] != -1) {
			  sprintf(procpredicate, "%s%d%s%d)", " WHERE (cid = ", 
				  clusterarray[0], " AND pid = ", procarray[0]);
		  }
		  else {
			  sprintf(procpredicate, "%s%d)", " WHERE (cid = ", clusterarray[0]);
		  }
	 
		  /* note that we really want to iterate till numclusters 
		  and not numprocs because procarray has holes and 
		  clusterarray does not
		  */
		  for(i=1; i < numclusters; i++) {
			  if(procarray[i] != -1) {
				  sprintf(temppredicate, "%s%d%s%d) ", " OR (cid = ", 
					  clusterarray[i], " AND pid = ", procarray[i]);
				  procpredicate = strcat(procpredicate, temppredicate); 	 
			  } else {
			  	 sprintf(temppredicate, "%s%d) ", " OR (cid = ", clusterarray[i]);
				 procpredicate = strcat(procpredicate, temppredicate); 	 
		  	}
	  	}
	  }	  

	  sprintf(procAds_str_query, "%s %s %s", 
			  "SELECT cid, pid, attr, val FROM quillwriter.ProcAds_Str", 
			  procpredicate,
			  "ORDER BY cid, pid");
	  sprintf(procAds_num_query, "%s %s %s", 
			  "SELECT cid, pid, attr, val FROM quillwriter.ProcAds_Num",
			  procpredicate,
			  "ORDER BY cid, pid");
	  sprintf(clusterAds_str_query, "%s %s %s", 
			  "SELECT cid, attr, val FROM quillwriter.ClusterAds_Str",
			  clusterpredicate,
			  "ORDER BY cid");
	  sprintf(clusterAds_num_query, "%s %s %s",
			  "SELECT cid, attr, val FROM quillwriter.ClusterAds_Num",
			  clusterpredicate,
			  "ORDER BY cid");	   

	  free(clusterpredicate);
	  free(procpredicate);
	  free(temppredicate);	  
  }

 	  // Query against ProcAds_Str Table
  if ((st = execQuery(procAds_str_query, procAdsStrRes, procAdsStrStmt, procAdsStrRes_num)) == FAILURE) {
	  return FAILURE_QUERY_PROCADS_STR;
  }
  
	  // Query against ProcAds_Num Table
  if ((st = execQuery(procAds_num_query, procAdsNumRes, procAdsNumStmt, procAdsNumRes_num)) == FAILURE) {
	  return FAILURE_QUERY_PROCADS_NUM;
  }
  
	  // Query against ClusterAds_Str Table
  if ((st = execQuery(clusterAds_str_query, clusterAdsStrRes, clusterAdsStrStmt, clusterAdsStrRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_STR;
  }	 
	  // Query against ClusterAds_Num Table
  if ((st = execQuery(clusterAds_num_query, clusterAdsNumRes, clusterAdsNumStmt, clusterAdsNumRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_NUM;
  }
  
  free(procAds_str_query);
  free(procAds_num_query);
  free(clusterAds_str_query);
  free(clusterAds_num_query);

  if (clusterAdsNumRes_num == 0 && clusterAdsStrRes_num == 0) {
	  return JOB_QUEUE_EMPTY;
  }

  procAdsStrResCursor = procAdsNumResCursor = clusterAdsStrResCursor = 
	  clusterAdsNumResCursor = -1;

  return SUCCESS; 
}

//! get a value retrieved from ProcAds_Str table
const char*
ORACLEDatabase::getJobQueueProcAds_StrValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!procAdsStrRes) {
		dprintf(D_ALWAYS, "no procAdsStrRes to fetch in ORACLEDatabase::getJobQueueProcAds_StrValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < procAdsStrResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueProcAds_StrValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (procAdsStrResCursor < row) {
			rs = procAdsStrRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				procAdsStrStmt->closeResultSet (procAdsStrRes);
				conn->terminateStatement (procAdsStrStmt); 
				procAdsStrRes = NULL;
				procAdsStrStmt = NULL;
				return NULL;
			}

			procAdsStrResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		cv = procAdsStrRes->getString(col+1);		
	} catch (SQLException ex) {
		procAdsStrStmt->closeResultSet (procAdsStrRes);
		conn->terminateStatement (procAdsStrStmt);
		procAdsStrStmt = NULL;
		procAdsStrRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueProcAds_StrValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}               
        
		return NULL;             		
	}

	rv = cv.c_str();

	return rv;
}

//! get a value retrieved from ProcAds_Num table
const char*
ORACLEDatabase::getJobQueueProcAds_NumValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!procAdsNumRes) {
		dprintf(D_ALWAYS, "no procAdsNumRes to fetch in ORACLEDatabase::getJobQueueProcAds_NumValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < procAdsNumResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueProcAds_NumValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (procAdsNumResCursor < row) {
			rs = procAdsNumRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				procAdsNumStmt->closeResultSet (procAdsNumRes);
				conn->terminateStatement (procAdsNumStmt); 
				procAdsNumRes = NULL;
				procAdsNumStmt = NULL;
				return NULL;
			}

			procAdsNumResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		cv = procAdsNumRes->getString(col+1);		
	} catch (SQLException ex) {
		procAdsNumStmt->closeResultSet (procAdsNumRes);
		conn->terminateStatement (procAdsNumStmt);
		procAdsNumStmt = NULL;
		procAdsNumRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueProcAds_NumValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}               
        
		return NULL;             		
	}

	rv = cv.c_str();

	return rv;
}

//! get a value retrieved from ClusterAds_Str table
const char*
ORACLEDatabase::getJobQueueClusterAds_StrValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!clusterAdsStrRes) {
		dprintf(D_ALWAYS, "no clusterAdsStrRes to fetch in ORACLEDatabase::getJobQueueClusterAds_StrValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < clusterAdsStrResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueClusterAds_StrValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (clusterAdsStrResCursor < row) {
			rs = clusterAdsStrRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				clusterAdsStrStmt->closeResultSet (clusterAdsStrRes);
				conn->terminateStatement (clusterAdsStrStmt); 
				clusterAdsStrRes = NULL;
				clusterAdsStrStmt = NULL;
				return NULL;
			}

			clusterAdsStrResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		cv = clusterAdsStrRes->getString(col+1);		
	} catch (SQLException ex) {
		clusterAdsStrStmt->closeResultSet (clusterAdsStrRes);
		conn->terminateStatement (clusterAdsStrStmt);
		clusterAdsStrStmt = NULL;
		clusterAdsStrRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueClusterAds_StrValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}               
        
		return NULL;             		
	}

	rv = cv.c_str();

	return rv;	
}

//! get a value retrieved from ClusterAds_Num table
const char*
ORACLEDatabase::getJobQueueClusterAds_NumValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!clusterAdsNumRes) {
		dprintf(D_ALWAYS, "no clusterAdsNumRes to fetch in ORACLEDatabase::getJobQueueClusterAds_NumValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < clusterAdsNumResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueClusterAds_NumValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (clusterAdsNumResCursor < row) {
			rs = clusterAdsNumRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				clusterAdsNumStmt->closeResultSet (clusterAdsNumRes);
				conn->terminateStatement (clusterAdsNumStmt); 
				clusterAdsNumRes = NULL;
				clusterAdsNumStmt = NULL;
				return NULL;
			}

			clusterAdsNumResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		cv = clusterAdsNumRes->getString(col+1);		
	} catch (SQLException ex) {
		clusterAdsNumStmt->closeResultSet (clusterAdsNumRes);
		conn->terminateStatement (clusterAdsNumStmt);
		clusterAdsNumStmt = NULL;
		clusterAdsNumRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueClusterAds_NumValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}               
        
		return NULL;             		
	}

	rv = cv.c_str();

	return rv;		
}

//! get a value retrieved from History_Horizontal table
const char*
ORACLEDatabase::getHistoryHorValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!historyHorRes) {
		dprintf(D_ALWAYS, "no historyHorRes to fetch in ORACLEDatabase::getJobQueueHistoryHorValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < historyHorResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueHistoryHorValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (historyHorResCursor < row) {
			rs = historyHorRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				historyHorStmt->closeResultSet (historyHorRes);
				conn->terminateStatement (historyHorStmt); 
				historyHorRes = NULL;
				historyHorStmt = NULL;
				return NULL;
			}

			historyHorResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		cv = historyHorRes->getString(col+1);		
	} catch (SQLException ex) {
		historyHorStmt->closeResultSet (historyHorRes);
		conn->terminateStatement (historyHorStmt);
		historyHorStmt = NULL;
		historyHorRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueHistoryHorValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}               
        
		return NULL;             		
	}

	rv = cv.c_str();

	return rv;			
}

//! get a value retrieved from History_Vertical table
const char*
ORACLEDatabase::getHistoryVerValue(int row, int col)
{
	ResultSet::Status rs;
	const char *rv;

	if (!historyVerRes) {
		dprintf(D_ALWAYS, "no historyVerRes to fetch in ORACLEDatabase::getJobQueueHistoryVerValue\n");
		return NULL;
	}

	try {
			/* if we are trying to fetch a row which is past, 
			   error out 
			*/
		if (row < historyVerResCursor) {
			dprintf(D_ALWAYS, "Fetching previous row is not supported in ORACLEDatabase::getJobQueueHistoryVerValue\n");
			return NULL;
		}

			/* first position to the row as specified */
		while (historyVerResCursor < row) {
			rs = historyVerRes->next (); 

				/* the requested row doesn't exist */	
			if (rs == ResultSet::END_OF_FETCH) {
				historyVerStmt->closeResultSet (historyVerRes);
				conn->terminateStatement (historyVerStmt); 
				historyVerRes = NULL;
				historyVerStmt = NULL;
				return NULL;
			}

			historyVerResCursor++;	
		}
		
			/* col index is 0 based, for oracle, since col index 
			   is 1 based, therefore add 1 to the column index 
			*/
		cv = historyVerRes->getString(col+1);		
	} catch (SQLException ex) {
		historyVerStmt->closeResultSet (historyVerRes);
		conn->terminateStatement (historyVerStmt);
		historyVerStmt = NULL;
		historyVerRes = NULL;

		dprintf(D_ALWAYS, "Error number: %d, Error message: %s in ORACLEDatabase::getJobQueueHistoryVerValue\n", ex.getErrorCode(), ex.getMessage().c_str());
		
			/* ORA-03113 means that the connection between Client 
			   and Server process was broken.
			*/
		if (ex.getErrorCode() == 3113) {
			disconnectDB();
		}               
        
		return NULL;             		
	}

	rv = cv.c_str();

	return rv;				
}

//! get the server version number, 
// oracle Connection->getServerVersion()
// returns a version string, therefore for now just
// return 10 to conform to the interface.
//! -1 if connection is invalid
int 
ORACLEDatabase::getDatabaseVersion() 
{
	return 10;
}
