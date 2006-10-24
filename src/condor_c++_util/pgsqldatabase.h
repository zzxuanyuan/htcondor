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

#ifndef _PGSQLDATABASE_H_
#define _PGSQLDATABASE_H_

#include "condor_common.h"
#include "libpq-fe.h"
#include "sqlquery.h"
#include "jobqueuedatabase.h"
#include "quill_enums.h"

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

//! PGSQLDataabse: Job Queue Database for PostgreSQL
//
class PGSQLDatabase : public JobQueueDatabase
{
public:
	
	PGSQLDatabase(const char* connect);
	~PGSQLDatabase();

		// connection method
	QuillErrCode         connectDB();
	QuillErrCode		 disconnectDB();
    QuillErrCode         checkConnection();
	QuillErrCode         resetConnection();

		// General DB processing methods
	QuillErrCode		 beginTransaction();
	QuillErrCode 		 commitTransaction();
	QuillErrCode 		 rollbackTransaction();

		// update methods
	QuillErrCode 	 	 execCommand(const char* sql, 
									 int &num_result);
	QuillErrCode 	 	 execCommand(const char* sql);

		// query methods
	QuillErrCode 	 	 execQuery(const char* sql);
	QuillErrCode 	 	 execQuery(const char* sql, PGresult *&result, 
								   int &num_result);
	QuillErrCode         execQuery(const char* sql,
								   int &num_result);

		//QuillErrCode   		 fetchNext();
	const char*	         getValue(int row, int col);
		//int  				 getIntValue(int col);

	QuillErrCode         releaseQueryResult();

		//
		// Job Queue DB processing methods
		//
	//! get the queue from the database
	QuillErrCode        getJobQueueDB(int *, int, int *, int,  bool,
									  const char *, int&, int&, int&, 
									  int&);
	
		//! get a value retrieved from ProcAds_Hor table
	const char*         getJobQueueProcAds_HorValue(int row, 
													int col);

	//! get a value retrieved from ProcAds_Ver table
	const char*         getJobQueueProcAds_VerValue(int row, 
													int col);

		//! get a value retrieved from ClusterAds_Hor table
	const char*         getJobQueueClusterAds_HorValue(int row, 
													   int col);
	//! get a value retrieved from ClusterAds_Ver table
	const char*         getJobQueueClusterAds_VerValue(int row, 
													   int col);

	const char*         getJobQueueClusterHorFieldName(int col);

	const int           getJobQueueClusterHorNumFields();

	const char*         getJobQueueProcHorFieldName(int col);

	const int           getJobQueueProcHorNumFields();

	//! get the history from the database
	QuillErrCode        queryHistoryDB(SQLQuery *,SQLQuery *,bool,
									   int&,int&);

	const char*         getHistoryHorValue(int row, int col);

	const char*         getHistoryVerValue(int row, int col);

	QuillErrCode		releaseHistoryResults();		

	QuillErrCode        releaseJobQueueResults();

	const char*         getHistoryHorFieldName(int col);

	const int           getHistoryHorNumFields();

	char*		        getDBError();
private:
	PGconn		         *connection;	//!< connection object
	PGresult	         *queryRes; 	//!< result for general query
	char				 *con_str;		//!< connection string

		// only for history tables retrieval
	PGresult             *historyHorRes;
	PGresult             *historyVerRes;

		// only for job queue tables retrieval
	PGresult	         *procAdsHorRes;	//!< result for ProcAds_Str table
	PGresult	         *procAdsVerRes;	//!< result for ProcAds_Num table
	PGresult	         *clusterAdsHorRes;//!< result for ClusterAds_Str table
	PGresult	         *clusterAdsVerRes;//!< result for ClusterAds_num table
};

#endif /* _PGSQLDATABSE_H_ */

