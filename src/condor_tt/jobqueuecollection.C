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
#include "jobqueuedatabase.h"
#include "condor_ttdb.h"
#include "jobqueuedbmanager.h"

static int typeOf(char *attName);

#define CONDOR_TT_TYPE_STRING    1
#define CONDOR_TT_TYPE_NUMBER    2
#define CONDOR_TT_TYPE_TIMESTAMP 3

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
}

void JobQueueCollection::setDBObj(JobQueueDatabase *DBObj)
{
	this->DBObj = DBObj;
}

void JobQueueCollection::setDBtype(dbtype dt)
{
	this->dt = dt;
}

void JobQueueCollection::setJobQueueDBManager(JobQueueDBManager *ptr)
{
	this->jqDBManager = ptr;
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
		hash_val += (long)(((int)str[i] - (int)first_char) * 6907);
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
}

// load the next cluster ad
bool JobQueueCollection::loadNextClusterAd(QuillErrCode &errStatus)
{
	return loadNextAd(curClusterAdIterateIndex, _ppClusterAdBucketList, 
					  errStatus);
}

// load the next ProcAd
bool JobQueueCollection::loadNextProcAd(QuillErrCode &errStatus)
{
	return loadNextAd(curProcAdIterateIndex, _ppProcAdBucketList, 
					  errStatus);
}

bool
JobQueueCollection::loadNextAd(int& index, 
							   ClassAdBucket **ppBucketList, 
							   QuillErrCode &errStatus)
{	
  // index is greater than the last index of bucket list?
  // we cant call it quits if there's another chained ad after this
  // and its the last bucket -- ameet
	if (index == _iBucketSize && !bChained) {
		return FALSE;
	}
		 
	if (bChained == false) { 
		pCurBucket = ppBucketList[index++];
		
		while (pCurBucket == NULL) {
			if (index == _iBucketSize) {
				return FALSE;
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

	// load this class ad
	errStatus = loadAd(pCurBucket->cid, 
					   pCurBucket->pid, 
					   pCurBucket->ad);

	return TRUE;
}

QuillErrCode 
JobQueueCollection::loadAd(char* cid, 
						   char* pid, 
						   ClassAd* ad)
{
	char*  sql_str;
	AssignOpBase*	expr;		// For Each Attribute in ClassAd
	VariableBase* 	nameExpr;	// For Attribute Name
	StringBase* 	valExpr;	// For Value
	char name[1000];
	char *value = NULL;
	int len;
	char* attNameList = NULL, *attValList = NULL;
	char* tmpVal = NULL;
	char* newvalue = NULL;

		// first generate the key columns
	if (pid != NULL) {
		attNameList = (char *) malloc (50);
		attValList = (char *) malloc (strlen(scheddname) +
									  strlen(cid) + strlen(pid) + 10); 
		sprintf(attNameList, "(scheddname, cluster_id, proc_id");
		sprintf(attValList, "('%s', %s, %s", scheddname, cid, pid);
	} else {
		attNameList = (char *) malloc (50);
		attValList = (char *) malloc (strlen(scheddname) +
									  strlen(cid) + 10); 
		sprintf(attNameList, "(scheddname, cluster_id");
		sprintf(attValList, "('%s', %s", scheddname, cid);		
	}

	ad->ResetExpr(); // for iteration initialization

	while((expr = (AssignOpBase*)(ad->NextExpr())) != NULL) {
		nameExpr = (VariableBase*)expr->LArg(); // Name Express Tree
		valExpr = (StringBase*)expr->RArg();	// Value Express Tree

		valExpr->PrintToNewStr(&value);

		if (value == NULL) break;

		//strip_double_quote(value);

		strcpy(name, "");
		nameExpr->PrintToStr(name);		
		
			// procad
		if (pid != NULL) {
			if (isHorizontalProcAttribute(name)) {
				attNameList = (char *) realloc (attNameList, 
												strlen(attNameList) + 
												strlen(name) + 5);
				attValList = (char *) realloc (attValList, 
											   strlen(attValList) + 
											   2*strlen(value) + 8);
				strcat(attNameList, ", ");
				strcat(attNameList, name);

				strcat(attValList, ", ");
				tmpVal = (char  *) malloc(2*strlen(value) + 300);

				switch (typeOf(name)) {				
				case CONDOR_TT_TYPE_STRING:	
					newvalue = jqDBManager->fillEscapeCharacters(value);
					sprintf(tmpVal, "'%s'", newvalue);
					free(newvalue);
					break;
				case CONDOR_TT_TYPE_NUMBER:
					sprintf(tmpVal, "%s", value);
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					char *ts_expr;
					clock = atoi(value);

					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr == NULL) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not builtin JobQueueCollection::loadAd\n");
						free(attNameList);
						free(attValList);	
						free(tmpVal);
						free(value);
						return FAILURE;
					}
					
					sprintf(tmpVal, "%s", ts_expr);
					free(ts_expr);
					
						/* the converted timestamp value is longer, so realloc
						   the buffer for attValList
						*/
					
					len = strlen(attValList) + strlen(tmpVal) + 8;

					attValList = (char *) realloc (attValList, len);
					
					break;
				default:
					dprintf(D_ALWAYS, "loadAd: unsupported horizontal proc ad attribute\n");

					free(attNameList);
					free(attValList);	
					free(value);
					free(tmpVal);
					return FAILURE;
				}
				strcat(attValList, tmpVal);
				free(tmpVal);
			} else {			
					// this is a vertical attribute
				newvalue = jqDBManager->fillEscapeCharacters(value);
				len = 1024 + strlen(name) + strlen(scheddname) +
					strlen(newvalue) + strlen(cid) + strlen(pid);
				sql_str = (char *) malloc(len);
				snprintf(sql_str, len, "INSERT INTO ProcAds_Vertical VALUES('%s', %s, %s, '%s', '%s')", 
						 scheddname,cid, pid, name, newvalue);
				free(newvalue);

				if (DBObj->execCommand(sql_str) == FAILURE) {
					dprintf(D_ALWAYS, "JobQueueCollection::loadAd - ERROR [SQL] %s\n", sql_str);
					free(sql_str);
					free(value);
					free(attNameList);
					free(attValList);
					return FAILURE;
				}
				free (sql_str);
			}
		} else {  // cluster ad
			if (isHorizontalClusterAttribute(name)) {
				len = strlen(attNameList) + strlen(name) + 5;

				attNameList = (char *) realloc (attNameList, 
												len);

				len = strlen(attValList) + 2*strlen(value) + 8;

				attValList = (char *) realloc (attValList, 
											   len);
				strcat(attNameList, ", ");
				strcat(attNameList, name);

				strcat(attValList, ", ");
				tmpVal = (char  *) malloc(2*strlen(value) + 300);

				switch (typeOf(name)) {				
				case CONDOR_TT_TYPE_STRING:	
					newvalue = jqDBManager->fillEscapeCharacters(value);
					sprintf(tmpVal, "'%s'", newvalue);
					free(newvalue);
					break;
				case CONDOR_TT_TYPE_NUMBER:
					sprintf(tmpVal, "%s", value);
					break;
				case CONDOR_TT_TYPE_TIMESTAMP:
					time_t clock;
					char *ts_expr;
					clock = atoi(value);					
					
					ts_expr = condor_ttdb_buildts(&clock, dt);	

					if (ts_expr == NULL) {
						dprintf(D_ALWAYS, "ERROR: Timestamp expression not builtin JobQueueCollection::loadAd\n");
						free(attNameList);
						free(attValList);	
						free(tmpVal);
						free(value);
						return FAILURE;
					}
					
					sprintf(tmpVal, "%s", ts_expr);
					free(ts_expr);	
					
						/* the converted timestamp value is longer, so realloc
						   the buffer for attValList
						*/
					
					len = strlen(attValList) + strlen(tmpVal) + 8;

					attValList = (char *) realloc (attValList, len);

					break;
				default:
					dprintf(D_ALWAYS, "loadAd: unsupported horizontal proc ad attribute\n");
					free(attNameList);
					free(attValList);	
					free(tmpVal);
					free(value);
					return FAILURE;
				}
				strcat(attValList, tmpVal);
				free(tmpVal); 				
			} else {
					// this is a vertical attribute
				newvalue = jqDBManager->fillEscapeCharacters(value);
				len = 1024 + strlen(name) + strlen(scheddname) +
					strlen(newvalue) + strlen(cid);
				sql_str = (char *) malloc(len);
				snprintf(sql_str, len, "INSERT INTO ClusterAds_Vertical VALUES('%s', %s, '%s', '%s')", scheddname,cid, name, newvalue);
				free(newvalue);
				if (DBObj->execCommand(sql_str) == FAILURE) {
					dprintf(D_ALWAYS, "JobQueueCollection::loadAd - ERROR [SQL] %s\n", sql_str);
					free(sql_str);
					free(value);
					free(attNameList);
					free(attValList);
					return FAILURE;
				}
				free (sql_str);
			}
		}

		free(value);
	} 

	strcat(attNameList, ")");
    strcat(attValList, ")");
	
		// load the horizontal tuple
		// procad
	if (pid != NULL) {
		len = 100 + strlen(attNameList) + strlen(attValList);
		sql_str = (char *) malloc(len);

		snprintf(sql_str, len, "INSERT INTO ProcAds_Horizontal %s VALUES %s", attNameList, attValList);

		if (DBObj->execCommand(sql_str) == FAILURE) {
			dprintf(D_ALWAYS, "JobQueueCollection::loadAd - ERROR [SQL] %s\n", sql_str);
			free(sql_str);
			free(attNameList);
			free(attValList);
			return FAILURE;
		}
		free (sql_str);
	} else {  // clusterad
		len = 100 + strlen(attNameList) + strlen(attValList);
		sql_str = (char *) malloc(len);

		snprintf(sql_str, len, "INSERT INTO ClusterAds_Horizontal %s VALUES %s", attNameList, attValList);

		if (DBObj->execCommand(sql_str) == FAILURE) {
			dprintf(D_ALWAYS, "JobQueueCollection::loadAd - ERROR [SQL] %s\n", sql_str);
			free(sql_str);
			free(attNameList);
			free(attValList);
			return FAILURE;
		}
		free (sql_str);
	}

	free(attNameList);
	free(attValList);

	return SUCCESS;
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
	 (strcasecmp(attr, "shadowbday") == 0) || 
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
	 (strcasecmp(attr, "jobuniverse") == 0) ||
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
	 (strcasecmp(attr, "shadowbday") == 0) || 
	 (strcasecmp(attr, "enteredcurrentstatus") == 0) || 
	 (strcasecmp(attr, "numrestarts") == 0) || 
	 (strcasecmp(attr, "remotehost") == 0)) {
    return true;
  }
     
  return false;
}

static int 
typeOf(char *attName)
{
	if (!(strcasecmp(attName, "remotehost") && 
		  strcasecmp(attName, "globaljobid") &&
		  strcasecmp(attName, "args") && 
		  strcasecmp(attName, "owner") && 
		  strcasecmp(attName, "cmd"))
		)
		return CONDOR_TT_TYPE_STRING;

	if (!(strcasecmp(attName, "jobstatus") && 
		  strcasecmp(attName, "imagesize") &&
		  strcasecmp(attName, "remoteusercpu") && 
		  strcasecmp(attName, "remotewallclocktime") &&
		  strcasecmp(attName, "jobuniverse") &&
		  strcasecmp(attName, "numrestarts") &&
		  strcasecmp(attName, "jobprio"))
		)
		return CONDOR_TT_TYPE_NUMBER;

	if (!(strcasecmp(attName, "qdate") &&
		  strcasecmp(attName, "shadowbday") &&
		  strcasecmp(attName, "enteredcurrentstatus"))
		)
		return CONDOR_TT_TYPE_TIMESTAMP;

	return -1;
}
