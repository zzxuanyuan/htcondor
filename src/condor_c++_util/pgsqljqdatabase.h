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

#ifndef _PGSQLJQDATABASE_H_
#define _PGSQLJQDATABASE_H_

#include "condor_common.h"
#include "libpq-fe.h"
#include "sqlquery.h"
#include "jobqueuedatabase.h"
#include "quill_enums.h"

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

//! PGSQLJQDataabse: JobQueueDatabase for PostgreSQL
//
class PGSQLJQDatabase : public JobQueueDatabase
{
public:
	
	PGSQLJQDatabase();
	~PGSQLJQDatabase();

	const char*          getHistoryHorFieldName(int col);
	const int            getHistoryHorNumFields();
	QuillErrCode		 releaseHistoryResults();		

	QuillErrCode		 queryHistoryDB(SQLQuery *, SQLQuery *, 
										bool, int&, int&);
	QuillErrCode         releaseJobQueueResults();

	QuillErrCode		 getJobQueueDB(int *, int, int *, int, char *, bool, 
									   int&, int&, int&, int&);
	const char*	         getJobQueueProcAds_StrValue(int row, int col);
	const char*	         getJobQueueProcAds_NumValue(int row, int col);
	const char*	         getJobQueueClusterAds_StrValue(int row, int col);
	const char*	         getJobQueueClusterAds_NumValue(int row, int col);
	const char*          getHistoryHorValue(int row, int col);
	const char*          getHistoryVerValue(int row, int col);
	  
	int 				 getDatabaseVersion();
private:
		// only for history tables retrieval
	PGresult             *historyHorRes;
	PGresult             *historyVerRes;

		// only for job queue tables retrieval
	PGresult	         *procAdsStrRes;	//!< result for ProcAds_Str table
	PGresult	         *procAdsNumRes;	//!< result for ProcAds_Num table
	PGresult	         *clusterAdsStrRes;//!< result for ClusterAds_Str table
	PGresult	         *clusterAdsNumRes;//!< result for ClusterAds_num table
};

#endif /* _PGSQLJQDATABSE_H_ */

