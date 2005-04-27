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
#include "condor_attributes.h"
#include "classad_merge.h"

#include "pgsqldatabase.h"
#include "jobqueuesnapshot.h"

//! constructor
JobQueueSnapshot::JobQueueSnapshot(const char* dbcon_str)
{
#ifdef _POSTGRESQL_DBMS_
	jqDB = new PGSQLDatabase(dbcon_str);
#endif
	curClusterAd = NULL;
	curClusterId = NULL;
	curProcId = NULL;

}

//! destructor
JobQueueSnapshot::~JobQueueSnapshot()
{
	if (jqDB != NULL)
		delete(jqDB);
}

//! prepare iteration of Job Ads in the job queue database
/*
	Return:
		-1: Error
		 0: No Result
		 1: Success
*/
int 
JobQueueSnapshot::startIterateAllClassAds(int cluster, 
					  int proc, 
					  char *owner, 
					  bool isfullscan)
{
	int st;
		// initialize index variables
	cur_procads_str_index = cur_procads_num_index =
	cur_clusterads_str_index = cur_clusterads_num_index = 0;

	procads_str_num = procads_num_num = 
	clusterads_str_num = clusterads_num_num = 0;

	st = jqDB->connectDB();
	if(st <= 0) {
		printf("Error while querying the database: no connection to the server\n");
		return -1;
	}
	st = jqDB->getJobQueueDB(cluster, 
				 proc, 
				 owner, 
				 isfullscan,
				 procads_str_num, 
				 procads_num_num,
				 clusterads_str_num, 
				 clusterads_num_num); // this retriesves DB
	if (st == 0) // There is no live jobs
		return 0;
	else if (st < 0) // Got some error
		return -1;

	if (getNextClusterAd(curClusterId, curClusterAd) < 0)
		return 0;

	return 1; // Success
}

//! iterate one by one
/*
	Return:
		 1: Success
		-1:	Error
		 0: No More ClusterAd
*/
int
JobQueueSnapshot::getNextClusterAd(const char*& cluster_id, ClassAd*& ad)
{
	const char	*cid, *attr, *val;

	cid = jqDB->getJobQueueClusterAds_StrValue(
			cur_clusterads_str_index, 0); // cid
	if (cid == NULL) 
		return 0;

	if (cluster_id == NULL) {
		cluster_id = (char*)cid;
		curProcId = NULL;
	}
	else if (strcmp(cluster_id, cid) == 0)
		return -1;
	else if (strcmp(cluster_id, cid) != 0) {
		cluster_id = (char*)cid;
		curProcId = NULL;
	}

	if (ad != NULL)
		delete ad;

		//
		// build a Next ClusterAd
		//

	ad = new ClassAd();

		// for ClusterAds_Num table
	while(cur_clusterads_num_index < clusterads_num_num) {
		cid = jqDB->getJobQueueClusterAds_NumValue(
				cur_clusterads_num_index, 0); // cid

		if (cid == NULL || strcmp(cluster_id, cid) != 0)
			break;

		attr = jqDB->getJobQueueClusterAds_NumValue(
				cur_clusterads_num_index, 1); // attr
		val = jqDB->getJobQueueClusterAds_NumValue(
				cur_clusterads_num_index++, 2); // val

		char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
		sprintf(expr, "%s = %s", attr, val);
		// add an attribute with a value into ClassAd
		ad->Insert(expr);
		free(expr);
	};


		// for ClusterAds_Str table
	while(cur_clusterads_str_index < clusterads_str_num) {
		cid = jqDB->getJobQueueClusterAds_StrValue(
				cur_clusterads_str_index, 0); // cid

		if (cid == NULL || strcmp(cid, curClusterId) != 0)
			break;
			
		attr = jqDB->getJobQueueClusterAds_StrValue(
				cur_clusterads_str_index, 1); // attr
		val = jqDB->getJobQueueClusterAds_StrValue(
				cur_clusterads_str_index++, 2); // val

		if ((strcasecmp(attr, "MyType") != 0) && 
			(strcasecmp(attr, "TargetType") != 0)) {
			char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
			sprintf(expr, "%s = %s", attr, val);
			// add an attribute with a value into ClassAd
			ad->Insert(expr);
			free(expr);
		}
	};

	return 1;
}

/*
	Return:
		2: No more ProcAds
		1: Success
		0: No more result for this ClusterAd
		-1: error
*/
int
JobQueueSnapshot::getNextProcAd(ClassAd*& ad)
{
	const char *cid, *pid, *attr, *val;

	if ((cur_procads_num_index >= procads_num_num) &&
		(cur_procads_str_index >= procads_str_num)) {
		ad = NULL;
		return 2;
	}
	else
		ad = new ClassAd();

	while(cur_procads_num_index < procads_num_num) {
		// Current ProcId Setting
		cid = jqDB->getJobQueueProcAds_NumValue(
			cur_procads_num_index, 0); // cid

		if (strcmp(cid, "0") != 0)
			break;
		else
			++cur_procads_num_index;
	};

	while(cur_procads_str_index < procads_str_num) {
		// Current ProcId Setting
		cid = jqDB->getJobQueueProcAds_StrValue(
			cur_procads_str_index, 0); // cid

		if (strcmp(cid, "0") != 0)
			break;
		else
			++cur_procads_str_index;
	};

	if (strcmp(cid, curClusterId) != 0)
		return 0;

	// Current ProcId Setting
	pid = jqDB->getJobQueueProcAds_StrValue(
			cur_procads_str_index, 1); // pid

	if(pid == NULL) 
		return 2;
	else if (curProcId == NULL && pid != NULL) 
		curProcId = pid;   
	else if (strcmp(pid, curProcId) == 0) 
		return -1;
	else if (strcmp(pid, curProcId) != 0) 
		curProcId = pid;
	else 
		return 0;

	while(cur_procads_num_index < procads_num_num) {
		cid = jqDB->getJobQueueProcAds_NumValue(
				cur_procads_num_index, 0); // cid
		pid = jqDB->getJobQueueProcAds_NumValue(
				cur_procads_num_index, 1); // pid

		if ((strcmp(cid, curClusterId) != 0) || 
			(strcmp(pid, curProcId) != 0))
			break;

		attr = jqDB->getJobQueueProcAds_NumValue(
				cur_procads_num_index, 2); // attr
		val  = jqDB->getJobQueueProcAds_NumValue(
				cur_procads_num_index++, 3); // val


		char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
		sprintf(expr, "%s = %s", attr, val);
		// add an attribute with a value into ClassAd
		ad->Insert(expr);
		free(expr);

	};

	while(cur_procads_str_index < procads_str_num) {
		cid = jqDB->getJobQueueProcAds_StrValue(
				cur_procads_str_index, 0); // cid
		pid = jqDB->getJobQueueProcAds_StrValue(
				cur_procads_str_index, 1); // pid

		if ((strcmp(cid, curClusterId) != 0) ||
			(strcmp(pid, curProcId) != 0))
			break;

		attr = jqDB->getJobQueueProcAds_StrValue(
				cur_procads_str_index, 2); // attr
		val  = jqDB->getJobQueueProcAds_StrValue(
				cur_procads_str_index++, 3); // val

		if (strcasecmp(attr, "MyType") == 0)
			ad->SetMyTypeName("Job");
		else if (strcasecmp(attr, "TargetType") == 0)
			ad->SetTargetTypeName("Machine");
		else { 
			char* expr = (char*)malloc(strlen(attr) + strlen(val) + 4);
			sprintf(expr, "%s = %s", attr, val);
			// add an attribute with a value into ClassAd
			ad->Insert(expr);
			free(expr);
		}
	};

	char* expr = 
	  (char *) malloc(strlen(ATTR_SERVER_TIME)
			  + 3   // for " = "
			  + 12  // for integer
			  + 1); // for null termination


	sprintf(expr, "%s = %ld", ATTR_SERVER_TIME, (long)time(NULL));
	// add an attribute with a value into ClassAd
	ad->Insert(expr);
	free(expr);
	return 1;

}

/*
	Return:
		1: Success
		0: No More Result
	   -1: Error
*/
int 
JobQueueSnapshot::iterateAllClassAds(ClassAd*& ad)
{
	int		st;

	//dprintf(D_ALWAYS, "calling getNextClusterAd\n");
	while((st = getNextProcAd(ad)) == 0) {
		getNextClusterAd(curClusterId, curClusterAd);
	};

	if (st == 1) {
		ad->ChainToAd(curClusterAd);
			/* MergeClassAds(ad, curClusterAd, false); */
	}	
	else if (st == 2)
		return 0;

	return 1;
}

//! release snapshot
int 
JobQueueSnapshot::release()
{
	jqDB->releaseJobQueueDB();
	jqDB->disconnectDB();

	return 1;
}
