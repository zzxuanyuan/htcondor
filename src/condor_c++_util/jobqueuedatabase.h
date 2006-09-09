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
#ifndef _JOBQUEUEDATABASE_H_
#define _JOBQUEUEDATABASE_H_

#include "condor_common.h"
#include "sqlquery.h"
#include "quill_enums.h"

class Database;

//! JobQueueDatabase
/*! It provides interfaces to talk to DBMS
 */
class JobQueueDatabase
{
public:
	//! destructor
	virtual ~JobQueueDatabase() {};

	// set the database object.
    void        setDB(Database *dbObj) {this->DBObj = dbObj;};

		//
		// Job Queue DB processing methods
		//
	//! get the queue from the database
	virtual QuillErrCode        getJobQueueDB(int *, int, int *, int,  bool,
											  const char *, int&, int&, int&, int&) = 0;

	//! get a value retrieved from ProcAds_Hor table
	virtual const char*         getJobQueueProcAds_HorValue(int row, 
															int col) = 0;
	//! get a value retrieved from ProcAds_Ver table
	virtual const char*         getJobQueueProcAds_VerValue(int row, 
															int col) = 0;
	//! get a value retrieved from ClusterAds_Hor table
	virtual const char*         getJobQueueClusterAds_HorValue(int row, 
															   int col) = 0;
	//! get a value retrieved from ClusterAds_Ver table
	virtual const char*         getJobQueueClusterAds_VerValue(int row, 
															   int col) = 0;
	virtual const char*         getJobQueueClusterHorFieldName(int col) = 0;
	virtual const int           getJobQueueClusterHorNumFields() = 0;

	virtual const char*         getJobQueueProcHorFieldName(int col) = 0;
	virtual const int           getJobQueueProcHorNumFields() = 0;

	//! get the history from the database
	virtual QuillErrCode        queryHistoryDB(SQLQuery *,SQLQuery *,bool,int&,int&) = 0;

	virtual const char*         getHistoryHorValue(int row, int col) = 0;
	virtual const char*         getHistoryVerValue(int row, int col) = 0;

	virtual QuillErrCode		releaseHistoryResults() = 0;		
	virtual QuillErrCode        releaseJobQueueResults() = 0;

	virtual const char*         getHistoryHorFieldName(int col) = 0;
	virtual const int           getHistoryHorNumFields() = 0;
protected:
    Database *DBObj;
};

#endif
