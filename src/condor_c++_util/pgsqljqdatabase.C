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

/* NOTE - we project out a few column names, so this only has the results
   AFTER the select  - ie the "schedd name" field from the database is not
   listed here */

const char *proc_field_names [] = { "Cluster", "Proc", "JobStatus", "ImageSize", "RemoteUserCpu", "RemoteWallClockTime", "RemoteHost", "GlobalJobId", "JobPrio", "Args" };

const char *cluster_field_names [] = { "Cluster", "Owner", "JobStatus", "JobPrio", "ImageSize", "QDate", "RemoteUserCpu", "RemoteWallClockTime", "Cmd", "Args" };


//! constructor
PGSQLJQDatabase::PGSQLJQDatabase()
{
  procAdsHorRes = procAdsVerRes = clusterAdsHorRes = clusterAdsVerRes = 
	  historyHorRes = historyVerRes = NULL;  

  this->DBObj = NULL;
}

//! destructor
PGSQLJQDatabase::~PGSQLJQDatabase()
{
  procAdsHorRes = procAdsVerRes = clusterAdsHorRes = clusterAdsVerRes = 
	  historyHorRes = historyVerRes = NULL;  
    this->DBObj = NULL;
}

//! get the field name at given column index from the cluster ads
const char *
PGSQLJQDatabase::getJobQueueClusterHorFieldName(int col) 
{
	return cluster_field_names[col];
  //return PQfname(clusterAdsHorRes, col);
}

//! get number of fields returned in the horizontal cluster ads
const int 
PGSQLJQDatabase::getJobQueueClusterHorNumFields() 
{
  return PQnfields(clusterAdsHorRes);
}

//! get the field name at given column index from proc ads
const char *
PGSQLJQDatabase::getJobQueueProcHorFieldName(int col) 
{
  //return PQfname(procAdsHorRes, col);
	return proc_field_names[col];
}

//! get number of fields in the proc ad horizontal
const int 
PGSQLJQDatabase::getJobQueueProcHorNumFields() 
{
  return PQnfields(procAdsHorRes);
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
PGSQLJQDatabase::getJobQueueDB( int *clusterarray, int numclusters, 
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


  if (DBObj == NULL) {
	  dprintf(D_ALWAYS, "Database connection object not set\n");
	  return FAILURE;
  }


	if(isfullscan) {
		procAds_hor_query.sprintf("SELECT cluster, proc, jobstatus, imagesize, remoteusercpu, remotewallclocktime, remotehost, globaljobid, jobprio,  args  FROM procads_horizontal WHERE scheddname=\'%s\' ORDER BY cluster, proc;", scheddname);
		procAds_ver_query.sprintf("SELECT cluster, proc, attr, val FROM procads_vertical WHERE scheddname=\'%s\' ORDER BY cluster, proc;", scheddname);

		clusterAds_hor_query.sprintf("SELECT cluster, owner, jobstatus, jobprio, imagesize, extract(epoch from qdate) as qdate, remoteusercpu, remotewallclocktime, cmd, args  FROM clusterads_horizontal WHERE scheddname=\'%s\' ORDER BY cluster;", scheddname);

		clusterAds_ver_query.sprintf("SELECT cluster, attr, val FROM clusterads_vertical WHERE scheddname=\'%s\' ORDER BY cluster;", scheddname);
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
			"SELECT cluster, proc, jobstatus, imagesize, remoteusercpu, remotewallclocktime, remotehost, globaljobid, jobprio,  args  FROM procads_horizontal WHERE scheddname=\'%s\' %s ORDER BY cluster, proc;", scheddname, procpredicate.Value() );

		procAds_ver_query.sprintf(
	"SELECT cluster, proc, attr, val FROM procads_vertical WHERE scheddname=\'%s\' %s ORDER BY cluster, proc;", 
			scheddname, procpredicate.Value() );

		clusterAds_hor_query.sprintf(
			"SELECT cluster, owner, jobstatus, jobprio, imagesize, qdate, remoteusercpu, remotewallclocktime, cmd, args  FROM clusterads_horizontal WHERE scheddname=\'%s\' %s ORDER BY cluster;", scheddname, clusterpredicate.Value());

		clusterAds_ver_query.sprintf(
		"SELECT cluster, attr, val FROM clusterads_vertical WHERE scheddname=\'%s\' %s ORDER BY cluster;", scheddname, clusterpredicate.Value());
	
	}

	/*dprintf(D_ALWAYS, "clusterAds_hor_query = %s\n", clusterAds_hor_query.Value());
	dprintf(D_ALWAYS, "clusterAds_ver_query = %s\n", clusterAds_ver_query.Value());
	dprintf(D_ALWAYS, "procAds_hor_query = %s\n", procAds_hor_query.Value());
	dprintf(D_ALWAYS, "procAds_ver_query = %s\n", procAds_ver_query.Value()); */

	  // Query against ClusterAds_Hor Table
  if ((st = DBObj->execQuery(clusterAds_hor_query.Value(), clusterAdsHorRes, 
					clusterAdsHorRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_NUM;
  }
	  // Query against ClusterAds_Ver Table
  if ((st = DBObj->execQuery(clusterAds_ver_query.Value(), clusterAdsVerRes, 
					clusterAdsVerRes_num)) == FAILURE) {
		// FIXME to return something other than clusterads_num!
	  return FAILURE_QUERY_CLUSTERADS_NUM;
  }
	  // Query against procAds_Hor Table
  if ((st = DBObj->execQuery(procAds_hor_query.Value(), procAdsHorRes, 
									procAdsHorRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_NUM;
  }
	  // Query against procAds_ver Table
  if ((st = DBObj->execQuery(procAds_ver_query.Value(), procAdsVerRes, 
									procAdsVerRes_num)) == FAILURE) {
	  return FAILURE_QUERY_CLUSTERADS_NUM;
  }
  
  if (clusterAdsVerRes_num == 0 && clusterAdsHorRes_num == 0) {
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

//! get a value retrieved from ProcAds_Hor table
const char*
PGSQLJQDatabase::getJobQueueProcAds_HorValue(int row, int col)
{
	return PQgetvalue(procAdsHorRes, row, col);
}

//! get a value retrieved from ProcAds_Ver table
const char*
PGSQLJQDatabase::getJobQueueProcAds_VerValue(int row, int col)
{
	return PQgetvalue(procAdsVerRes, row, col);
}

//! get a value retrieved from ClusterAds_Hor table
const char*
PGSQLJQDatabase::getJobQueueClusterAds_HorValue(int row, int col)
{
	return PQgetvalue(clusterAdsHorRes, row, col);
}

//! get a value retrieved from ClusterAds_Ver table
const char*
PGSQLJQDatabase::getJobQueueClusterAds_VerValue(int row, int col)
{
	return PQgetvalue(clusterAdsVerRes, row, col);
}

//! release the result for job queue database
QuillErrCode
PGSQLJQDatabase::releaseJobQueueResults()
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


//! put an end flag for bulk loading

