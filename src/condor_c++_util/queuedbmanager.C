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
#include "condor_io.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "jqmond_dbschema_def.h"
#include "condor_attributes.h"

//#include "jobqueuedatabase.h"
//#include "pgsqldatabase.h"
#include "queuedbmanager.h"
#include "odbc.h"
#include "classad_collection.h"

#ifndef _DEBUG_
#define _DEBUG_
#endif

extern ODBC *DBObj;


//! constructor
QueueDBManager::QueueDBManager()
{
	// 
	// Ultimately this two string must be gotten from configuration file
	//
  initialized = FALSE;
  jobQueueDBConn = NULL;

  //
  // This is just for test setting
  //
#ifdef _REMOTE_DB_CONNECTION_
  //	jobQueueLogFile = strdup("/scratch/condor/spool/job_queue.log");
  printf( "storing remote database\n");
  //db_handle = new ODBC();
  //jobQueueDBConn = strdup("host=ad16.cs.wisc.edu port=5432 dbname=jqmon");
  //	jobQueueDBConn = strdup("host=nighthawk.cs.wisc.edu dbname=jqmon");
#else
  jobQueueDBConn = strdup("dbname=jqmon");
#endif
  
#ifdef _POSTGRESQL_DBMS_
  //jqDatabase = new PGSQLDatabase(jobQueueDBConn);
#endif
  
  xactState = NOT_IN_XACT;
  
  addingCount = 0; 
  
  multi_sql_str = NULL;
}

//! destructor
QueueDBManager::~QueueDBManager()
{
  // release Objects
  //if (jqDatabase != NULL)
  //	delete jqDatabase;
  
  initialized = FALSE;
  // release strings
  if (jobQueueDBConn != NULL)
    free(jobQueueDBConn);
  if (multi_sql_str != NULL)
    free(multi_sql_str);
  //delete db_handle;
}

//**************************************************************************
// Methods
//**************************************************************************

/*! delete the current DB
 *  \return the result status
 *			1: Success
 *			0: Fail	(SQL execution fail)
*/	
int
QueueDBManager::cleanupJobQueueDB()
{
	int		sqlNum = 6;
	int		i;
	char 	sql_str[sqlNum][1024];

	// Need to vaccum in case of PostgreSQL 
	// However, VACCUM command should be invoked outside XACT
	sprintf(sql_str[0], 
		"DELETE FROM ClusterAds_Vertical;");
	sprintf(sql_str[1], 
		"DELETE FROM ClusterAds_Horizontal;");
	sprintf(sql_str[2], 
		"DELETE FROM ProcAds_Vertical;");
	sprintf(sql_str[3], 
		"DELETE FROM ProcAds_Horizontal;");
	sprintf(sql_str[4], 
		"DELETE FROM History_Vertical;");
	sprintf(sql_str[5], 
		"DELETE FROM History_Horizontal;");

	for (i = 0; i < sqlNum; i++) {
		if (DBObj->odbc_sqlstmt(sql_str[i]) < 0) {
			displayDBErrorMsg("Clean UP ALL Data --- ERROR");
			return 0; // return a error code, 0
		}
	}

	return 1;
}

/*! remove the delete rows in a database
 */
int
QueueDBManager::tuneupJobQueueDB()
{
	int		sqlNum = 1;
	int		i;
	char 	sql_str[sqlNum][1024];

	// Need to vaccum in case of PostgreSQL 
	sprintf(sql_str[0], 
		"VACUUM;");

	for (i = 0; i < sqlNum; i++) {
		if (DBObj->odbc_sqlstmt(sql_str[i]) < 0) {
			displayDBErrorMsg("VACUUM Database --- ERROR");
			return 0; // return a error code, 0
		}
	}

	return 1;
}


/*! connect to DBMS
 *  \return the result status
 *			1: Success
 * 			0: Fail (DB connection and/or Begin Xact fail)
 */	
int
QueueDBManager::connectDB(int  Xact)
{
   return 1;
  //db_handle->odbc_connect("condor", "scidb", ""); // connect to DB
  //return db_handle->isConnected();
  //return DBObj->isConnected();

  /*if (jqDatabase->connectDB(jobQueueDBConn) == 0) // connect to DB
    return 0;
    
    if (Xact == BEGIN_XACT) {
    if (jqDatabase->beginTransaction() == 0) // begin XACT
    return 0;
    }
    
    return 1;
  */
}


/*! disconnect from DBMS, and handle XACT (commit, abort, not in XACT)
 *  \param commit XACT command 
 *					0: non-Xact
 *					1: commit
 *					2: abort
 */
int
QueueDBManager::disconnectDB(int commit)
{
  return 1;
  //db_handle->odbc_disconnect();
  //return 0;  //since its a non-Xact see above function description

  /*if (commit == COMMIT_XACT) {
    if (xactState != BEGIN_XACT) {
    jqDatabase->commitTransaction(); // commit XACT
    xactState = NOT_IN_XACT;
    }
    } else if (commit == ABORT_XACT) { // abort XACT
    jqDatabase->rollbackTransaction();
    }
    
    jqDatabase->disconnectDB(); // disconnect from DB
    
    return 1;
  */
}


int
QueueDBManager::commitTransaction()
{
  /*if (xactState != BEGIN_XACT) {
    jqDatabase->commitTransaction(); // commit XACT
    xactState = NOT_IN_XACT;
    return 1;
    }
  */
  return 0;
}


/*
  int	
  QueueDBManager::findDeletedJobs(StaticHashTable *hash_table)
  {
  char* 	key = NULL;
  int		op_type;
  
  while ((op_type = caLogParser->readLogEntry()) > 0) {
  if (op_type == CondorLogOp_DestroyClassAd) {
  caLogParser->getDestroyClassAdBody(key);
  hash_table->insert((const char*)key);
  
  if (key != NULL) {
  free(key);
  key = NULL;
  }
  }
  }
  return 1;
  }
*/

/*! build the job queue collection from job_queue.log file
 */
/*
  int	
  QueueDBManager::buildJobQueue(JobQueueCollection *jobQueue)
  {
  int		op_type;
  
  while ((op_type = caLogParser->readLogEntry()) > 0) {
  if (processClassAdLogEntry(op_type, jobQueue) == 0) // process each ClassAd Log Entry
  return 0;
  }
  
  return 1;
  }
*/

/*! read and process ClassAd Log Entry
 */
/*
  int 
  QueueDBManager::read_proc_LogEntry()
  {
  int op_type = 0;
  
  // Process ClassAd Log Entry
  while ((op_type = caLogParser->readLogEntry()) > 0) {
  if (processClassAdLogEntry(op_type, false) == 0)
  return 0; // fail: need to abort Xact
  }
  
  return 1; // success
  }
*/

/*! process only DELTA
 */
/*
  int 
  QueueDBManager::addJobQueueDB()
  {
  int st;
  
  connectDB();
  
  caLogParser->setNextOffset();
  
  st = read_proc_LogEntry();
  
  // Store a polling information into DB
  if (st > 0)
  prober->setProbeInfo();
  
  if (st == 0) 
  disconnectDB(ABORT_XACT); // abort and end Xact
  else {
  // VACUUM should be called outside XACT
  // So, Commit XACT shouble be invoked beforehand.
  if (xactState != BEGIN_XACT) {
  jqDatabase->commitTransaction(); // end XACT
  xactState = NOT_IN_XACT;
  }
  
  if ((addingCount++ * pollingPeriod) > (60 * 60)) {
  tuneupJobQueueDB();
  addingCount = 0;
  }
  
  disconnectDB(NOT_IN_XACT); // commit and end Xact
  }
  
  return st;
  }
*/

int
QueueDBManager::processHistoryAd(ClassAd *ad) {
  
  int        cid, pid;
  ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cid);
  ad->EvalInteger (ATTR_PROC_ID, NULL, pid);
  
  char       sql_str1[409600];
  //char       sql_str2[409600];
  ExprTree *expr;
  ExprTree *L_expr;
  ExprTree *R_expr;
  char *value = NULL;
  char name[1000];
  bool flag1=false, flag2=false,flag3=false;

  sprintf(sql_str1, 
	  "INSERT INTO History_Horizontal(cid, pid) VALUES(%d, %d);", cid, pid);
  if (DBObj->odbc_sqlstmt(sql_str1) < 0) {
    displayDBErrorMsg("History Ad Processing --- ERROR");
    return 0; // return a error code, 0
  }

  //FILE *fp = fopen("/scratch/akini/historylogdump", "a");
  //fprintf(fp, "*************\n");
  //ad->fPrint(fp);
  //fprintf(fp, "*************\n");
  //if ( !fp ) {
  //  dprintf(D_ALWAYS,"ERROR saving to history file; cannot open history file\n");
  // return 0;
  //} 
  else {
    ad->ResetExpr(); // for iteration initialization
    while((expr=ad->NextExpr()) != NULL) {
    //while((expr = (AssignOpBase*)(ad->NextExpr())) != NULL) {
      strcpy(name, "");
      L_expr = expr->LArg();
      L_expr->PrintToStr(name);
      
      //nameExpr = (VariableBase*)expr->LArg(); // Name Express Tree
      
      R_expr = expr->RArg();
      value = NULL;
      R_expr->PrintToNewStr(&value);
      //valExpr = (StringBase*)expr->RArg();        // Value Express Tree
      //val = valExpr->Value();                                     // Value
      if (value == NULL) break;
  
      if(strcasecmp(name, "remotewallclocktime") == 0) {
        if(flag1) continue;
	flag1 = true;
      }
      else if(strcasecmp(name, "completiondate") == 0) {
	if(flag2) continue;
	flag2 = true;
      }
      else if(strcasecmp(name, "committedtime") == 0) {
        if(flag3) continue;
	flag3 = true;
      }

      if(isHorizontalHistoryAttribute(name)) {
	if(strcasecmp(name, "in") == 0 ||
	   strcasecmp(name, "user") == 0) {
	  strcat(name, "_j");
	}

	if(strcasecmp(name, "qdate") == 0 || 
	   strcasecmp(name, "lastmatchtime") == 0 || 
	   strcasecmp(name, "jobstartdate") == 0 || 
	   strcasecmp(name, "jobcurrentstartdate") == 0 ||
	   strcasecmp(name, "enteredcurrentstatus") == 0 ||
	   strcasecmp(name, "completiondate") == 0
	   ) {
	  sprintf(sql_str1, 
		  "UPDATE History_Horizontal SET %s = (('epoch'::timestamp + '%s seconds') at time zone 'UTC') WHERE cid = %d and pid = %d;", name, value, cid, pid);
	}
	else {
	  sprintf(sql_str1, 
		  "UPDATE History_Horizontal SET %s = '%s' WHERE cid = %d and pid = %d;", name, value, cid, pid);
	}	
      }
      else {
	sprintf(sql_str1, 
		"INSERT INTO History_Vertical(cid, pid, attr, val) VALUES(%d, %d, '%s', '%s');", cid, pid, name, value);
      }
      
      //dprintf(D_ALWAYS, "in the processHistoryClassAd before database write\n");
      if (DBObj->odbc_sqlstmt(sql_str1) < 0) {
	displayDBErrorMsg("History Ad Processing --- ERROR");
	//if(fp) fclose(fp);
	//return 0; // return a error code, 0
      }
      //dprintf(D_ALWAYS, "in the processHistoryClassAd after database write\n");
      //fprintf(fp, "%d\t%d\t%s\t%s\n", cid, pid, name, value);
      free(value);
    }
  }
  //if(fp) fclose(fp);
  
  return 1;
}




/*  handle classad entry
 *
 *  if exec_later == false: work directly with DBMS
 *  if exec_later == true : make a SQL which includes a multiple of commands
 *
 *  \param op_type commmand type
 *  \param exec_later execute it later?
 */
int 
QueueDBManager::processClassAdLogEntry(int op_type, bool exec_later)
{
	char *key, *mytype, *targettype, *name, *value;
	key = mytype = targettype = name = value = NULL;
	int	st = 1;

		// REMEMBER:
		//	each get*ClassAdBody() funtion allocates the memory of 
		// 	parameters. Therefore, they all must be deallocated here
	switch(op_type) {
	    case CondorLogOp_NewClassAd:
	      //			if (caLogParser->getNewClassAdBody(key, mytype, targettype) < 0)
	      //			return 0; 

			st = processNewClassAd(key, mytype, targettype, exec_later);
			
			break;
	    case CondorLogOp_DestroyClassAd:
	      //			if (caLogParser->getDestroyClassAdBody(key) < 0)
	      //			return 0;
			
			st = processDestroyClassAd(key, exec_later);
			
			break;
	    case CondorLogOp_SetAttribute:
	      //printf("SetAttributeII: key[%s],name[%s],value[%s]\n",key,name,value);
	      //			if (caLogParser->getSetAttributeBody(key, name, value) < 0)
	      //				return 0;
			//	printf("SetAttributeII: key[%s],name[%s],value[%s]\n",key,name,value);
			st = processSetAttribute(key, name, value, exec_later);
			
			break;
	    case CondorLogOp_DeleteAttribute:
	      //			if (caLogParser->getDeleteAttributeBody(key, name) < 0)
	      //				return 0;

			st = processDeleteAttribute(key, name, exec_later);
		
			break;
		case CondorLogOp_BeginTransaction:
		        st = processBeginTransaction();
			break;
		case CondorLogOp_EndTransaction:
			st = processEndTransaction();
			break;
	    default:
			printf( "Unsupported Job Queue Command is met [%d]\n", op_type);
		    return 0;
			break;
	}

		// pointers are release
	if (key != NULL) free(key);
	if (mytype != NULL) free(mytype);
	if (targettype != NULL) free(targettype);
	if (name != NULL) free(name);
	if (value != NULL) free(value);

	return st;
}

/*! display a error message 
*/
void
QueueDBManager::displayDBErrorMsg(const char* errmsg)
{
	printf( "[JQMOND] %s\n", errmsg);
	//printf( "\t%s\n", jqDatabase->getDBError());
}

/*! seprate a key into Cluster Id and Proc Id 
 *  \return key type 
 *			1: when it is a cluster id
 *			2: when it is a proc id
 * 			0: it fails
 *
 *	\warning The memories of cid and pid should be allocated in advance.
 */
int
QueueDBManager::getProcClusterIds(const char* key, char* cid, char* pid)
{
	int key_len, i;
	long iCid;
	char*	pid_in_key;

	if (key == NULL) 
		return 0;

	key_len = strlen(key);

	for (i = 0; i < key_len; i++) {
		if(key[i]  != '.')	
			cid[i]=key[i];
		else {
			cid[i] = '\0';
			break;
		}
	}

	// In case that the key doesn't include "."
	if (i == key_len)
		return 0; // Error

		// These two lines are for removing a leading zero.
	iCid = atol(cid);
	sprintf(cid,"%ld", iCid);


	pid_in_key = (char*)(key + (i + 1));
	strcpy(pid, pid_in_key);

	if (atol(pid) == -1) // Cluster ID
		return 1;	

	return 2; // Proc ID
}

/*! process NewClassAd command, working with DBMS
 *  \param key key
 *  \param mytype mytype
 *  \param ttype targettype
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 */
int 
QueueDBManager::processNewClassAd(const char* key, const char* mytype, const char* ttype, bool exec_later)
{
  char sql_str1[409600];
  char sql_str2[409600];
  char sql_str3[409600];
  char cid[512];
  char pid[512];
  int  id_sort;
  
  // Debugging purpose
  // printf("%d %s %s %s\n", CondorLogOp_NewClassAd, key, mytype, ttype);
  
  // It could be ProcAd or ClusterAd
  // So need to check
  id_sort = getProcClusterIds(key, cid, pid);
  
  switch(id_sort) {
  case 1:
    sprintf(sql_str1, 			
	    "INSERT INTO ClusterAds_Vertical (cid, attr, val) VALUES ('%s', 'MyType', '\"%s\"');", cid, mytype);
    
    sprintf(sql_str2, 
	    "INSERT INTO ClusterAds_Vertical (cid, attr, val) VALUES ('%s', 'TargetType', '\"%s\"');", cid, ttype);
    
    sprintf(sql_str3, 
	    "INSERT INTO ClusterAds_Horizontal (cid) VALUES ('%s');", cid);
    
    break;
  case 2:
    sprintf(sql_str1, 
	    "INSERT INTO ProcAds_Vertical (cid, pid, attr, val) VALUES ('%s', '%s', 'MyType', '\"Job\"');", cid, pid);
    
    sprintf(sql_str2, 
	    "INSERT INTO ProcAds_Vertical (cid, pid, attr, val) VALUES ('%s', '%s', 'TargetType', '\"Machine\"');", cid, pid);
    
    sprintf(sql_str3, 
	    "INSERT INTO ProcAds_Horizontal (cid, pid) VALUES ('%s', '%s');", cid, pid);

    break;
  case 0:
    printf( "New ClassAd Processing --- ERROR\n");
    return 0; // return a error code, 0
    break;
  }
  
  
  if (exec_later == false) { // execute them now
    if (DBObj->odbc_sqlstmt(sql_str1) < 0) {
      displayDBErrorMsg("New ClassAd Processing --- ERROR");
      return 0; // return a error code, 0
    }
    if (DBObj->odbc_sqlstmt(sql_str2) < 0) {
      displayDBErrorMsg("New ClassAd Processing --- ERROR");
      return 0; // return a error code, 0
    }
    if (DBObj->odbc_sqlstmt(sql_str3) < 0) {
      displayDBErrorMsg("New ClassAd Processing --- ERROR");
      return 0; // return a error code, 0
    }
  }
  else {
    if (multi_sql_str != NULL) { // append them to a SQL buffer
      multi_sql_str = (char*)realloc(multi_sql_str, 
				     strlen(multi_sql_str) + strlen(sql_str1) + strlen(sql_str2) + strlen(sql_str3) + 1);
      strcat(multi_sql_str, sql_str1);
      strcat(multi_sql_str, sql_str2);
      strcat(multi_sql_str, sql_str3);
    }
    else {
      multi_sql_str = (char*)malloc(
				    strlen(sql_str1) + strlen(sql_str2) + strlen(sql_str3) + 1);
      sprintf(multi_sql_str,"%s%s%s", sql_str1, sql_str2, sql_str3);
    }
  }		
  
  return 1;
}

/*! process DestroyClassAd command, working with DBMS
 *  \param key key
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 */
int 
QueueDBManager::processDestroyClassAd(const char* key, bool exec_later)
{
	char sql_str1[1024]; // sql for string table
	char sql_str2[1024]; // sql for number table
	char cid[1024];
	char pid[1024];
	int  id_sort;

// Debugging purpose
// printf("%d %s\n", CondorLogOp_DestroyClassAd, key);

		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);

	switch(id_sort) {
	case 1:	// ClusterAds
		sprintf(sql_str1, 
		"DELETE FROM ClusterAds_Vertical WHERE cid = '%s';", cid);

		sprintf(sql_str2, 
		"DELETE FROM ClusterAds_Horizontal WHERE cid = '%s';", cid);
		break;
	case 2:
		sprintf(sql_str1, 
		"DELETE FROM ProcAds_Vertical WHERE cid = '%s' AND pid = '%s';", 
				cid, pid);

		sprintf(sql_str2, 
		"DELETE FROM ProcAds_Horizontal WHERE cid = '%s' AND pid = '%s';", 
				cid, pid);
		break;
	case 0:
		fprintf(stdout, "[JQMON] Destroy ClassAd --- ERROR\n");
		return 0; // return a error code, 0
		break;
	}

	if (exec_later == false) { 
		if (DBObj->odbc_sqlstmt(sql_str1) < 0) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			return 0; // return a error code, 0
		}
		if (DBObj->odbc_sqlstmt(sql_str2) < 0) {
			displayDBErrorMsg("Destroy ClassAd Processing --- ERROR");
			return 0; // return a error code, 0
		}
	}
	else {
		if (multi_sql_str != NULL) {
			multi_sql_str = (char*)realloc(multi_sql_str, 
			strlen(multi_sql_str) + strlen(sql_str1) + strlen(sql_str2) + 1);
			strcat(multi_sql_str, sql_str1);
			strcat(multi_sql_str, sql_str2);
		}
		else {
			multi_sql_str = (char*)malloc(
				strlen(sql_str1) + strlen(sql_str2) + 1);
			sprintf(multi_sql_str, "%s%s", sql_str1, sql_str2);
		}
	}
		
	return 1;
}

/*! process SetAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \param value attribute value
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 *	Note:
 *	Because this is not just update, but set. So, we need to delete and insert
 *  it. 
 */
int 
QueueDBManager::processSetAttribute(const char* key, const char* name, const char* value, bool exec_later)
{
	char sql_str_up[409600];
	char sql_str_in[409600];
	char sql_str_del_in[819200];
	char cid[512];
	char pid[512];
	int  id_sort;
	char tempvalue[1000];
	//bool ishorizontal = false;
	//int		ret_st;

	memset(sql_str_up, 0, 409600);
	memset(sql_str_in, 0, 409600);

#ifdef _DEBUG_LOG_ENTRY
	printf("%d %s %s %s\n", CondorLogOp_SetAttribute, key, name, value);
#endif

		// It could be ProcAd or ClusterAd
		// So need to check
	id_sort = getProcClusterIds(key, cid, pid);

	switch(id_sort) {
	case 1:
	  if(isHorizontalClusterAttribute(name)) {
	    if (strcasecmp(name, "user") == 0) {
	      strncpy(tempvalue, value+1, strlen(value)-2);
	      tempvalue[strlen(value)-2] = '\0';
	      sprintf(sql_str_del_in, 
		      "UPDATE ClusterAds_Horizontal SET %s_j = '%s' WHERE cid = '%s';", name, tempvalue, cid);
	    }

	    else if(strcasecmp(name, "qdate") == 0) {
	      sprintf(sql_str_del_in, 
		      "UPDATE ClusterAds_Horizontal SET %s = (('epoch'::timestamp + '%s seconds') at time zone 'UTC') WHERE cid = '%s';", name, value, cid);
	    }
	    else {
	      sprintf(sql_str_del_in, 
		      "UPDATE ClusterAds_Horizontal SET %s = '%s' WHERE cid = '%s';", name, value, cid);
	    }
	  }
	  else {
	    sprintf(sql_str_del_in, 
		    "DELETE FROM ClusterAds_Vertical WHERE cid = '%s' AND attr = '%s'; INSERT INTO ClusterAds_Vertical (cid, attr, val) VALUES ('%s', '%s', '%s');", cid, name, cid, name, value);
	  }
	  break;
	case 2:
	  if(isHorizontalProcAttribute(name)) {
	    sprintf(sql_str_del_in, 
		    "UPDATE ProcAds_Horizontal SET %s = '%s' WHERE cid = '%s' and pid = '%s';", name, value, cid, pid);
	  }
	  else {
	    sprintf(sql_str_del_in, 
		    "DELETE FROM ProcAds_Vertical WHERE cid = '%s' AND pid = '%s' AND attr = '%s'; INSERT INTO ProcAds_Vertical (cid, pid, attr, val) VALUES ('%s', '%s', '%s', '%s');", cid, pid, name, cid, pid, name, value);
	  }
	  break;
	case 0:
	  printf( "Set Attribute Processing --- ERROR\n");
	  return 0;
	  break;
	}
	
	
	int ret_st = 0;
	if (exec_later == false) {
	  ret_st = DBObj->odbc_sqlstmt(sql_str_del_in);
	  
	  if (ret_st < 0) {
	    fprintf(stdout, "[SQL] %s\n", sql_str_del_in);
	    displayDBErrorMsg("Set Attribute --- ERROR");
	    return 0;
	  }
	}
	else {
	  if (multi_sql_str != NULL) {
	    // NOTE:
	    // this case is not trivial 
	    // because there could be multiple insert
	    // statements.
	    multi_sql_str = (char*)realloc(multi_sql_str, 
					   strlen(multi_sql_str) + strlen(sql_str_del_in) + 1);
	    strcat(multi_sql_str, sql_str_del_in);
	  }
	  else {
	    multi_sql_str = (char*)malloc(
					  strlen(sql_str_del_in) + 1);
	    strcpy(multi_sql_str, sql_str_del_in);
	  }
	  
	  
	}

	return 1;
}


/*! process DeleteAttribute command, working with DBMS
 *  \param key key
 *  \param name attribute name
 *  \param exec_later send SQL into RDBMS now or not?
 *  \return the result status
 *			0: error
 *			1: success
 */
int 
QueueDBManager::processDeleteAttribute(const char* key, const char* name, bool exec_later)
{
  //char sql_str1[409600];
  char sql_str[409600];
  char cid[512];
  char pid[512];
  int  id_sort;
  int  ret_st;
  
  memset(sql_str, 0, 409600);
  
  // Debugging purpose
  // printf("%d %s %s\n", CondorLogOp_DeleteAttribute, key, name);
  
  // It could be ProcAd or ClusterAd
  // So need to check
  id_sort = getProcClusterIds(key, cid, pid);
  
  switch(id_sort) {
  case 1:
    if(isHorizontalClusterAttribute(name)) {
      sprintf(sql_str , 
	      "UPDATE ClusterAds_Horizontal SET %s = NULL WHERE cid = '%s';", name, cid);
    }
    else {
      sprintf(sql_str , 
	      "DELETE ClusterAds_Vertical WHERE cid = '%s' AND attr = '%s';", cid, name);
    }
    
    break;
  case 2:
    if(isHorizontalProcAttribute(name)) {
      sprintf(sql_str, 
	      "UPDATE ProcAds_Horizontal SET %s = NULL WHERE cid = '%s' AND pid = '%s';", name, cid, pid);
    }
    else {
      sprintf(sql_str, 
	      "DELETE FROM ProcAds_Vertical WHERE cid = '%s' AND pid = '%s' AND attr = '%s';", cid, pid, name);
    }
    break;
  case 0:
    printf( "Delete Attribute Processing --- ERROR\n");
    return 0;
    break;
  }
  
  if (sql_str != NULL) {
    if (exec_later == false) {
      ret_st = DBObj->odbc_sqlstmt(sql_str);
      
      if (ret_st < 0) {
	fprintf(stdout, "[SQL] %s\n", sql_str);
	displayDBErrorMsg("Delete Attribute --- ERROR");
	return 0;
      }
    }
    else {
      if (multi_sql_str != NULL) {
	multi_sql_str = (char*)realloc(multi_sql_str, 
				       strlen(multi_sql_str) + strlen(sql_str) + 1);
	strcat(multi_sql_str, sql_str);
      }
      else {
	multi_sql_str = (char*)malloc(
				      strlen(sql_str) + 1);
	sprintf(multi_sql_str, "%s", sql_str);
      }
    }		
  }
  
  return 1;
}

/*! process BeginTransaction command
 *  \return the result status
 *			1: success
 */
int 
QueueDBManager::processBeginTransaction()
{
// Debugging purpose
//printf("%d\n", CondorLogOp_BeginTransaction);
	xactState = BEGIN_XACT;
	return 1;
}

/*! process EndTransaction command
 *  \return the result status
 *			1: success
 */
int 
QueueDBManager::processEndTransaction()
{
// Debugging purpose
//printf("%d\n", CondorLogOp_EndTransaction);
	xactState = COMMIT_XACT;
	return 1;
}

//! initialize: currently check the DB schema
/*! \param initJQDB initialize DB?
 */
int
QueueDBManager::init(bool initJQDB)
{
  initialized = TRUE;
  return 1;
  /*if (initJQDB == true) { // initialize Job Queue DB
    if (checkSchema() == 0)
      return 0;
    //prober->probe();
    //return initJobQueueDB();
    return 1;
  }
  else
    return checkSchema();
  */
}


/*! check the DB schema
 *  \return the result status
 *			0: error
 *			1: success
 */
int
QueueDBManager::checkSchema()
{
  /*
  char 	sql_str[409600]; 
  int		ret_st;
  
  //
  // DB schema check should be done here 
  //
  // 1. check the number of tables 
  // 2. check the list of tables
  // 3. check the one tuple of JobQueuePollingInfo table 
  connectDB(NOT_IN_XACT);
  
  strcpy(sql_str, SCHEMA_CHECK_STR); // SCHEMA_CHECK_STR is defined 
  // in jqmond_global.h
  // execute DB schema check!
  ret_st = jqDatabase->execQuery(sql_str);
  
  if (ret_st == SCHEMA_SYS_TABLE_NUM) {
    dprintf(D_ALWAYS, "Schema Check OK!\n");
    disconnectDB(NOT_IN_XACT);
  }
  else if (ret_st == 0) { // Schema is not defined in DB
    dprintf(D_ALWAYS,"Schema is not defined!\n");
    dprintf(D_ALWAYS,"Create DB Schema for jqmond!\n");
    if (jqDatabase->beginTransaction() == 0) // this conn is not in Xact,
      return 0;			   				 // so begin Xact!
    
    //
    // Here, Create DB Schema:
    //
    strcpy(sql_str, SCHEMA_CREATE_PROCADS_TABLE_STR);
    ret_st = jqDatabase->execCommand(sql_str);
    if(ret_st < 0) {
      disconnectDB(ABORT_XACT);
      return 0;
    }
    
    strcpy(sql_str, SCHEMA_CREATE_CLUSTERADS_TABLE_STR);
    ret_st = jqDatabase->execCommand(sql_str);
    if(ret_st < 0) {
      disconnectDB(ABORT_XACT);
      return 0;
    }
    
    strcpy(sql_str, SCHEMA_CREATE_HISTORY_TABLE_STR);
    ret_st = jqDatabase->execCommand(sql_str);
    if(ret_st < 0) {
      disconnectDB(ABORT_XACT);
      return 0;
    }
    
    disconnectDB(COMMIT_XACT);		
  }
  else { // Unknown error
    dprintf(D_ALWAYS,"Schema Check Unknown Error!\n");
    disconnectDB(NOT_IN_XACT);
    
    return 0;
  }
  */
  return 1;
}


