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
#include "condor_attributes.h"
#include "classad_merge.h"

#include "pgsqldatabase.h"
#include "pgsqljqdatabase.h"
#include "jobqueuesnapshot.h"

//! constructor
JobQueueSnapshot::JobQueueSnapshot(const char* dbcon_str)
{
    DBObj = new PGSQLDatabase(dbcon_str);
	jqDB = new PGSQLJQDatabase();
    jqDB->setDB(DBObj);
	curClusterAd = NULL;
	curClusterId = NULL;
	curProcId = NULL;

}

//! destructor
JobQueueSnapshot::~JobQueueSnapshot()
{
	release();

	if (jqDB != NULL) {
		delete(jqDB);
	}
	jqDB = NULL;

	if (DBObj != NULL) {
		delete(DBObj);
	}
	DBObj = NULL;
}

//! prepare iteration of Job Ads in the job queue database
/*
	Return:
		FAILURE: Error
		JOB_QUEUE_EMPTY: No Result
		SUCCESS: Success
*/
QuillErrCode 
JobQueueSnapshot::startIterateAllClassAds(int *clusterarray,
					  int numclusters, 
					  int *procarray, 
					  int numprocs,
					  char *schedd, 
					  bool isfullscan)
{
	QuillErrCode st;
		// initialize index variables

	cur_procads_hor_index = cur_procads_ver_index =
	cur_clusterads_hor_index = cur_clusterads_ver_index = 0;

	procads_hor_num = procads_ver_num = 
	clusterads_hor_num = clusterads_ver_num = 0;
	
	if(DBObj->connectDB() == FAILURE) {
		return FAILURE;
	}

	if(DBObj->beginTransaction() == FAILURE) {
		printf("Error while querying the database: unable to start new transaction");
		return FAILURE;
	}

	st = jqDB->getJobQueueDB(clusterarray, 
				 numclusters,
				 procarray, 
				 numprocs,
				 isfullscan,
				 schedd,
				 procads_hor_num, 
				 procads_ver_num,
				 clusterads_hor_num, 
				 clusterads_ver_num); // this retriesves DB
	
	if(DBObj->commitTransaction() == FAILURE) {
		printf("Error while querying the database: unable to commit transaction");
		return FAILURE;
	}

	if (st == JOB_QUEUE_EMPTY) {// There is no live jobs
		return JOB_QUEUE_EMPTY;
	}

	if (st != SUCCESS) {// Got some error
		return FAILURE;
	}

	if (getNextClusterAd(curClusterId, curClusterAd) == DONE_CLUSTERADS_CURSOR) {
		return JOB_QUEUE_EMPTY;
	}

	return SUCCESS; // Success
}

//! iterate one by one
/*
	Return:
		 SUCCESS
		 FAILURE
		 DONE_CLUSTERADS_CURSOR
*/
QuillErrCode
JobQueueSnapshot::getNextClusterAd(const char*& cluster_id, ClassAd*& ad)
{
	const char	*cid, *attr, *val;

	if (cur_clusterads_hor_index >= clusterads_hor_num) {
		return DONE_CLUSTERADS_CURSOR;
	}
	cid = jqDB->getJobQueueClusterAds_HorValue(
			cur_clusterads_hor_index, 0); // cid
	if (cid == NULL) {
		return DONE_CLUSTERADS_CURSOR;
	}

		//cluster_id could be null if we're entering for the first time
		//in this cas, we set it to the value obtained from the row above
		//same goes for the case where cluster_id is not equal to cid - 
		//this case comes up when we get a new cluster ad
	if (cluster_id == NULL || strcmp(cluster_id, cid) != 0) {
		cluster_id = (char*)cid;
		curProcId = NULL;
	}
		//FAILURE case as each time we consume all attributes of the ad
		//so getting a cid which is equal to cluster_id is bizarre
	else {
		return FAILURE;
	}

		//
		// build a Next ClusterAd
		//

	ad = new ClassAd();

		// for ClusterAds vertical table
	while(cur_clusterads_ver_index < clusterads_ver_num) {
		cid = jqDB->getJobQueueClusterAds_VerValue(
				cur_clusterads_ver_index, 0); // cid

		if (cid == NULL || strcmp(cluster_id, cid) != 0) {
			break;
		}
		attr = jqDB->getJobQueueClusterAds_VerValue(
				cur_clusterads_ver_index, 1); // attr
		val = jqDB->getJobQueueClusterAds_VerValue(
				cur_clusterads_ver_index++, 2); // val

		char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
		sprintf(expr, "%s = %s", attr, val);
			// add an attribute with a value into ClassAd
		ad->Insert(expr);
		free(expr);
	};



	int numfields = jqDB->getJobQueueClusterHorNumFields();
	
	for(int i = 1; i < numfields; i++) {

		attr = jqDB->getJobQueueClusterHorFieldName(i);
		val = jqDB->getJobQueueClusterAds_HorValue(
				cur_clusterads_hor_index, i); // val
		char* expr = (char*)malloc(strlen(attr) + strlen(val) + 6);
		sprintf(expr, "%s = %s", attr, val);
			// add an attribute with a value into ClassAd
		ad->Insert(expr);
		free(expr);
	}
	cur_clusterads_hor_index++;

	return SUCCESS;
}

/*
	Return:
		DONE_PROCADS_CURSOR
		DONE_PROCADS_CUR_CLUSTERAD
		SUCCESS
		FAILURE
*/
QuillErrCode
JobQueueSnapshot::getNextProcAd(ClassAd*& ad)
{
	const char *cid = NULL, *pid = NULL, *attr, *val;

	if ((cur_procads_ver_index >= procads_ver_num) &&
		(cur_procads_hor_index >= procads_hor_num)) {
		ad = NULL;
		return DONE_PROCADS_CURSOR;
	}
	else {
		ad = new ClassAd();
	}

		//the below two while loops is to iterate over 
		//the dummy rows where cid == 0

	while(cur_procads_ver_index < procads_ver_num) {
		// Current ProcId Setting
		cid = jqDB->getJobQueueProcAds_VerValue(
			cur_procads_ver_index, 0); // cid

		if (strcmp(cid, "0") != 0) {
			break;
		}
		
		++cur_procads_ver_index;
	};

	while(cur_procads_hor_index < procads_hor_num) {
		// Current ProcId Setting
		cid = jqDB->getJobQueueProcAds_HorValue(
			cur_procads_hor_index, 0); // cid

		if (strcmp(cid, "0") != 0) {
			break;
		}

		++cur_procads_hor_index;
	};

		/* if cid is null or cid is not equal to the 
		   current cluster id, return */
	if (!cid || strcmp(cid, curClusterId) != 0) {
		delete ad;
		ad = NULL;
		return DONE_PROCADS_CUR_CLUSTERAD;
	}

		/* it's possible that we are at the end of the cursor because of the 
		   above loop that increments cur_procads_hor_index. If so, then 
		   return DONE_PROCADS_CURSOR. Otherwise we try to get the pid from 
		   the cursor that still has rows in it.

		*/
	if (cur_procads_hor_index < procads_hor_num) {
			// pid sits at attribute index 1
		pid = jqDB->getJobQueueProcAds_HorValue(cur_procads_hor_index, 1); 
	} else if (cur_procads_ver_index < procads_ver_num) {
			// pid sits at attribute index 1
		pid = jqDB->getJobQueueProcAds_VerValue(cur_procads_ver_index, 1); 
	} else {
		delete ad;
		ad = NULL;
		return DONE_PROCADS_CURSOR;
	}

		//the below four if statements were created to 
		//eliminate any bizarre race conditions between 
		//reading and writing the tables - the latter being
		//done by the quill daemon. 
	if(pid == NULL) {
		delete ad;
		ad = NULL;
		return DONE_PROCADS_CURSOR;
	}
	else if (curProcId == NULL) {
		curProcId = pid;   
	}
	else if (strcmp(pid, curProcId) == 0) {
		delete ad;
		ad = NULL;
		return FAILURE;
	}
	else  { /* pid and curProcId are not NULL and not equal */ 
		curProcId = pid;
	}

		//the below two while loops grab stuff out of 
		//the Procads_Num and Procads_Str table

	while(cur_procads_ver_index < procads_ver_num) {
		cid = jqDB->getJobQueueProcAds_VerValue(
				cur_procads_ver_index, 0); // cid
		pid = jqDB->getJobQueueProcAds_VerValue(
				cur_procads_ver_index, 1); // pid

		if ((strcmp(cid, curClusterId) != 0) || 
			(strcmp(pid, curProcId) != 0))
			break;

		attr = jqDB->getJobQueueProcAds_VerValue(
				cur_procads_ver_index, 2); // attr
		val  = jqDB->getJobQueueProcAds_VerValue(
				cur_procads_ver_index++, 3); // val


		char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
		sprintf(expr, "%s = %s", attr, val);
		// add an attribute with a value into ClassAd
		ad->Insert(expr);
		free(expr);

	};

	int numfields = jqDB->getJobQueueProcHorNumFields();
	
	for(int i = 2; i < numfields; i++) {

		attr = jqDB->getJobQueueProcHorFieldName(i);
		val = jqDB->getJobQueueProcAds_HorValue(
				cur_procads_hor_index, i); // val
		char* expr = (char*)malloc(strlen(attr) + strlen(val) + 6);
		sprintf(expr, "%s = %s", attr, val);
			// add an attribute with a value into ClassAd
		ad->Insert(expr);
		free(expr);
	}

	ad->SetMyTypeName("Job");
	ad->SetTargetTypeName("Machine");
	cur_procads_hor_index++;

	char* expr = 
	  (char *) malloc(strlen(ATTR_SERVER_TIME)
			  + 3   // for " = "
			  + 20  // for integer
			  + 1); // for null termination


	sprintf(expr, "%s = %ld", ATTR_SERVER_TIME, (long)time(NULL));
	// add an attribute with a value into ClassAd
	ad->Insert(expr);
	free(expr);
	return SUCCESS;

}

/*
	Return:
	    SUCCESS
		DONE_JOBS_CURSOR
	    FAILURE
*/
QuillErrCode
JobQueueSnapshot::iterateAllClassAds(ClassAd*& ad)
{
	QuillErrCode		st1, st2;

	/* while there are still more procads associated with current clusterad,
	   dont get another clusterad
	*/
	
	st1 = getNextProcAd(ad);
	while(st1 == DONE_PROCADS_CUR_CLUSTERAD) {
		st2 = getNextClusterAd(curClusterId, curClusterAd);
		
			//a sanity check for a race condition that should never occur
			//but has, in the past. Not having the below check may result
			//in an infinite loop. If this sanity check is removed, one
			//way to recreate this infinite loop
			//is by simply submitting 2 sets of jobs (so two 
			//clusters) and then logging into the db and deleting the 
			//clusterad attributes for the most recent cluster. Then simply
			//issue a condor_q and the infinite loop will result
			//I'm not sure how this race condition occurs. Firstly, querying
			//clusterads and procads are done within a transaction. Secondly,
			//if st2 was in fact DONE_CLUSTERADS_CURSOR, then st1 should have
			//been DONE_PROCADS_CURSOR and not DONE_PROCADS_CUR_CLUSTERAD. 
			//So if st1 was DONE_PROCADS_CURSOR, we'd immediately exit the 
			//while loop, and there wouldn't be an infinite loop.
			//Anyway, this sanity check avoids this race condition although
			//we are still not sure how this race condition is caused in the
			//first place
			// - Ameet 10/18
		if(st2 == DONE_CLUSTERADS_CURSOR) {
			break;
		}
		st1 = getNextProcAd(ad);
	};

	if (st1 == SUCCESS) {
		ad->ChainToAd(curClusterAd);
	}	

		//the third check here is triggered when the above bizarre race 
		//condition occurs
	else if (st1 == DONE_PROCADS_CURSOR 
			 || st1 == FAILURE 
			 || (st1 == DONE_PROCADS_CUR_CLUSTERAD 
				 && st2 == DONE_CLUSTERADS_CURSOR)) {
		return DONE_JOBS_CURSOR;
	}
	return SUCCESS;
}

//! release snapshot
QuillErrCode
JobQueueSnapshot::release()
{
	QuillErrCode st1, st2;
	st1 = jqDB->releaseJobQueueResults();
	st2 = DBObj->disconnectDB();

	if(st1 == SUCCESS && st2 == SUCCESS) {
		return SUCCESS;
	}
	else {
		return FAILURE;
	}
}
