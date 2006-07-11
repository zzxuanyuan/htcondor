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
#ifndef _TTMANAGER_H_
#define _TTMANAGER_H_

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"

#include "jobqueuedbmanager.h"
#include "quill_enums.h"

#define MAXLOGNUM 7
#define MAXLOGPATHLEN 100

//! TTManager
/*! \brief this class orchestrates TT's log writing code
 */
class TTManager : public Service
{
 public:
		//! constructor	
	TTManager();
	
		//! destructor	
	~TTManager();
	
		//! read all config options from condor_config file. 
		//! Also create class members instead of doing it in the constructor
	void    config(bool reconfig = false);

		//! register all timer handlers
	void	registerTimers();
	
		//! timer handler for maintaining job queue and sending SCHEDD_AD to collector
	void	pollingTime();	
 private:

		// top level maintain function
	void     maintain();

		// general event log maintain function (non-transactional data)
	QuillErrCode event_maintain();
		// xml log maintain function
	QuillErrCode xml_maintain();

		// check and throw away big files
	void    checkAndThrowBigFiles();

		//! create the QUILL_AD that's sent to the collector
	void     createQuillAd(void);

		//! update the QUILL_AD's dynamic attributes
	void     updateQuillAd(void);

	QuillErrCode insertMachines(AttrList *ad);
	QuillErrCode insertEvents(AttrList *ad);
	QuillErrCode insertFiles(AttrList *ad);
	QuillErrCode insertFileusages(AttrList *ad);
	QuillErrCode insertHistoryJob(AttrList *ad);
	QuillErrCode insertBasic(AttrList *ad, char *tableName);
	QuillErrCode updateBasic(AttrList *info, AttrList *condition, char *tableName);
	QuillErrCode insertScheddAd(AttrList *ad);
	QuillErrCode insertMasterAd(AttrList *ad);
	QuillErrCode insertNegotiatorAd(AttrList *ad);

	char    sqlLogList[MAXLOGNUM][MAXLOGPATHLEN];
	char    sqlLogCopyList[MAXLOGNUM+1][MAXLOGPATHLEN]; // 1 more file for "thrown" file
	int     numLogs;
        
	int		pollingTimeId;			//!< timer handler id of pollingTime function
	int		pollingPeriod;			//!< polling time period in seconds

	CollectorList   *collectors;
	ClassAd         *ad;

	Database* DBObj;
	JobQueueDBManager jqDBManager;
	const char*	jobQueueDBConn;  		//!< DB connection string
};

#endif /* _TTMANAGER_H_ */
