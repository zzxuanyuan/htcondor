
/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_io.h"
#include "mysqldatabase.h"


//const int QUILLPP_HistoryHorFieldNum = 62;
//const char *QUILLPP_HistoryHorFields[] ={"ScheddName", "ClusterId", "ProcId", "QDate", "Owner", "GlobalJobId", "NumCkpts", "NumRestarts", "NumSystemHolds", "CondorVersion", "CondorPlatform", "RootDir", "Iwd", "JobUniverse", "Cmd", "MinHosts", "MaxHosts", "JobPrio", "User", "Env", "UserLog", "CoreSize", "KillSig", "In", "TransferIn", "Out", "TransferOut", "Err", "TransferErr", "ShouldTransferFiles", "TransferFiles", "ExecutableSize", "DiskUsage", "FileSystemDomain", "Args", "LastMatchTime", "NumJobMatches", "JobStartDate", "JobCurrentStartDate", "JobRunCount", "FileReadCount", "FileReadBytes", "FileWriteCount", "FileWriteBytes", "FileSeekCount", "TotalSuspensions", "ImageSize", "ExitStatus", "LocalUserCpu", "LocalSysCpu", "RemoteUserCpu", "RemoteSysCpu", "BytesSent", "BytesRecvd", "RSCBytesSent", "RSCBytesRecvd", "ExitCode", "JobStatus", "EnteredCurrentStatus", "RemoteWallClockTime", "LastRemoteHost", "CompletionDate", 0};
//const int QUILLPP_HistoryHorIsQuoted[] ={/*"ScheddName"*/1, /*"ClusterId"*/0, /*"ProcId"*/0, /*"QDate"*/0, /*"Owner"*/1, /*"GlobalJobId"*/1, /*"NumCkpts"*/0, /*"NumRestarts"*/0, /*"NumSystemHolds"*/0, /*"CondorVersion"*/1, /*"CondorPlatform"*/1, /*"RootDir"*/1, /*"Iwd"*/1, /*"JobUniverse"*/0, /*"Cmd"*/1, /*"MinHosts"*/0, /*"MaxHosts"*/0, /*"JobPrio"*/0, /*"User"*/1, /*"Env"*/1, /*"UserLog"*/1, /*"CoreSize"*/0, /*"KillSig"*/1, /*"In"*/1, /*"TransferIn"*/0, /*"Out"*/1, /*"TransferOut"*/0, /*"Err"*/1, /*"TransferErr"*/0, /*"ShouldTransferFiles"*/1, /*"TransferFiles"*/1, /*"ExecutableSize"*/0, /*"DiskUsage"*/0, /*"FileSystemDomain"*/1, /*"Args"*/1, /*"LastMatchTime"*/0, /*"NumJobMatches"*/0, /*"JobStartDate"*/0, /*"JobCurrentStartDate"*/0, /*"JobRunCount"*/0, /*"FileReadCount"*/0, /*"FileReadBytes"*/0, /*"FileWriteCount"*/0, /*"FileWriteBytes"*/0, /*"FileSeekCount"*/0, /*"TotalSuspensions"*/0, /*"ImageSize"*/0, /*"ExitStatus"*/0, /*"LocalUserCpu"*/0, /*"LocalSysCpu"*/0, /*"RemoteUserCpu"*/0, /*"RemoteSysCpu"*/0, /*"BytesSent"*/0, /*"BytesRecvd"*/0, /*"RSCBytesSent"*/0, /*"RSCBytesRecvd"*/0, /*"ExitCode"*/0, /*"JobStatus"*/0, /*"EnteredCurrentStatus"*/0, /*"RemoteWallClockTime"*/0, /*"LastRemoteHost"*/1, /*"CompletionDate"*/0};
// 
//
///* NOTE - we project out a few column names, so this only has the results
//   AFTER the select  - ie the "schedd name" field from the database is not
//   listed here */
//const int proc_field_num = 13;
//const char *proc_field_names [] = { "Cluster", "Proc", "JobStatus", "ImageSize", "RemoteUserCpu", "RemoteWallClockTime", "RemoteHost", "GlobalJobId", "JobPrio", "Args", "ShadowBday", "EnteredCurrentStatus", "NumRestarts" };
//
//const int proc_field_is_quoted [] = { /*"Cluster"*/ 0, /*"Proc"*/0, /*"JobStatus"*/0, /*"ImageSize"*/0, /*"RemoteUserCpu"*/0, /*"RemoteWallClockTime"*/0, /*"RemoteHost"*/1, /*"GlobalJobId"*/1, /*"JobPrio"*/0, /*"Args"*/1, /*"ShadowBday"*/0, /*"EnteredCurrentStatus"*/0, /*"NumRestarts"*/0 };
//
//const int cluster_field_num = 11;
//const char *cluster_field_names [] = { "Cluster", "Owner", "JobStatus", "JobPrio", "ImageSize", "QDate", "RemoteUserCpu", "RemoteWallClockTime", "Cmd", "Args", "JobUniverse" };
//
//const int cluster_field_is_quoted [] = { /*"Cluster"*/0, /*"Owner"*/1, /*"JobStatus"*/0, /*"JobPrio"*/0, /*"ImageSize"*/0, /*"QDate"*/0, /*"RemoteUserCpu"*/0, /*"RemoteWallClockTime"*/0, /*"Cmd"*/1, /*"Args"*/1, /*"JobUniverse"*/0 };



//simple utility routine to malloc space for a string and strcpy it
static void copy_str(char** dest, const char* src) {
    if (src != NULL && dest != NULL) {
        *dest = (char*)malloc(strlen(src)+1);
        strcpy(*dest, src);
    } else {
        *dest = NULL;
    }
}

//! constructor
MYSQLDatabase::MYSQLDatabase(const char* _host, const char* _port, const char* _user, const char* _pass, const char* _db)
{
    connected = false;
	result = NULL;

    copy_str(&host, _host);
    copy_str(&port, _port);
    copy_str(&user, _user);
    copy_str(&pass, _pass);
    copy_str(&db, _db);

	conn = mysql_init(NULL);
    if (conn) {
        connected = true;
    } else {
        EXCEPT("error initializing mysql library, mysql_init() failed");
    }
}

MYSQLDatabase::MYSQLDatabase(const char* connect)
{
	EXCEPT("PROGRAMMER ERROR: MYSQLDatabase(const char* connect) constructor should never be called on mysql");
}

//! destructor
MYSQLDatabase::~MYSQLDatabase()
{
    if (conn) {
        mysql_close(conn);
        conn = NULL;;
    }
    if (result) {
        mysql_free_result(result);
        result = NULL;
    }
    connected = false;
    mysql_library_end();
}

//! connect to DB
/*! \param connect DB connect string
 */
QuillErrCode
MYSQLDatabase::connectDB()
{
    if (mysql_real_connect(conn,host,user,pass,db,0,NULL,0)) {
        connected = true;
        return QUILL_SUCCESS;
    } else {
        dprintf(D_ALWAYS,"error: %s\n",mysql_error(conn));
        connected = false;
        return QUILL_FAILURE; 
    }
}

//@ disconnect from DBMS
QuillErrCode
MYSQLDatabase::disconnectDB() 
{
    if (conn) {
        mysql_close(conn);
        conn = NULL;
        connected = false;
    }
    return QUILL_SUCCESS;
}

//! begin Transaction
QuillErrCode 
MYSQLDatabase::beginTransaction() 
{
    int err = mysql_query(conn,"BEGIN");
    if (err == 0) {
		dprintf(D_FULLDEBUG, "SQL COMMAND: BEGIN TRANSACTION\n");
        return QUILL_SUCCESS;
    } else {
        fprintf(stderr,"error during mysql_query(): %s\n",mysql_error(conn));
		dprintf(D_ALWAYS, "ERROR STARTING NEW TRANSACTION\n");
        return QUILL_FAILURE;
    }
}

//! commit Transaction
QuillErrCode 
MYSQLDatabase::commitTransaction()
{
    int err = mysql_query(conn,"COMMIT");
    if (err == 0) {
		dprintf(D_FULLDEBUG, "SQL COMMAND: COMMIT TRANSACTION\n");
        return QUILL_SUCCESS;
    } else {
        fprintf(stderr,"error during mysql_query(): %s\n",mysql_error(conn));
		dprintf(D_ALWAYS, "ERROR COMMITTING TRANSACTION\n");
        return QUILL_FAILURE;
    }
}

//! abort Transaction
QuillErrCode
MYSQLDatabase::rollbackTransaction()
{
    int err = mysql_query(conn,"ROLLBACK");
    if (err == 0) {
		dprintf(D_FULLDEBUG, "SQL COMMAND: ROLLBACK TRANSACTION\n");
        return QUILL_SUCCESS;
    } else {
        fprintf(stderr,"error during mysql_query(): %s\n",mysql_error(conn));
		dprintf(D_ALWAYS, "ERROR ROLLING BACK TRANSACTION\n");
        return QUILL_FAILURE;
    }
}

/*! execute a command
 *
 *  execaute SQL which doesn't have any retrieved result, such as
 *  insert, delete, and udpate.
 *
 */
QuillErrCode 
MYSQLDatabase::execCommand(const char* sql, 
						   int &num_result)
{
    if (execQuery(sql) == QUILL_SUCCESS) {
        num_result = numRows;
        return QUILL_SUCCESS;
    } else {
        return QUILL_FAILURE;
    }
}

QuillErrCode 
MYSQLDatabase::execCommand(const char* sql) 
{
    return execQuery(sql);
}


//! execute a SQL query
QuillErrCode
MYSQLDatabase::execQuery(const char* sql, int &num_result) 
{
    if (execQuery(sql) == QUILL_SUCCESS) {
        num_result = numRows;
        return QUILL_SUCCESS;
    } else {
        return QUILL_FAILURE;
    }
}


/*! execute a SQL query
 */
QuillErrCode
MYSQLDatabase::execQuery(const char* sql)
{

    ASSERT(sql);

#ifdef TT_TIME_SQL
	struct timeval tvStart, tvEnd;
	gettimeofday( &tvStart, NULL );
#endif	

	dprintf(D_FULLDEBUG, "SQL COMMAND: %s\n", sql);

	if (mysql_query(conn, sql) != 0) {
		dprintf(D_ALWAYS, 
			"[SQL EXECUTION ERROR] %s\n", mysql_error(conn));
		dprintf(D_ALWAYS, 
			"[SQL: %s]\n", sql);

		return QUILL_FAILURE;

	} else {
        result = mysql_store_result(conn);
        if (result) {
            numFields = mysql_num_fields(result);
            numRows = mysql_num_rows(result);
        } else {
            if (mysql_field_count(conn) == 0) {
                numRows = mysql_affected_rows(conn);
            } else {
                dprintf(D_ALWAYS, "[SQL EXECUTION ERROR] Query should have returned data but did not - %s\n", mysql_error(conn));
                dprintf(D_ALWAYS, "[SQL: %s]\n", sql);

                return QUILL_FAILURE;
            }
        }
    }

#ifdef TT_TIME_SQL
	gettimeofday( &tvEnd, NULL );

	dprintf(D_FULLDEBUG, "Execution time: %ld\n", 
			(tvEnd.tv_sec - tvStart.tv_sec)*1000 + 
			(tvEnd.tv_usec - tvStart.tv_usec)/1000);
#endif

	return QUILL_SUCCESS;
}


//! get a result for the executed query
const char*
MYSQLDatabase::getValue(int row, int col)
{
static char* emptyString = "";

    if (result) {
        if (row >= 0 && row < numRows && col >= 0 && col < numFields) {
            mysql_data_seek(result,row);
            MYSQL_ROW r = mysql_fetch_row(result);
            if (r[col]) {
                return r[col];
            } else {
                return emptyString;
            }
        } else {
            EXCEPT("Attempt to retrieve value from result set outside of result set bounds");
        }
    } else {
        EXCEPT("Attempt to retrieve value from empty result set");
    }

    return NULL;
}


//! release the generic query result object
QuillErrCode
MYSQLDatabase::releaseQueryResult()
{
    if (result != NULL) {
        mysql_free_result(result);
        result = NULL;
    }

	return QUILL_SUCCESS;
}

//! check if the connection is ok
QuillErrCode
MYSQLDatabase::checkConnection()
{
    return mysql_ping(conn) == 0 ? QUILL_SUCCESS : QUILL_FAILURE ;
}

//! check if the connection is ok
QuillErrCode
MYSQLDatabase::resetConnection()
{

    //close and reopen connection
    if (conn) {
        mysql_close(conn);
    }
    if (mysql_real_connect(conn,host,user,pass,db,0,NULL,0)) {
        connected = true;
    } else {
        dprintf(D_ALWAYS,"error: %s\n",mysql_error(conn));
        connected = false;
    }

    //test the new connection
    if (checkConnection() == QUILL_SUCCESS) {
		dprintf(D_FULLDEBUG, "DB Connection Ok\n");
        return QUILL_SUCCESS;
    } else {
		dprintf(D_FULLDEBUG, "DB Connection BAD\n");
        return QUILL_FAILURE;
    }
}

//! get the field name at given column index from the cluster ads
const char *
MYSQLDatabase::getJobQueueClusterHorFieldName(int col) 
{
	return cluster_field_names[col];
}

//! get number of fields returned in the horizontal cluster ads
int 
MYSQLDatabase::getJobQueueClusterHorNumFields() 
{
    return clusterAdsHorRes_numFields;
}

//! get the field name at given column index from proc ads
const char *
MYSQLDatabase::getJobQueueProcHorFieldName(int col) 
{
	return proc_field_names[col];
}

//! get number of fields in the proc ad horizontal
int 
MYSQLDatabase::getJobQueueProcHorNumFields() 
{
    return procAdsHorRes_numFields;
}

//! get the field name at given column index
const char *
MYSQLDatabase::getHistoryHorFieldName(int col) 
{
	if (col >= QUILLPP_HistoryHorFieldNum) {
		dprintf(D_ALWAYS, "column index %d exceeds max column num %d in MYSQLDatabase::getHistoryHorFieldName.\n", col, QUILLPP_HistoryHorFieldNum);
		return NULL;
	} else {
		return QUILLPP_HistoryHorFields[col];
	}
}

//! get number of fields returned in result
int 
MYSQLDatabase::getHistoryHorNumFields() 
{
	return QUILLPP_HistoryHorFieldNum;
}

//! release all history results
QuillErrCode
MYSQLDatabase::releaseHistoryResults()
{
    EXCEPT("UNIMPLEMENTED: releaseHistoryResults");
	return QUILL_SUCCESS;
}

/*! get the job queue
 *
 *	\return 
 *		JOB_QUEUE_EMPTY: There is no job in the queue
 *      FAILURE_QUERY_* : error querying table *
 *		QUILL_SUCCESS: There is some job in the queue and query was successful
 *
 *		
 */
QuillErrCode
MYSQLDatabase::getJobQueueDB( int *clusterarray, int numclusters, 
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
		procAds_hor_query.sprintf(
                "SELECT cluster_id, proc_id, jobstatus, imagesize, remoteusercpu, "
                    "remotewallclocktime, remotehost, globaljobid, jobprio, args, "
                    "UNIX_TIMESTAMP(shadowbday) as shadowbday, "
                    "UNIX_TIMESTAMP(enteredcurrentstatus) as enteredcurrentstatus, "
                    "numrestarts "
                "FROM procads_horizontal "
                "WHERE scheddname=\'%s\' "
                "ORDER BY cluster_id, proc_id", scheddname);

		procAds_ver_query.sprintf(
                "SELECT cluster_id, proc_id, attr, val "
                "FROM procads_vertical WHERE scheddname=\'%s\' "
                "ORDER BY cluster_id, proc_id", scheddname);

		clusterAds_hor_query.sprintf(
                "SELECT cluster_id, owner, jobstatus, jobprio, imagesize, UNIX_TIMESTAMP(qdate) as qdate, "
                    "remoteusercpu, remotewallclocktime, cmd, args, jobuniverse "
                "FROM clusterads_horizontal "
                "WHERE scheddname=\'%s\' "
                "ORDER BY cluster_id", scheddname);

		clusterAds_ver_query.sprintf(
                "SELECT cluster_id, attr, val "
                "FROM clusterads_vertical "
                "WHERE scheddname=\'%s\' "
                "ORDER BY cluster_id", scheddname);
	} else {

        //refer to PGSQLDatabase::getJobQueueDB comments to understand the following

	    if(numclusters > 0) {
			// build up the cluster predicate
			clusterpredicate.sprintf("%s%d)", 
					" AND ( (cluster_id = ",clusterarray[0]);
			for(i=1; i < numclusters; i++) {
				clusterpredicate.sprintf_cat( 
				"%s%d) ", " OR (cluster_id = ", clusterarray[i] );
      		}

			// now build up the proc predicate string. 	
			// first decide how to open it
			 if(procarray[0] != -1) {
					procpredicate.sprintf("%s%d%s%d)", 
							" AND ( (cluster_id = ", clusterarray[0], 
							" AND proc_id = ", procarray[0]);
	 		} else {  // no proc for this entry, so only have cluster
					procpredicate.sprintf( "%s%d)", 
								" AND ( (cluster_id = ", clusterarray[0]);
	 		}
	
			// fill in the rest of the proc predicate 
	 		// note that we really want to iterate till numclusters and not 
			// numprocs because procarray has holes and clusterarray does not
			for(i=1; i < numclusters; i++) {
				if(procarray[i] != -1) {
					procpredicate.sprintf_cat( "%s%d%s%d) ", 
					" OR (cluster_id = ",clusterarray[i]," AND proc_id = ",procarray[i]);
				} else { 
					procpredicate.sprintf_cat( "%s%d) ", 
						" OR (cluster_id = ", clusterarray[i]);
				}
			} //end of for loop

			// balance predicate strings, since it needs to get
			// and-ed with the schedd name below
			clusterpredicate += " ) ";
			procpredicate += " ) ";
		} // end of numclusters > 0


		procAds_hor_query.sprintf( 
			    "SELECT cluster_id, proc_id, jobstatus, imagesize, remoteusercpu, "
                    "remotewallclocktime, remotehost, globaljobid, jobprio, args, "
                    "UNIX_TIMESTAMP(shadowbday) as shadowbday, "
                    "UNIX_TIMESTAMP(enteredcurrentstatus) as enteredcurrentstatus, numrestarts "
                "FROM procads_horizontal "
                "WHERE scheddname=\'%s\' %s "
                "ORDER BY cluster_id, proc_id", scheddname, procpredicate.Value() );

		procAds_ver_query.sprintf(
                "SELECT cluster_id, proc_id, attr, val "
                "FROM procads_vertical "
                "WHERE scheddname=\'%s\' %s "
                "ORDER BY cluster_id, proc_id", scheddname, procpredicate.Value() );

		clusterAds_hor_query.sprintf(
                "SELECT cluster_id, owner, jobstatus, jobprio, imagesize, "
                    "UNIX_TIMESTAMP(qdate) as qdate, remoteusercpu, remotewallclocktime, "
                    "cmd, args, jobuniverse "
                "FROM clusterads_horizontal "
                "WHERE scheddname=\'%s\' %s "
                "ORDER BY cluster_id", scheddname, clusterpredicate.Value());

		clusterAds_ver_query.sprintf(
                "SELECT cluster_id, attr, val "
                "FROM clusterads_vertical "
                "WHERE scheddname=\'%s\' %s "
                "ORDER BY cluster_id", scheddname, clusterpredicate.Value());	
	}

  return QUILL_SUCCESS;
}

/*! get the historical information
 *
 *	\return
 *		QUILL_SUCCESS: declare cursor succeeded 
 *		FAILURE_QUERY_*: query failed
 */
QuillErrCode
MYSQLDatabase::openCursorsHistory(SQLQuery *queryhor, 
								  SQLQuery *queryver,
								  bool longformat)

{  
    EXCEPT("UNIMPLEMENTED: openCursorsHistory");
	return QUILL_SUCCESS;
}

QuillErrCode
MYSQLDatabase::closeCursorsHistory(SQLQuery *queryhor, 
								   SQLQuery *queryver,
								   bool longformat)

{  
    EXCEPT("UNIMPLEMENTED: closeCursorsHistory");
	return QUILL_SUCCESS;
}

//! get a value retrieved from Jobs_Horizontal_History table
QuillErrCode
MYSQLDatabase::getHistoryHorValue(SQLQuery *queryhor, 
								  int row, int col, const char **value)
{
    EXCEPT("UNIMPLEMENTED: getHistoryHorValue");
	return QUILL_SUCCESS;
}

//! get a value retrieved from Jobs_Vertical_History table
QuillErrCode
MYSQLDatabase::getHistoryVerValue(SQLQuery *queryver, 
								  int row, int col, const char **value)
{
    EXCEPT("UNIMPLEMENTED: getHistoryVerValue");
	return QUILL_SUCCESS;
}

//! get a value retrieved from ProcAds_Hor table
const char*
MYSQLDatabase::getJobQueueProcAds_HorValue(int row, int col)
{
    EXCEPT("UNIMPLEMENTED: getJobQueueProcAds_HorValue");
	return NULL;
}

//! get a value retrieved from ProcAds_Ver table
const char*
MYSQLDatabase::getJobQueueProcAds_VerValue(int row, int col)
{
    EXCEPT("UNIMPLEMENTED: getJobQueueProcAds_VerValue");
	return NULL;
}

//! get a value retrieved from ClusterAds_Hor table
const char*
MYSQLDatabase::getJobQueueClusterAds_HorValue(int row, int col)
{
    EXCEPT("UNIMPLEMENTED: getJobQueueClusterAds_HorValue");
	return NULL;
}

//! get a value retrieved from ClusterAds_Ver table
const char*
MYSQLDatabase::getJobQueueClusterAds_VerValue(int row, int col)
{
    EXCEPT("UNIMPLEMENTED: getJobQueueClusterAds_VerValue");
	return NULL;
}

//! release the result for job queue database
QuillErrCode
MYSQLDatabase::releaseJobQueueResults()
{
	if (procAdsHorRes != NULL) {
        mysql_free_result(procAdsHorRes);
		procAdsHorRes = NULL;
	}
	if (procAdsVerRes != NULL) {
        mysql_free_result(procAdsVerRes);
		procAdsVerRes = NULL;
	}
	if (clusterAdsHorRes != NULL) {
        mysql_free_result(clusterAdsHorRes);
		clusterAdsHorRes = NULL;
	}
	if (clusterAdsVerRes != NULL) {
        mysql_free_result(clusterAdsVerRes);
		clusterAdsVerRes = NULL;
	}

	return QUILL_SUCCESS;
}

//! get a DBMS error message
const char*
MYSQLDatabase::getDBError()
{
    const char* err = NULL;
    if (conn) {
        err = mysql_error(conn);
    }
    return err;
}

QuillErrCode MYSQLDatabase::execCommandWithBind(const char* sql, 
												int bnd_cnt,
												const char** val_arr,
												QuillAttrDataType *typ_arr) 
{
    //this function is only used and implemented by oracle database
    EXCEPT("PROGRAMMER ERROR: execCommandWithBind should never be called on mysql");
    return QUILL_FAILURE;
}

QuillErrCode MYSQLDatabase::execQueryWithBind(const char* sql, 
												int bnd_cnt,
												const char** val_arr,
												QuillAttrDataType *typ_arr,
												int &num_result) 
{
    //this function is only used and implemented by oracle database
	EXCEPT("PROGRAMMER ERROR: execQueryWithBind should never be called on mysql");
	return QUILL_FAILURE;
}

