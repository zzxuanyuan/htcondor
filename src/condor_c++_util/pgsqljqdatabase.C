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

#include "pgsqldatabase.h"
#include "pgsqljqdatabase.h"

//! constructor
PGSQLJQDatabase::PGSQLJQDatabase()
{
  procAdsStrRes = procAdsNumRes = clusterAdsStrRes = clusterAdsNumRes = 
	  historyHorRes = historyVerRes = NULL;  

  this->DBObj = NULL;
}

//! destructor
PGSQLJQDatabase::~PGSQLJQDatabase()
{
	procAdsStrRes = procAdsNumRes = clusterAdsStrRes = clusterAdsNumRes = 
		historyHorRes = historyVerRes = NULL;  
    this->DBObj = NULL;
}

//! get the field name at given column index
const char *
PGSQLJQDatabase::getHistoryHorFieldName(int col) 
{
  return PQfname(historyHorRes, col);
}

//! get number of fields returned in result
const int 
PGSQLJQDatabase::getHistoryHorNumFields() 
{
  return PQnfields(historyHorRes);
}

//! release all history results
QuillErrCode
PGSQLJQDatabase::releaseHistoryResults()
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
PGSQLJQDatabase::getJobQueueDB(int *clusterarray, int numclusters, 
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

  if (DBObj == NULL) {
	  dprintf(D_ALWAYS, "Database connection object not set\n");
	  return FAILURE;
  }

  procAds_str_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  procAds_num_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * sizeof(char));
  clusterAds_str_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * 
										 sizeof(char));
  clusterAds_num_query = (char *) malloc(MAX_FIXED_SQL_STR_LENGTH * 
										 sizeof(char));

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
    temppredicate = (char *) malloc(1024 * sizeof(char));
    strcpy(temppredicate, "  ");

    if(numclusters > 0) {
      sprintf(clusterpredicate, "%s%d)", " WHERE (cid = ", clusterarray[0]);
      for(i=1; i < numclusters; i++) {
	 sprintf(temppredicate, "%s%d) ", " OR (cid = ", clusterarray[i]);
	 strcat(clusterpredicate, temppredicate); 	 
      }

	 if(procarray[0] != -1) {
            sprintf(procpredicate, "%s%d%s%d)", " WHERE (cid = ", clusterarray[0], " AND pid = ", procarray[0]);
	 }
	 else {
            sprintf(procpredicate, "%s%d)", " WHERE (cid = ", clusterarray[0]);
	 }
	 
	 // note that we really want to iterate till numclusters and not numprocs 
	 // because procarray has holes and clusterarray does not
         for(i=1; i < numclusters; i++) {
	    if(procarray[i] != -1) {
	       sprintf(temppredicate, "%s%d%s%d) ", " OR (cid = ", clusterarray[i], " AND pid = ", procarray[i]);
	       procpredicate = strcat(procpredicate, temppredicate); 	 
            }
	    else {
	       sprintf(temppredicate, "%s%d) ", " OR (cid = ", clusterarray[i]);
	       procpredicate = strcat(procpredicate, temppredicate); 	 
            }
	 }
    }

    sprintf(procAds_str_query, "%s %s %s", 
	    "SELECT cid, pid, attr, val FROM ProcAds_Str", 
	    procpredicate,
	    "ORDER BY cid, pid;");
    sprintf(procAds_num_query, "%s %s %s", 
	    "SELECT cid, pid, attr, val FROM ProcAds_Num",
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
    free(temppredicate);
  }

  /*dprintf(D_ALWAYS, "clusterAds_str_query = %s\n", clusterAds_str_query);
  dprintf(D_ALWAYS, "clusterAds_num_query = %s\n", clusterAds_num_query);
  dprintf(D_ALWAYS, "procAds_str_query = %s\n", procAds_str_query);
  dprintf(D_ALWAYS, "procAds_num_query = %s\n", procAds_num_query);*/

	  // Query against ProcAds_Str Table
  if ((st = DBObj->execQuery(procAds_str_query, procAdsStrRes, procAdsStrRes_num)) == FAILURE) {
	  return FAILURE_QUERY_PROCADS_STR;
  }
  
	  // Query against ProcAds_Num Table
  if ((st = DBObj->execQuery(procAds_num_query, procAdsNumRes, procAdsNumRes_num)) == FAILURE) {
	  return FAILURE_QUERY_PROCADS_NUM;
  }
  
	  // Query against ClusterAds_Str Table
  if ((st = DBObj->execQuery(clusterAds_str_query, clusterAdsStrRes, clusterAdsStrRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_STR;
  } 
	  // Query against ClusterAds_Num Table
  if ((st = DBObj->execQuery(clusterAds_num_query, clusterAdsNumRes, clusterAdsNumRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_NUM;
  }
  
  free(procAds_str_query);
  free(procAds_num_query);
  free(clusterAds_str_query);
  free(clusterAds_num_query);

  if (clusterAdsNumRes_num == 0 && clusterAdsStrRes_num == 0) {
    return JOB_QUEUE_EMPTY;
  }

  return SUCCESS;
}

/*! get the historical information
 *
 *	\return
 *		HISTORY_EMPTY: There is no Job in history
 *		SUCCESS: history is not empty and query succeeded
 *		FAILURE_QUERY_*: query failed
 */
QuillErrCode
PGSQLJQDatabase::queryHistoryDB(SQLQuery *queryhor, 
			      SQLQuery *queryver, 
			      bool longformat, 
			      int& historyads_hor_num, 
			      int& historyads_ver_num)
{  
	QuillErrCode st;
	if (DBObj == NULL) {
		dprintf(D_ALWAYS, "Database connection object not set\n");
		return FAILURE;
	}

	if ((st = DBObj->execQuery(queryhor->getQuery(), historyHorRes, historyads_hor_num)) == FAILURE) {
		return FAILURE_QUERY_HISTORYADS_HOR;
	}
	if (longformat && (st = DBObj->execQuery(queryver->getQuery(), historyVerRes, historyads_ver_num)) == FAILURE) {
		return FAILURE_QUERY_HISTORYADS_VER;
	}
  
	if (historyads_hor_num == 0) {
		return HISTORY_EMPTY;
	}
	
	return SUCCESS;
}

//! get a value retrieved from History_Horizontal table
const char*
PGSQLJQDatabase::getHistoryHorValue(int row, int col)
{
	return PQgetvalue(historyHorRes, row, col);
}

//! get a value retrieved from History_Vertical table
const char*
PGSQLJQDatabase::getHistoryVerValue(int row, int col)
{
	return PQgetvalue(historyVerRes, row, col);
}

//! get a value retrieved from ProcAds_Str table
const char*
PGSQLJQDatabase::getJobQueueProcAds_StrValue(int row, int col)
{
	return PQgetvalue(procAdsStrRes, row, col);
}

//! get a value retrieved from ProcAds_Num table
const char*
PGSQLJQDatabase::getJobQueueProcAds_NumValue(int row, int col)
{
	return PQgetvalue(procAdsNumRes, row, col);
}

//! get a value retrieved from ClusterAds_Str table
const char*
PGSQLJQDatabase::getJobQueueClusterAds_StrValue(int row, int col)
{
	return PQgetvalue(clusterAdsStrRes, row, col);
}

//! get a value retrieved from ClusterAds_Num table
const char*
PGSQLJQDatabase::getJobQueueClusterAds_NumValue(int row, int col)
{
	return PQgetvalue(clusterAdsNumRes, row, col);
}

//! release the result for job queue database
QuillErrCode
PGSQLJQDatabase::releaseJobQueueResults()
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

	return SUCCESS;
}	


//! get the server version number, 
//! -1 if connection is invalid
int 
PGSQLJQDatabase::getDatabaseVersion() 
{
	int pg_version_number = 0;   
	pg_version_number = PQserverVersion(connection);
	if(pg_version_number > 0) {
		return pg_version_number;
	}
	else {
		return -1;
	}
}

//! put an end flag for bulk loading

