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
#include <math.h>
#include "jobqueuecollection.h"
#include "get_daemon_name.h"
#include "condor_config.h"
#include "misc_utils.h"

//! constructor
JobQueueCollection::JobQueueCollection(int iBucketNum)
{
	int i;
	char *tmp;

	_iBucketSize = iBucketNum;
	typedef ClassAdBucket* ClassAdBucketPtr;
	_ppProcAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	_ppClusterAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	for (i = 0; i < _iBucketSize; i++) {
		_ppProcAdBucketList[i] = NULL;
		_ppClusterAdBucketList[i] = NULL;
	}

	procAdNum = clusterAdNum = 0;
	curClusterAdIterateIndex = 0;
	curProcAdIterateIndex = 0;

	pCurBucket = NULL;
	bChained = false;

	ClusterAd_H_CopyStr = NULL;
	ClusterAd_V_CopyStr = NULL;
	ProcAd_H_CopyStr = NULL;
	ProcAd_V_CopyStr = NULL;	

	tmp = param( "SCHEDD_NAME" );
	if( tmp ) {
		scheddname = build_valid_daemon_name( tmp );
	} else {
		scheddname = default_daemon_name();
	}  

	free(tmp);
}

//! destructor
JobQueueCollection::~JobQueueCollection()
{
	int i;

	ClassAdBucket *pCurBucket;
	ClassAdBucket *pPreBucket;
	for (i = 0; i < _iBucketSize; i++) {
		pCurBucket = _ppProcAdBucketList[i];
		while (pCurBucket != NULL) {
			pPreBucket = pCurBucket;
			pCurBucket = pCurBucket->pNext;
			delete pPreBucket;
		}

		pCurBucket = _ppClusterAdBucketList[i];
		while (pCurBucket != NULL) {
			pPreBucket = pCurBucket;
			pCurBucket = pCurBucket->pNext;
			delete pPreBucket;
		}
	}

	delete[] _ppClusterAdBucketList;
	delete[] _ppProcAdBucketList;

		// free temporarily used COPY string buffers
	if (ClusterAd_H_CopyStr != NULL) free(ClusterAd_H_CopyStr);
	if (ClusterAd_V_CopyStr != NULL) free(ClusterAd_V_CopyStr);
	if (ProcAd_H_CopyStr != NULL) free(ProcAd_H_CopyStr);
	if (ProcAd_V_CopyStr != NULL) free(ProcAd_V_CopyStr);
}

//! find a ProcAd
/*! \param cid Cluster Id
 *  \param pid Proc Id
 *  \return Proc ClassAd
 */
ClassAd*
JobQueueCollection::findProcAd(char* cid, char* pid)
{
	return find(cid, pid);
}

//! find a ClusterAd
/*! \param cid Cluster Id
 *  \return Cluster ClassAd
 */
ClassAd*
JobQueueCollection::findClusterAd(char* cid)
{
	return find(cid);
}

//! find a ClassAd - if pid == NULL return ProcAd, else return ClusterAd
/*! \param cid Cluster Id
 *  \param pid Proc Id (default is NULL)
 *  \return Cluster or Proc Ad
 */
ClassAd*
JobQueueCollection::find(char* cid, char* pid)
{
	int cid_len, pid_len;
	char* id = NULL;
	int	ad_type;
	int index;
	int len;

	cid_len = pid_len = 0;

	if (pid == NULL) { // need to find ClusterAd
		ad_type = ClassAdBucket::CLUSTER_AD;
		index = hashfunction(cid); // hash function invoked
		cid_len = strlen(cid);
	}
	else { // need to find ProcAd
		ad_type = ClassAdBucket::PROC_AD;
		cid_len = strlen(cid);
		pid_len = strlen(pid);
		len = cid_len + pid_len + 2;
		id = (char*)malloc(len * sizeof(char));
		snprintf(id, len, "%s%s", pid,cid);
		index = hashfunction(id); // hash function invoked
		free(id);
	}

	// find a bucket with a index
	ClassAdBucket *pCurBucket;
	if (ad_type == ClassAdBucket::CLUSTER_AD)
		pCurBucket = _ppClusterAdBucketList[index];
	else 
		pCurBucket = _ppProcAdBucketList[index];


	// find ClassAd 
	// if there is a chain, follow it
	while(pCurBucket != NULL)
	{
		if (
			((ad_type == ClassAdBucket::CLUSTER_AD) && 
		    (strcmp(cid, pCurBucket->cid) == 0)) 
			||
			((ad_type == ClassAdBucket::PROC_AD) &&
		     (strcmp(cid, pCurBucket->cid) == 0) &&
		     (strcmp(pid, pCurBucket->pid) == 0))
		   )
		{
			return pCurBucket->ad; 
		}
		else {
			pCurBucket = pCurBucket->pNext;
		}
	}

	return NULL;
}

//! insert a ProcAd
/*! \param cid Cluster Id
 *  \param pid Proc Id
 *  \parm procAd ProcAd to be inserted
 *  \return the result status of insert
 */
int 
JobQueueCollection::insertProcAd(char* cid, char* pid, ClassAd* procAd)
{
	int st;
	int len = strlen(cid) + strlen(pid) + 2;
	char* id = (char*)malloc(len * sizeof(char));

	ClassAdBucket *pBucket = new ClassAdBucket(cid, pid, procAd);

	snprintf(id, len, "%s%s",pid,cid);
	st = insert(id, pBucket, _ppProcAdBucketList);
	if (st > 0) {
		++procAdNum;
	}

	free(id); // id is just used for hashing purpose, 
			  // so it must be freed here.
	return st;
}

//! insert a ClusterAd
/*! \param cid Cluster Id
 *  \parm clusetrAd ClusterAd to be inserted
 *  \return the result status of insert
 */
int
JobQueueCollection::insertClusterAd(char* cid, ClassAd* clusterAd)
{
	int st;
	ClassAdBucket *pBucket = new ClassAdBucket(cid, clusterAd);
	st = insert(cid, pBucket, _ppClusterAdBucketList);
	if (st > 0) {
		++clusterAdNum;
	}

	return st;
}

/*! insert a ClassAd into a hash table
 *  \param id Id
 *	\param pBucket bucket to be inserted
 *	\param ppBucketList Bucket List which this bucket is inserted into 
 *  \return the result status of insert
			1: sucess
			0: duplicate
			-1: error
*/
int 
JobQueueCollection::insert(char* id, ClassAdBucket* pBucket, ClassAdBucket **ppBucketList)
{
	// hash function invoke
	
	int index = hashfunction(id);

	// find and delete in a bucket list
	ClassAdBucket *pCurBucket = ppBucketList[index];

	if (pCurBucket == NULL) {
		ppBucketList[index] = pBucket;
		return 1;
	}

	while(1)
	{
		// duplicate check
		if (pCurBucket->pid == NULL && pBucket->pid == NULL) {
			if (strcasecmp(pCurBucket->cid, pBucket->cid) == 0) {
					return 0;
			}
		}
		else if (pCurBucket->pid != NULL && pBucket->pid != NULL) {
			if ((strcasecmp(pCurBucket->cid, pBucket->cid) == 0) &&
				(strcasecmp(pCurBucket->pid, pBucket->pid) == 0)) {
					return 0;
			}
		}

		if (pCurBucket->pNext == NULL) {
			pCurBucket->pNext = pBucket;
			return 1;
		}
		else {
			pCurBucket = pCurBucket->pNext;
		}
	}

	return -1;
}


//! delete a ProcAd
/*! \param cid Cluster Id
 *  \param pid Proc Id
 */
int
JobQueueCollection::removeProcAd(char* cid, char* pid)
{
	return remove(cid, pid);
}

//! delete a ClusterAd
/*! \param cid Cluster Id
 */
int 
JobQueueCollection::removeClusterAd(char* cid)
{
	return remove(cid);
}

//! delete a ClassAd from a collection
/*! \param cid Cluter Id
 *  \parma pid Proc Id
 *  \return the result status of deletion
 *			1: sucess
 *			0: not found
 *			-1: other errors
 */
int 
JobQueueCollection::remove(char* cid, char* pid)
{
	int 	cid_len, pid_len, ad_type, index;
	char* 	id = NULL;
	int i;
	int len;

	cid_len = pid_len = 0;

	if (pid == NULL) { // need to delete a ClusterAd
		ad_type = ClassAdBucket::CLUSTER_AD;
		index = hashfunction(cid); // hash function invoked
		cid_len = strlen(cid);
	}
	else { // need to delete a ProcAd
		ad_type = ClassAdBucket::PROC_AD;
		cid_len = strlen(cid);
		pid_len = strlen(pid);
		len = cid_len + pid_len + 2;
		id = (char*)malloc(len * sizeof(char));
		snprintf(id, len, "%s%s", pid, cid);
		index = hashfunction(id); // hash function invoked
		free(id);
	}

	// find and delete in a bucket list
	ClassAdBucket *pPreBucket;
	ClassAdBucket *pCurBucket;
	ClassAdBucket **ppBucketList;
	if (ad_type == ClassAdBucket::CLUSTER_AD) {
		pPreBucket = pCurBucket = _ppClusterAdBucketList[index];
		ppBucketList = _ppClusterAdBucketList;
	}
	else {
		pPreBucket = pCurBucket = _ppProcAdBucketList[index];
		ppBucketList = _ppProcAdBucketList;
	}

	for (i = 0; pCurBucket != NULL; i++)
	{
		if (((ad_type == ClassAdBucket::CLUSTER_AD) && 
		    (strcmp(cid, pCurBucket->cid) == 0)) 
			||
			((ad_type == ClassAdBucket::PROC_AD) &&
		     (strcmp(cid, pCurBucket->cid) == 0) &&
		     (strcmp(pid, pCurBucket->pid) == 0)))
		{
			if (i == 0) {
				ppBucketList[index] = pCurBucket->pNext;
			}
			else {
				pPreBucket->pNext = pCurBucket->pNext;
			}

			delete pCurBucket;
			return 1;
		}
		else {
			pPreBucket = pCurBucket;
			pCurBucket = pCurBucket->pNext;
		}
	}

	return 0;
}

//! hashing function
/*! \return hash value
 */
int 
JobQueueCollection::hashfunction(char* str)
{
	int 			str_len = strlen(str);
	int 			i;
	char 			first_char = '.';
	unsigned long 	hash_val = 0;

	for (i = 0; i < str_len; i++) {
		hash_val += (long)((int)str[i] - (int)first_char) * (long)pow(37, i);
	}

	return (int)(hash_val % _iBucketSize);
}

//! initialize all job ads iteration
void
JobQueueCollection::initAllJobAdsIteration()
{
	curProcAdIterateIndex = 0; // that of ProcAd List
	curClusterAdIterateIndex = 0; // that of ClusterAd List
	pCurBucket = NULL;
	bChained = false;

	if (ClusterAd_H_CopyStr != NULL) {
		free(ClusterAd_H_CopyStr);
		ClusterAd_H_CopyStr = NULL;
	}

	if (ClusterAd_V_CopyStr != NULL) {
		free(ClusterAd_V_CopyStr);
		ClusterAd_V_CopyStr = NULL;
	}

	if (ProcAd_H_CopyStr != NULL) {
		free(ProcAd_H_CopyStr);
		ProcAd_H_CopyStr = NULL;
	}

	if (ProcAd_V_CopyStr != NULL) {
		free(ProcAd_V_CopyStr);
		ProcAd_V_CopyStr = NULL;
	}
}

//! get the next COPY string for ClusterAd_Horizontal table
/*! \warning the returned string must not be freeed
 */
char*
JobQueueCollection::getNextClusterAd_H_CopyStr()
{
	if (ClusterAd_H_CopyStr != NULL) {
		free(ClusterAd_H_CopyStr);
		ClusterAd_H_CopyStr = NULL;
	}

	getNextAdCopyStr(true, curClusterAdIterateIndex, _ppClusterAdBucketList, ClusterAd_H_CopyStr);

	return ClusterAd_H_CopyStr;
}

//! get the next COPY string for ClusterAd_Vertical table
/*! \warning the returned string must not be freeed
 */
char*
JobQueueCollection::getNextClusterAd_V_CopyStr()
{
	if (ClusterAd_V_CopyStr != NULL) {
		free(ClusterAd_V_CopyStr);
		ClusterAd_V_CopyStr = NULL;
	}

	getNextAdCopyStr(false, curClusterAdIterateIndex, _ppClusterAdBucketList, ClusterAd_V_CopyStr);
	return ClusterAd_V_CopyStr;
}

//! get the next COPY string for ProcAd_Horizontal table
/*! \warning the returned string must not be freeed
 */
char*
JobQueueCollection::getNextProcAd_H_CopyStr()
{
	if (ProcAd_H_CopyStr != NULL) {
		free(ProcAd_H_CopyStr);
		ProcAd_H_CopyStr = NULL;
	}

	getNextAdCopyStr(true, curProcAdIterateIndex, _ppProcAdBucketList, ProcAd_H_CopyStr);
	return ProcAd_H_CopyStr;
}

//! get the next COPY string for ProcAd_Vertical table
/*! \warning the returned string must not be freeed
 */
char*
JobQueueCollection::getNextProcAd_V_CopyStr()
{
	if (ProcAd_V_CopyStr != NULL) {
		free(ProcAd_V_CopyStr);
		ProcAd_V_CopyStr = NULL;
	}

	getNextAdCopyStr(false, curProcAdIterateIndex, _ppProcAdBucketList, ProcAd_V_CopyStr);
	return ProcAd_V_CopyStr;
}



void
JobQueueCollection::getNextAdCopyStr(bool bHor, 
									 int& index, 
									 ClassAdBucket **ppBucketList, 
									 char*& ret_str)
{	
  // index is greater than the last index of bucket list?
  // we cant call it quits if there's another chained ad after this
  // and its the last bucket -- ameet
	if (index == _iBucketSize && !bChained) {
		if (ret_str != NULL) { 
			free(ret_str);
			ret_str = NULL;
		}
		return;
	}
		 
	if (bChained == false) { 
		pCurBucket = ppBucketList[index++];
		
		while (pCurBucket == NULL) {
			if (index == _iBucketSize) {
				if (ret_str != NULL) { 
					free(ret_str);
					ret_str = NULL;
				}
				return;
			}
			pCurBucket = ppBucketList[index++];
		}
	} 
	else { // we are following the chained buckets
		pCurBucket = pCurBucket->pNext;
	}
		// is there a chaned bucket?
	if (pCurBucket->pNext != NULL) {
		bChained = true;
	}
	else {
		bChained = false;
	}

	// making a COPY string for this ClassAd
	makeCopyStr(bHor, pCurBucket->cid, 
					  pCurBucket->pid, 
					  pCurBucket->ad, 
				ret_str);
}

void 
JobQueueCollection::makeCopyStr(bool bHor, 
								char* cid, 
								char* pid, 
								ClassAd* ad, 
								char*& ret_str)
{
	char* 	line_str = NULL;

	AssignOpBase*	expr;		// For Each Attribute in ClassAd
	VariableBase* 	nameExpr;	// For Attribute Name
	StringBase* 	valExpr;	// For Value
	char name[1000];
	char *value = NULL;

	char *jobstatus = (char *) 0;
	char *imagesize = (char *) 0;
	char *remoteusercpu = (char *) 0;
	char *remotewallclocktime = (char *) 0;
	char *remotehost = (char *) 0;
	char *globaljobid = (char *) 0;
	char *owner = (char *) 0;
	char *jobprio = (char *) 0;
	char *qdate = (char *) 0;
	char *cmd = (char *) 0;
	char *args = (char *) 0;
	int len;

		// init of returned string
	if (ret_str != NULL) {
		free(ret_str);
		ret_str = NULL;
	}

	ad->ResetExpr(); // for iteration initialization

	while((expr = (AssignOpBase*)(ad->NextExpr())) != NULL) {

		nameExpr = (VariableBase*)expr->LArg(); // Name Express Tree
		valExpr = (StringBase*)expr->RArg();	// Value Express Tree

			// free the previous  value
		if (value) {
			free(value);
		}

		valExpr->PrintToNewStr(&value);

		if (value == NULL) break;

		strip_double_quote(value);

		strcpy(name, "");
		nameExpr->PrintToStr(name);		
		
			// loading horizontal part
		if (bHor) {
				// procad
			if (pid != NULL) {
				if(strcasecmp(name, "jobstatus") ==0) {
					jobstatus = strdup(value);
				} else if (strcasecmp(name, "imagesize") ==0) {
					imagesize = strdup(value);
				} else if (strcasecmp(name, "globaljobid") ==0) {
					globaljobid = strdup(value);
				} else if (strcasecmp(name, "remotewallclocktime") ==0) {
					remotewallclocktime = strdup(value);
				} else if (strcasecmp(name, "remoteusercpu") ==0) {
					remoteusercpu = strdup(value);
				} else if (strcasecmp(name, "jobprio") ==0) {
					jobprio = strdup(value);
				} else if (strcasecmp(name, "args") ==0) {
					args = strdup(value);
				} else if (strcasecmp(name, "remotehost") ==0) {
					remotehost = strdup(value);
				}
			} else {  // cluster ad
				if(strcasecmp(name, "jobstatus") ==0) {
					jobstatus = strdup(value);
				} else if (strcasecmp(name, "imagesize") ==0) {
					imagesize = strdup(value);
				} else if (strcasecmp(name, "remotewallclocktime") ==0) {
					remotewallclocktime = strdup(value);
				} else if (strcasecmp(name, "remoteusercpu") ==0) {
					remoteusercpu = strdup(value);
				} else if (strcasecmp(name, "owner") ==0) {
					owner = strdup(value);
				} else if (strcasecmp(name, "jobprio") ==0) {
					jobprio = strdup(value);
				} else if (strcasecmp(name, "qdate") ==0) {
						// change the qdate value from seconds to date time format
					time_t numsecs;
					struct tm *tm;
					char tmp[100];
					
					numsecs =  atoi(value);
					tm = localtime((time_t *)&numsecs);
					
					snprintf(tmp, 100, "%d/%d/%d %02d:%02d:%02d %s", 
							tm->tm_mon+1,
							tm->tm_mday,
							tm->tm_year+1900,
							tm->tm_hour,
							tm->tm_min,
							tm->tm_sec,
							my_timezone(tm->tm_isdst));

					qdate = strdup(tmp);

				} else if (strcasecmp(name, "cmd") ==0) {
					cmd = strdup(value);
				} else if (strcasecmp(name, "args") ==0) {
					args = strdup(value);
				}
			}

			continue;

		} else { // loading vertical part
				// procad
			if (pid != NULL) {

				if (isHorizontalProcAttribute(name)) {
					continue;
				}

				len = strlen(name) + strlen(scheddname)
					+ strlen(value) + strlen(cid) 
					+ strlen(pid) + strlen("\t\t\t\t\n") + 1;
				line_str = (char*)malloc(len * sizeof(char));
				snprintf(line_str, len, "%s\t%s\t%s\t%s\t%s\n", scheddname,
						 cid, pid, name, value); 				
				
			} else {	//cluster ad
				if (isHorizontalClusterAttribute(name)) {
					continue;
				}

				len = strlen(name) 
					+ strlen(value) + strlen(cid) 
					+ strlen(scheddname) + strlen("\t\t\t\n") + 1;
				line_str = (char*)malloc(len * sizeof(char));
				snprintf(line_str, len, "%s\t%s\t%s\t%s\n", scheddname,
						 cid, name, value); 					
			}
		}

			// concatenate the line to the ClassAd COPY string  
		if (ret_str == NULL) {
			len = strlen(line_str) + 1;
			ret_str = (char*)malloc(len);
			strncpy(ret_str, line_str, len);
		}
		else {
			len = strlen(line_str) + 1;
			ret_str = (char*)realloc(ret_str, 
									 strlen(ret_str) + len);
			strncat(ret_str, line_str, len);
		}

		free(line_str);
		line_str = NULL;
	}

	if (value) {
		free(value);
	}

	if (bHor) {
			// procad
		if (pid != NULL) {
			len = strlen(scheddname) 
				+ strlen(cid) + strlen(pid) 
				+ (jobstatus?strlen(jobstatus):3) 
				+ (imagesize?strlen(imagesize):3) 
				+ (globaljobid?strlen(globaljobid):3) 
				+ (remotewallclocktime?strlen(remotewallclocktime):3)
				+ (remoteusercpu?strlen(remoteusercpu):3)
				+ (remotehost?strlen(remotehost):3)
				+ (jobprio?strlen(jobprio):3)
				+ (args?strlen(args):3)				
				+ strlen("\t\t\t\t\t\t\t\t\t\t\n") + 1;
				line_str = (char*)malloc(len * sizeof(char));
				snprintf(line_str, len, 
						 "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
						 scheddname,
						 cid, pid, 
						 jobstatus?jobstatus:"\\N", 
						 imagesize?imagesize:"\\N", 
						 remoteusercpu?remoteusercpu:"\\N", 
						 remotewallclocktime?remotewallclocktime:"\\N", 
						 remotehost?remotehost:"\\N", 
						 globaljobid?globaljobid:"\\N",
						 jobprio?jobprio:"\\N",
						 args?args:"\\N");
		} else {  // clusterad
			len = strlen(scheddname) 
				+ strlen(cid) 
				+ (owner?strlen(owner):3) 
				+ (jobstatus?strlen(jobstatus):3) 
				+ (jobprio?strlen(jobprio):3) 
				+ (imagesize?strlen(imagesize):3) 
				+ (qdate?strlen(qdate):3)
				+ (remoteusercpu?strlen(remoteusercpu):3)
				+ (remotewallclocktime?strlen(remotewallclocktime):3)
				+ (cmd?strlen(cmd):3)
				+ (args?strlen(args):3) 
				+ strlen("\t\t\t\t\t\t\t\t\t\t\n") + 1;
			line_str = (char*)malloc(len * sizeof(char));
			snprintf(line_str, len, 
					"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", scheddname,
					cid, owner?owner:"\\N", jobstatus?jobstatus:"\\N", 
					jobprio?jobprio:"\\N", 
					imagesize?imagesize:"\\N", qdate?qdate:"\\N", 
					remoteusercpu?remoteusercpu:"\\N", 
					remotewallclocktime?remotewallclocktime:"\\N", 
					cmd?cmd:"\\N", args?args:"\\N");
		}

		len = strlen(line_str) + 1;
		ret_str = (char*)malloc(len * sizeof(char));
		strncpy(ret_str, line_str, len);
		free(line_str);
		line_str = NULL;		

		if(jobstatus) {
			free(jobstatus);
		}

		if(imagesize) {
			free(imagesize);
		}

		if(remoteusercpu) {
			free(remoteusercpu);
		}

		if(remotewallclocktime) {
			free(remotewallclocktime);
		}

		if(globaljobid) {
			free(globaljobid);
		}

		if(owner) {
			free(owner);
		}
		
		if(jobprio) {
			free(jobprio);
		}
		
		if(qdate) {
			free(qdate);
		}

		if(cmd) {
			free(cmd);
		}

		if(args) {
			free(args);
		}
	}
}

bool isHorizontalHistoryAttribute(const char *attr) {

  if((strcasecmp(attr, "qdate") == 0) || 
     (strcasecmp(attr, "owner") == 0) ||
     (strcasecmp(attr, "globaljobid") == 0) ||
     (strcasecmp(attr, "numckpts") == 0) ||
     (strcasecmp(attr, "numrestarts") == 0) ||
     (strcasecmp(attr, "numsystemholds") == 0) ||
     (strcasecmp(attr, "condorversion") == 0) ||
     (strcasecmp(attr, "condorplatform") == 0) ||
     (strcasecmp(attr, "rootdir") == 0) ||
     (strcasecmp(attr, "iwd") == 0) ||
     (strcasecmp(attr, "jobuniverse") == 0) ||
     (strcasecmp(attr, "cmd") == 0) ||
     (strcasecmp(attr, "minhosts") == 0) ||
     (strcasecmp(attr, "maxhosts") == 0) ||
     (strcasecmp(attr, "jobprio") == 0) ||
     (strcasecmp(attr, "user") == 0) ||     
     (strcasecmp(attr, "env") == 0) ||
     (strcasecmp(attr, "userlog") == 0) ||
     (strcasecmp(attr, "coresize") == 0) ||
     (strcasecmp(attr, "killsig") == 0) ||
     (strcasecmp(attr, "rank") == 0) ||
     (strcasecmp(attr, "in") == 0) ||
     (strcasecmp(attr, "transferin") == 0) ||
     (strcasecmp(attr, "out") == 0) ||
     (strcasecmp(attr, "transferout") == 0) ||
     (strcasecmp(attr, "err") == 0) ||
     (strcasecmp(attr, "transfererr") == 0) ||
     (strcasecmp(attr, "shouldtransferfiles") == 0) ||
     (strcasecmp(attr, "transferfiles") == 0) ||
     (strcasecmp(attr, "executablesize") == 0) ||
     (strcasecmp(attr, "diskusage") == 0) ||
     (strcasecmp(attr, "requirements") == 0) ||
     (strcasecmp(attr, "filesystemdomain") == 0) ||
     (strcasecmp(attr, "args") == 0) ||
     (strcasecmp(attr, "lastmatchtime") == 0) ||
     (strcasecmp(attr, "numjobmatches") == 0) ||
     (strcasecmp(attr, "jobstartdate") == 0) ||
     (strcasecmp(attr, "jobcurrentstartdate") == 0) ||
     (strcasecmp(attr, "jobruncount") == 0) ||
     (strcasecmp(attr, "filereadcount") == 0) ||
     (strcasecmp(attr, "filereadbytes") == 0) ||
     (strcasecmp(attr, "filewritecount") == 0) ||
     (strcasecmp(attr, "filewritebytes") == 0) ||
     (strcasecmp(attr, "fileseekcount") == 0) ||
     (strcasecmp(attr, "totalsuspensions") == 0) ||
     (strcasecmp(attr, "imagesize") == 0) ||
     (strcasecmp(attr, "exitstatus") == 0) ||
     (strcasecmp(attr, "localusercpu") == 0) ||
     (strcasecmp(attr, "localsyscpu") == 0) ||
     (strcasecmp(attr, "remoteusercpu") == 0) ||
     (strcasecmp(attr, "remotesyscpu") == 0) ||
     (strcasecmp(attr, "bytessent") == 0) ||
     (strcasecmp(attr, "bytesrecvd") == 0) ||
     (strcasecmp(attr, "rscbytessent") == 0) ||
     (strcasecmp(attr, "rscbytesrecvd") == 0) ||
     (strcasecmp(attr, "exitcode") == 0) ||
     (strcasecmp(attr, "jobstatus") == 0) ||
     (strcasecmp(attr, "enteredcurrentstatus") == 0) ||
     (strcasecmp(attr, "remotewallclocktime") == 0) ||
     (strcasecmp(attr, "lastremotehost") == 0) ||
     (strcasecmp(attr, "completiondate") == 0)) {
	  return true;
  }

  return false;
}

bool isHorizontalClusterAttribute(const char *attr) {

  if((strcasecmp(attr, "owner") == 0) ||
     (strcasecmp(attr, "jobstatus") == 0) ||
     (strcasecmp(attr, "jobprio") == 0) ||
     (strcasecmp(attr, "imagesize") == 0) ||
     (strcasecmp(attr, "qdate") == 0) ||
     (strcasecmp(attr, "remoteusercpu") == 0) ||
     (strcasecmp(attr, "remotewallclocktime") == 0) ||
     (strcasecmp(attr, "cmd") == 0) ||
     (strcasecmp(attr, "args") == 0)) {
    return true;
  }

  return false;  
}

bool isHorizontalProcAttribute(const char *attr) {

  if((strcasecmp(attr, "jobstatus") == 0) ||
     (strcasecmp(attr, "imagesize") == 0) ||
     (strcasecmp(attr, "globaljobid") == 0) ||
     (strcasecmp(attr, "remotewallclocktime") == 0) ||
     (strcasecmp(attr, "remoteusercpu") == 0) ||
	 (strcasecmp(attr, "jobprio") == 0) ||
	 (strcasecmp(attr, "args") == 0) || 
	 (strcasecmp(attr, "remotehost") == 0)) {
    return true;
  }
     
  return false;
}
