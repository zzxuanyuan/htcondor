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

#include "jobqueuedatabase.h"
#include </scratch/akini/pgsql/include/libpq-fe.h>

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
	int			releaseQueryResult();

	char*		getDBError();

	int			sendBulkyData(char* data);
	int			sendBulkyDataEnd();

		// Job Queue DB processing methods
	int			getJobQueueDB(int&, int&, int&, int&);
	const char*	getJobQueueProcAds_StrValue(int row, int col);
	const char*	getJobQueueProcAds_NumValue(int row, int col);
	const char*	getJobQueueClusterAds_StrValue(int row, int col);
	const char*	getJobQueueClusterAds_NumValue(int row, int col);
	int			releaseJobQueueDB();		

private:
	PGconn		*connection;		//!< connection object
	PGresult	*queryResult; 	//!< result for general query

		// only for Job Queue DB retrieval
	PGresult	*procAds_Str;	//!< result for ProcAds_Str relation
	PGresult	*procAds_Num;	//!< result for ProcAds_Num relation
	PGresult	*clusterAds_Str;//!< result for ClusterAds_Str relation
	PGresult	*clusterAds_Num;//!< result for ClusterAds_num relation
};

#endif /* _PGSQLDATABSE_H_ */

#endif /* _POSTGRESQL_DBMS_ */
