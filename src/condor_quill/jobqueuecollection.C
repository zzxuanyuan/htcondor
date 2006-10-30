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
#include "jobqueuecollection.h"
#include "jobqueuedbmanager.h"

#undef ATTR_VERSION
#include "oracledatabase.h"

//! constructor
JobQueueCollection::JobQueueCollection(int iBucketNum)
{
	int i;

	_iBucketSize = iBucketNum;
	typedef ClassAdBucket* ClassAdBucketPtr;
	_ppProcAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	_ppClusterAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	_ppHistoryAdBucketList = new ClassAdBucketPtr[_iBucketSize];
	for (i = 0; i < _iBucketSize; i++) {
		_ppProcAdBucketList[i] = NULL;
		_ppClusterAdBucketList[i] = NULL;
		_ppHistoryAdBucketList[i] = NULL;
	}

	procAdNum = clusterAdNum = historyAdNum = 0;
	curClusterAdIterateIndex = 0;
	curProcAdIterateIndex = 0;
	curHistoryAdIterateIndex = 0;

	pCurBucket = NULL;
	bChained = false;
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
		pCurBucket = _ppHistoryAdBucketList[i];
		while (pCurBucket != NULL) {
			pPreBucket = pCurBucket;
			pCurBucket = pCurBucket->pNext;
			delete pPreBucket;
		}
	}

	delete[] _ppClusterAdBucketList;
	delete[] _ppProcAdBucketList;
	delete[] _ppHistoryAdBucketList;
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
		id = (char*)malloc(cid_len + pid_len + 2);
		sprintf(id,"%s%s", pid,cid);
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

//! insert a History Ad
/*! \param cid Cluster Id
 *  \param pid Proc Id
 *  \parm historyAd HistoryAd to be inserted
 *  \return the result status of insert
 */
int 
JobQueueCollection::insertHistoryAd(char* cid, char* pid, ClassAd* historyAd)
{
	int st;
	char* id = (char*)malloc(strlen(cid) + strlen(pid) + 2);

	ClassAdBucket *pBucket = new ClassAdBucket(cid, pid, historyAd);

	sprintf(id, "%s%s",pid,cid);
	st = insert(id, pBucket, _ppHistoryAdBucketList);
	if (st > 0) {
		++historyAdNum;
	}
	
	free(id); // id is just used for hashing purpose, 
			  // so it must be freed here.
	return st;
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
	char* id = (char*)malloc(strlen(cid) + strlen(pid) + 2);

	ClassAdBucket *pBucket = new ClassAdBucket(cid, pid, procAd);

	sprintf(id, "%s%s",pid,cid);
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
		id = (char*)malloc(cid_len + pid_len + 2);
		sprintf(id,"%s%s", pid, cid);
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
}

//! initialize all job ads iteration
void
JobQueueCollection::initAllHistoryAdsIteration()
{
	curHistoryAdIterateIndex = 0; // that of HistoryAd List
	pCurBucket = NULL;
	bChained = false;
}

//! load next history ad
bool
JobQueueCollection::loadNextHistoryAd(QuillErrCode &errStatus)
{
	// index is greater than the last index of bucket list?
	// we cant call it quits if there's another chained ad after this
	// and its the last bucket -- ameet
	if (curHistoryAdIterateIndex == _iBucketSize && !bChained) {
	  return FALSE;
	}
		 
	if (bChained == false) { 
	  pCurBucket = _ppHistoryAdBucketList[curHistoryAdIterateIndex++];
	  
	  while (pCurBucket == NULL) {
	    if (curHistoryAdIterateIndex == _iBucketSize) {
	      return FALSE;
	    }
	    pCurBucket = _ppHistoryAdBucketList[curHistoryAdIterateIndex++];
	  }
	} 
	else // we are following the chained buckets
	  pCurBucket = pCurBucket->pNext;
	
	// is there a chained bucket?
	if (pCurBucket->pNext != NULL) {
	  bChained = true;
	}
	else {
	  bChained = false;
	}
	
	// making a COPY string for this ClassAd
	errStatus = loadHistoryAd(pCurBucket->cid, 
							  pCurBucket->pid, 
							  pCurBucket->ad);
		
	return TRUE;
}

QuillErrCode
JobQueueCollection::loadHistoryAd(char* cid, char* pid, ClassAd* ad)
{
	char* 	val = NULL;
	char*   sql_str;

	AssignOp*	expr;		// For Each Attribute in ClassAd
	Variable* 	nameExpr;	// For Attribute Name
	String* 	valExpr;	// For Value
	int len;

	// The below two attributes should be inserted into 
	//the SQL String for the vertical component
	// MyType = "Job"
	// TargetType = "Machine"

	len = MAX_FIXED_SQL_STR_LENGTH + 2*strlen(cid) + 2*strlen(pid);

	sql_str = (char *) malloc(len);

	snprintf(sql_str, len, 
			 "INSERT INTO History_Vertical(cid,pid,attr,val) "
			 "SELECT %s,%s,'MyType','\"Job\"' FROM DUAL WHERE NOT EXISTS"
			 "(SELECT cid,pid FROM History_Vertical WHERE cid=%s "
			 "AND pid=%s)",
			 cid, pid, cid, pid);

	if (DBObj->execCommand(sql_str) == FAILURE) {
		free(sql_str);
		return FAILURE;
	}

	snprintf(sql_str, len, 
			 "INSERT INTO History_Vertical(cid,pid,attr,val) "
			 "SELECT %s,%s,'TargetType','\"Machine\"' FROM DUAL WHERE NOT EXISTS"
			 "(SELECT cid,pid FROM History_Vertical WHERE cid=%s AND pid=%s)",
			 cid,pid,cid,pid);

	if (DBObj->execCommand(sql_str) == FAILURE) {
		free(sql_str);
		return FAILURE;
	}
		
	// creating a new horizontal string consisting of one insert 
	// and many updates
	snprintf(sql_str, len,
			"INSERT INTO History_Horizontal(cid,pid,\"EnteredHistoryTable\") "
			"SELECT %s,%s,current_timestamp FROM DUAL WHERE NOT EXISTS(SELECT cid,pid "
			"FROM History_Horizontal WHERE cid=%s AND pid=%s)", 
			cid,pid,cid,pid);

	if (DBObj->execCommand(sql_str) == FAILURE) {
		free(sql_str);
		return FAILURE;
	}

	free(sql_str);

	ad->ResetExpr(); // for iteration initialization

	while((expr = (AssignOp*)(ad->NextExpr())) != NULL) {
	  nameExpr = (Variable*)expr->LArg(); // Name Express Tree
	  valExpr = (String*)expr->RArg();	// Value Express Tree
	  val = valExpr->Value();	      		// Value
	  if (val == NULL) {
		  break;
	  }

	  val = JobQueueDBManager::fillEscapeCharacters(val);
	  
	  
	  // make a SQL line for each attribute
	  //line_str = (char*)malloc(strlen(nameExpr->Name()) 
	  //			   + strlen(val) + strlen(cid) 
	  //			   + strlen(pid) + strlen("\t\t\t\n") + 1);
	  if(strcmp(nameExpr->Name(),"ClusterId") == 0 ||
	     strcmp(nameExpr->Name(),"ProcId") == 0) {
		free(val);
	    continue;
	  }
	  else if(strcmp(nameExpr->Name(),"QDate") == 0 ||
		  strcmp(nameExpr->Name(),"RemoteWallClockTime") == 0 ||
		  strcmp(nameExpr->Name(),"RemoteUserCpu") == 0 ||
		  strcmp(nameExpr->Name(),"RemoteSysCpu") == 0 ||
		  strcmp(nameExpr->Name(),"ImageSize") == 0 ||
		  strcmp(nameExpr->Name(),"JobStatus") == 0 ||
		  strcmp(nameExpr->Name(),"JobPrio") == 0 ||
		  strcmp(nameExpr->Name(),"CompletionDate") == 0) {

		  len = MAX_FIXED_SQL_STR_LENGTH + 2*strlen(nameExpr->Name()) +
			  strlen(cid) + strlen(pid) + strlen(val);

		  sql_str = (char *) malloc(len);
		  
		  snprintf(sql_str, len,
				"UPDATE History_Horizontal SET \"%s\" = %s "
				"WHERE cid=%s AND pid=%s AND \"%s\" IS NULL",  
				nameExpr->Name(), val,cid,pid,nameExpr->Name());
		  
		  if (DBObj->execCommand(sql_str) == FAILURE) {
			  free(sql_str);
			  free(val);
			  return FAILURE;
		  }
		  free(sql_str);
	  }

	  else if(strcmp(nameExpr->Name(),"Owner") == 0 ||
		  strcmp(nameExpr->Name(),"Cmd") == 0 ||
		  strcmp(nameExpr->Name(),"LastRemoteHost") == 0) {

		  len = MAX_FIXED_SQL_STR_LENGTH + 2*strlen(nameExpr->Name()) +
			  strlen(cid) + strlen(pid) + strlen(val);

		  sql_str = (char *) malloc(len);
		  
		  snprintf(sql_str, len,
				"UPDATE History_Horizontal SET \"%s\" = '%s' "
				"WHERE cid=%s AND pid=%s AND \"%s\" IS NULL",  
				nameExpr->Name(), val,cid,pid, nameExpr->Name());

		  if (DBObj->execCommand(sql_str) == FAILURE) {
			  free(sql_str);
			  free(val);
			  return FAILURE;
		  }
		  free(sql_str);		  
	  }
	  else {
		  len = MAX_FIXED_SQL_STR_LENGTH + 2*strlen(nameExpr->Name()) +
			  strlen(val) + 2*strlen(cid) + 2*strlen(pid);

		  sql_str = (char *) malloc(len);
		  
		  snprintf(sql_str, len,
				   "INSERT INTO History_Vertical(cid,pid,attr,val) "
				 "SELECT %s,%s,'%s','%s' FROM DUAL WHERE NOT EXISTS(SELECT cid,pid FROM "
				   "History_Vertical where cid=%s and pid=%s and attr='%s')",  
				   cid, pid, nameExpr->Name(), val,cid,pid,nameExpr->Name());

		  if (DBObj->execCommand(sql_str) == FAILURE) {
			  free(sql_str);
			  free(val);
			  return FAILURE;
		  }
		  free(sql_str);		 		  
	  }
	  
	  free(val);
	}

	return SUCCESS;
}



// load the next cluster ad
bool
JobQueueCollection::loadNextClusterAd(QuillErrCode &errStatus)
{
	return loadNextAd(curClusterAdIterateIndex, 
					 _ppClusterAdBucketList, 
					 errStatus);
}

// load the next proc ad
bool
JobQueueCollection::loadNextProcAd(QuillErrCode &errStatus)
{
	return loadNextAd(curProcAdIterateIndex, 
					 _ppProcAdBucketList, 
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
	errStatus = loadAd( pCurBucket->cid, 
						pCurBucket->pid, 
						pCurBucket->ad);
	
	return TRUE;
}

QuillErrCode 
JobQueueCollection::loadAd(char* cid, 
						   char* pid, 
						   ClassAd* ad)
{
	char*   sql_str = NULL;
	char* 	endptr = NULL;
	char* 	val = NULL;

	const int 	NUM_TYPE = 1;
	const int	OTHER_TYPE = 2;	
	int  		value_type; // 1: Number, 
	double      doubleval = 0;

	AssignOp*	expr;		// For Each Attribute in ClassAd
	Variable* 	nameExpr;	// For Attribute Name
	String* 	valExpr;	// For Value
	int len;

		// The below two attributes should be inserted into tables
		// MyType = "Job"
		// TargetType = "Machine"
	if (pid != NULL) {
		len = MAX_FIXED_SQL_STR_LENGTH + strlen(cid) + strlen(pid);

		sql_str = (char *) malloc(len);

		snprintf(sql_str, len, 
				 "INSERT INTO Procads_Str (CID, PID, ATTR, VAL) VALUES (%s, %s, 'MyType', 'Job')", cid, pid);

		if (DBObj->execCommand(sql_str) == FAILURE) {
			free(sql_str);
			return FAILURE;
		}

		snprintf(sql_str, len, 
				 "INSERT INTO Procads_Str (CID, PID, ATTR, VAL) VALUES (%s, %s, 'TargetType', 'Machine')", cid, pid);

		if (DBObj->execCommand(sql_str) == FAILURE) {
			free(sql_str);
			return FAILURE;
		}		

		free (sql_str);	
	} else {	
		len = MAX_FIXED_SQL_STR_LENGTH + strlen(cid);
		sql_str = (char *) malloc(len);
		
		snprintf(sql_str, len, 
				 "INSERT INTO Clusterads_Str (CID, ATTR, VAL) VALUES (%s, 'MyType', 'Job')", cid);


		if (DBObj->execCommand(sql_str) == FAILURE) {
			free(sql_str);
			return FAILURE;
		}

		snprintf(sql_str, len, 
				 "INSERT INTO Clusterads_Str (CID, ATTR, VAL) VALUES (%s, 'TargetType', 'Machine')", cid);

		if (DBObj->execCommand(sql_str) == FAILURE) {
			free(sql_str);
			return FAILURE;
		}		

		free (sql_str);		   
	}

	ad->ResetExpr(); // for iteration initialization

	while((expr = (AssignOp*)(ad->NextExpr())) != NULL) {

		nameExpr = (Variable*)expr->LArg(); // Name Express Tree
		valExpr = (String*)expr->RArg();	// Value Express Tree
		val = valExpr->Value();					// Value
		if (val == NULL) break;
		
		doubleval = strtod(val, &endptr);
		if(val == endptr) {
			value_type = OTHER_TYPE;
		}
 
		else {
			if(*endptr != '\0') {
				value_type = OTHER_TYPE;
			}
			else {
				value_type = NUM_TYPE;
			}
		}

		val = JobQueueDBManager::fillEscapeCharacters(val);

		if (value_type == OTHER_TYPE) {
				// load into *_STR tables		   
			if (pid != NULL) { // ProcAd	

				len = MAX_FIXED_SQL_STR_LENGTH + strlen(nameExpr->Name()) +
					strlen(val) + strlen(cid) + strlen(pid);

				sql_str = (char*)malloc(len);
				
				snprintf(sql_str, len, 
						 "INSERT INTO Procads_Str (CID, PID, ATTR, VAL) VALUES (%s, %s, '%s', '%s')", cid, pid, nameExpr->Name(), val);

				if (DBObj->execCommand(sql_str) == FAILURE) {
					free(sql_str);
					free(val);
					return FAILURE;
				}

				free(sql_str);
			} 
			else { // ClusterAd

				len = MAX_FIXED_SQL_STR_LENGTH + strlen(nameExpr->Name()) +
					strlen(val) + strlen(cid);

				sql_str = (char*)malloc(len);
				
				snprintf(sql_str, len, 
						 "INSERT INTO Clusterads_Str (CID, ATTR, VAL) VALUES (%s, '%s', '%s')", cid, nameExpr->Name(), val);

				if (DBObj->execCommand(sql_str) == FAILURE) {
					free(sql_str);
					free(val);
					return FAILURE;
				}
				
				free(sql_str);
			}
		} else {
				// load into *_NUM tables 
			if (pid != NULL) { // ProcAd	

				len = MAX_FIXED_SQL_STR_LENGTH + strlen(nameExpr->Name()) +
					strlen(val) + strlen(cid) + strlen(pid);

				sql_str = (char*)malloc(len);
				
				snprintf(sql_str, len, 
						 "INSERT INTO Procads_Num (CID, PID, ATTR, VAL) VALUES (%s, %s, '%s', %s)", cid, pid, nameExpr->Name(), val);

				if (DBObj->execCommand(sql_str) == FAILURE) {
					free(sql_str);
					free(val);
					return FAILURE;
				}

				free(sql_str);
			} 
			else { // ClusterAd

				len = MAX_FIXED_SQL_STR_LENGTH + strlen(nameExpr->Name()) +
					strlen(val) + strlen(cid);

				sql_str = (char*)malloc(len);
				
				snprintf(sql_str, len, 
						 "INSERT INTO Clusterads_Num (CID, ATTR, VAL) VALUES (%s, '%s', %s)", cid, nameExpr->Name(), val);

				if (DBObj->execCommand(sql_str) == FAILURE) {
					free(sql_str);
					free(val);
					return FAILURE;
				}
				
				free(sql_str);
			}
		}
		
		free(val);
	}
	
	return SUCCESS;
}

void JobQueueCollection::setDBObj(JobQueueDatabase *DBObj)
{
	this->DBObj = DBObj;
}
