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

#ifndef _PGSQLDATABASE_H_
#define  _PGSQLDATABASE_H_

#include <libpq-fe.h>
#include "sqlquery.h"
#include "jobqueuedatabase.h"


//! PGSQLDataabse: JobQueueDatabase for PostgreSQL
//
class PGSQLDatabase : public JobQueueDatabase
{
public:
	
	PGSQLDatabase();
	PGSQLDatabase(const char* connect);
	~PGSQLDatabase();

	int			connectDB();
	int			connectDB(const char* connect);
	int			disconnectDB();

		// General DB processing methods
	int			beginTransaction();
	int 		commitTransaction();
	int 		rollbackTransaction();

	int 	 	execCommand(const char* sql);

	int 	 	execQuery(const char* sql);
	int 	 	execQuery(const char* sql, PGresult*& result);
	const char*	getValue(int row, int col);
	const char* getHistoryHorFieldName(int col);
	const int   getHistoryHorNumFields();
	int		    releaseHistoryResults();		

	char*		getDBError();

	int		    sendBulkData(char* data);
	int		    sendBulkDataEnd();

	int		    queryHistoryDB(SQLQuery *, SQLQuery *, 
							   bool, int&, int&);
	int         releaseJobQueueDB();

	int         releaseQueryResult();

		// Job Queue DB processing methods
	int		    getJobQueueDB(int, int, char *, bool, 
							  int&, int&, int&, int&);
	const char*	getJobQueueProcAds_StrValue(int row, int col);
	const char*	getJobQueueProcAds_NumValue(int row, int col);
	const char*	getJobQueueClusterAds_StrValue(int row, int col);
	const char*	getJobQueueClusterAds_NumValue(int row, int col);
	const char* getHistoryHorValue(int row, int col);
	const char* getHistoryVerValue(int row, int col);
	  
private:
	PGconn		*connection;		//!< connection object
	PGresult	*queryRes; 	//!< result for general query
	PGresult    *historyHorRes;
	PGresult    *historyVerRes;
		// only for Job Queue DB retrieval
	PGresult	*procAdsStrRes;	//!< result for ProcAds_Str relation
	PGresult	*procAdsNumRes;	//!< result for ProcAds_Num relation
	PGresult	*clusterAdsStrRes;//!< result for ClusterAds_Str relation
	PGresult	*clusterAdsNumRes;//!< result for ClusterAds_num relation
};

#endif /* _PGSQLDATABSE_H_ */

#endif /* _POSTGRESQL_DBMS_ */
