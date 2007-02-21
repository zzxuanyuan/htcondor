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
#include <string.h>
#include "condor_config.h"
#include "dbms_utils.h"
#include "condor_attributes.h"
#include "MyString.h"

extern "C" {

//! Gets the writer password required by the quill++
//  daemon to access the database
static char * getWritePassword(char *write_passwd_fname, 
							   const char *host, const char *port, 
							   const char *db,
							   const char *dbuser) {
	FILE *fp = NULL;
	char *passwd = (char *) malloc(64 * sizeof(char));
	int len;
	char *prefix;
	MyString *msbuf = 0;
	const char *buf;
	bool found = FALSE;

		// prefix is for the prefix of the entry in the .pgpass
		// it is in the format of the following:
		// host:port:db:user:password
	len = 10+strlen(host) + strlen(port) + strlen(db) + strlen(dbuser);

	prefix = (char  *) malloc (len * sizeof(char));

	snprintf(prefix, len, "%s:%s:%s:%s:", host, port, db, dbuser);

	len = strlen(prefix);

	fp = safe_fopen_wrapper(write_passwd_fname, "r");

	if(fp == NULL) {
		EXCEPT("Unable to open password file %s\n", write_passwd_fname);
	}
	
		//dprintf(D_ALWAYS, "prefix: %s\n", prefix);

	msbuf = new MyString();
	while(msbuf->readLine(fp, true)) {
		buf = msbuf->Value();

			//dprintf(D_ALWAYS, "line: %s\n", buf);

			// check if the entry matches the prefix
		if (strncmp(buf, prefix, len) == 0) {
				// extract the password
			strncpy(passwd, &buf[len], 64);
			delete msbuf;
			found = TRUE;
			
				// remove the new line in the end
			if (passwd[strlen(passwd)-1] == '\n') {
				passwd[strlen(passwd)-1] = '\0';
			}

			break;
		}

		delete msbuf;
		msbuf = new MyString();
	}

    fclose(fp);
	if (!found) {
		EXCEPT("Unable to find password from file %s\n", write_passwd_fname);
	}

	return passwd;
}

dbtype getConfigDBType() 
{
	char *tmp;
	dbtype dt;

	tmp = param("QUILL_DB_TYPE");
	if (tmp) {
		if (strcasecmp(tmp, "ORACLE") == 0) {
			dt = T_ORACLE;
		} else if (strcasecmp(tmp, "PGSQL") == 0) {
			dt = T_PGSQL;
		}
		free (tmp);
	} else {
		dt = T_PGSQL; // assume PGSQL by default
	}
	
	return dt;
}

char *getDBConnStr(
const char* jobQueueDBIpAddress,
const char* jobQueueDBName,
const char* jobQueueDBUser,
const char* spool
)
{
	char *host = NULL, *port = NULL;
	char *writePassword;
	char *writePasswordFile;
	int len, tmp1, tmp2, tmp3;
	char *jobQueueDBConn;

	ASSERT(jobQueueDBIpAddress);
	ASSERT(jobQueueDBName);
	ASSERT(jobQueueDBUser);
	ASSERT(spool);

		//parse the ip address and get host and port
	len = strlen(jobQueueDBIpAddress);
	host = (char *) malloc(len * sizeof(char));
	port = (char *) malloc(len * sizeof(char));
	
		//split the <ipaddress:port> into its two parts accordingly
	char *ptr_colon = strchr(jobQueueDBIpAddress, ':');
	strncpy(host, jobQueueDBIpAddress, 
			ptr_colon - jobQueueDBIpAddress);
		// terminate the string properly
	host[ptr_colon - jobQueueDBIpAddress] = '\0';
	strncpy(port, ptr_colon+1, len);
		// terminate the string properyly
	port[strlen(ptr_colon+1)] = '\0';

		// get the password from the .pgpass file
	writePasswordFile = (char *) malloc(_POSIX_PATH_MAX * sizeof(char));
	snprintf(writePasswordFile, _POSIX_PATH_MAX, "%s/.pgpass", spool);

	writePassword = getWritePassword(writePasswordFile, 
										   host?host:"", port?port:"", 
										   jobQueueDBName?jobQueueDBName:"", 
										   jobQueueDBUser);

	free(writePasswordFile);

	tmp1 = jobQueueDBName?strlen(jobQueueDBName):0;
	tmp2 = strlen(writePassword);
	
		//tmp3 is the size of dbconn - its size is estimated to be
		//(2 * len) for the host/port part, tmp1 + tmp2 for the
		//password and dbname part and 1024 as a cautiously
		//overestimated sized buffer
	tmp3 = (2 * len) + tmp1 + tmp2 + 1024;

	jobQueueDBConn = (char *) malloc(tmp3 * sizeof(char));

	snprintf(jobQueueDBConn, tmp3, 
			 "host=%s port=%s user=%s password=%s dbname=%s", 
			 host?host:"", port?port:"", 
			 jobQueueDBUser?jobQueueDBUser:"", 
			 writePassword?writePassword:"", 
			 jobQueueDBName?jobQueueDBName:"");

	free(writePassword);
	
	if(host) {
		free(host);
		host = NULL;
	}

	if(port) {
		free(port);
		port = NULL;
	}
	
	return jobQueueDBConn;
}

bool stripdoublequotes(char *attVal) {
	int attValLen;

	if (!attVal) {
		return FALSE;
	}

	attValLen = strlen(attVal);

		/* strip enclosing double quotes
		*/
	if (attVal[attValLen-1] == '"' && attVal[0] == '"') {
		strncpy(attVal, &attVal[1], attValLen-2);
		attVal[attValLen-2] = '\0';
		return TRUE;	 
	} else {
		return FALSE;
	}	
}

bool stripdoublequotes_MyString(MyString &value) {
	int attValLen;

	if (value.IsEmpty()) {
		return FALSE;
	}
	
	attValLen = value.Length();

		/* strip enclosing double quotes
		*/
	if (value[attValLen-1] == '"' && value[0] == '"') {
		value = value.Substr(1, attValLen-2);
		return TRUE;
	} else {
		return FALSE;
	}
}

/* The following attributes are treated as horizontal daemon attributes.
   Notice that if you add more attributes to the following list, the horizontal
   schema for Daemons_Horizontal needs to revised accordingly.
*/
bool isHorizontalDaemonAttr(
char *attName, 
QuillAttrDataType &attr_type) {
	if (!(strcasecmp(attName, ATTR_NAME) &&
		  strcasecmp(attName, ATTR_UPDATESTATS_HISTORY))) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!(strcasecmp(attName, "monitorselfcpuusage") &&
				 strcasecmp(attName, "monitorselfimagesize") && 
				 strcasecmp(attName, "monitorselfresidentsetsize") && 
				 strcasecmp(attName, "monitorselfage") &&
				 strcasecmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) && 
				 strcasecmp(attName, ATTR_UPDATESTATS_TOTAL) &&
				 strcasecmp(attName, ATTR_UPDATESTATS_SEQUENCED) && 
				 strcasecmp(attName, ATTR_UPDATESTATS_LOST))) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;

	} else if (!(strcasecmp(attName, ATTR_LAST_HEARD_FROM) &&
				 strcasecmp(attName, "MonitorSelfTime"))) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;

	}
				 
	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

/* An attribute is treated as a static one if it is not one of the following
   Notice that if you add more attributes to the following list, the horizontal
   schema for Machines_Horizontal needs to revised accordingly.
*/
bool isHorizontalMachineAttr(
char *attName, 
QuillAttrDataType &attr_type) {
	if (!(strcasecmp(attName, ATTR_NAME) &&
		  strcasecmp(attName, ATTR_OPSYS) && 
		  strcasecmp(attName, ATTR_ARCH) &&		  
		  strcasecmp(attName, ATTR_STATE) && 
		  strcasecmp(attName, ATTR_ACTIVITY) &&		 
		  strcasecmp(attName, ATTR_GLOBAL_JOB_ID))) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!strcasecmp(attName, ATTR_CPU_IS_BUSY)) {
		attr_type = CONDOR_TT_TYPE_BOOL;
		return TRUE;

	} else if (!(strcasecmp(attName, ATTR_KEYBOARD_IDLE) && 
				 strcasecmp(attName, ATTR_CONSOLE_IDLE) &&
				 strcasecmp(attName, ATTR_LOAD_AVG) && 
				 strcasecmp(attName, "CondorLoadAvg") &&
				 strcasecmp(attName, ATTR_TOTAL_LOAD_AVG) && 
				 strcasecmp(attName, ATTR_VIRTUAL_MEMORY) &&
				 strcasecmp(attName, ATTR_MEMORY ) &&
				 strcasecmp(attName, ATTR_TOTAL_VIRTUAL_MEMORY) &&
				 strcasecmp(attName, ATTR_CPU_BUSY_TIME) &&
				 strcasecmp(attName, ATTR_CURRENT_RANK) &&
				 strcasecmp(attName, ATTR_CLOCK_MIN) && 
				 strcasecmp(attName, ATTR_CLOCK_DAY) &&
				 strcasecmp(attName, ATTR_UPDATE_SEQUENCE_NUMBER) &&
				 strcasecmp(attName, ATTR_UPDATESTATS_TOTAL) &&
				 strcasecmp(attName, ATTR_UPDATESTATS_SEQUENCED) &&
				 strcasecmp(attName, ATTR_UPDATESTATS_LOST))) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;
	} else if (!(strcasecmp(attName, "LastReportedTime") &&
				 strcasecmp(attName, ATTR_ENTERED_CURRENT_ACTIVITY) &&
				 strcasecmp(attName, ATTR_ENTERED_CURRENT_STATE))) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;
	}

	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

bool isHorizontalProcAttribute(
const char *attName, 
QuillAttrDataType &attr_type) {
	if (!(strcasecmp(attName, "args"))) {
		attr_type = CONDOR_TT_TYPE_CLOB;
		return TRUE;

	} else if (!(strcasecmp(attName, "remotehost") && 
				 strcasecmp(attName, "globaljobid"))) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!(strcasecmp(attName, "jobstatus") &&
				 strcasecmp(attName, "imagesize") &&
				 strcasecmp(attName, "remoteusercpu") &&
				 strcasecmp(attName, "remotewallclocktime") &&
				 strcasecmp(attName, "jobprio") &&
				 strcasecmp(attName, "numrestarts"))) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;

	} else if (!(strcasecmp(attName, "shadowbday") && 
				 strcasecmp(attName, "enteredcurrentstatus"))) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;
	}

	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

bool isHorizontalClusterAttribute(
const char *attName, 
QuillAttrDataType &attr_type) {
	if (!(strcasecmp(attName, "cmd") && 
		  strcasecmp(attName, "args"))
		) {
		attr_type = CONDOR_TT_TYPE_CLOB;
		return TRUE;

	} else if (!strcasecmp(attName, "owner")) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!(strcasecmp(attName, "jobstatus") &&
				 strcasecmp(attName, "jobprio") &&
				 strcasecmp(attName, "imagesize") &&
				 strcasecmp(attName, "remoteusercpu") &&
				 strcasecmp(attName, "remotewallclocktime") &&
				 strcasecmp(attName, "jobuniverse"))) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;

	} else if (!strcasecmp(attName, "qdate")) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;
	}

	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

bool isHorizontalHistoryAttribute(
const char *attName, 
QuillAttrDataType &attr_type) {

	if (!(strcasecmp(attName, "cmd") && 
		  strcasecmp(attName, "env") &&
		  strcasecmp(attName, "args"))
		) {
		attr_type = CONDOR_TT_TYPE_CLOB;
		return TRUE;

	} else if (!(strcasecmp(attName, "transferin") && 
				 strcasecmp(attName, "transferout") &&
				 strcasecmp(attName, "transfererr"))
			   ) {
		
		attr_type = CONDOR_TT_TYPE_BOOL;
		return TRUE;

	} else if (!(strcasecmp(attName, "owner") &&
				 strcasecmp(attName, "globaljobid") &&				 
				 strcasecmp(attName, "condorversion") &&
				 strcasecmp(attName, "condorplatform") &&
				 strcasecmp(attName, "rootdir") &&
				 strcasecmp(attName, "Iwd") &&
				 strcasecmp(attName, "user") &&
				 strcasecmp(attName, "userlog")	&&
				 strcasecmp(attName, "killsig") &&
				 strcasecmp(attName, "in") &&
				 strcasecmp(attName, "out") &&
				 strcasecmp(attName, "err") &&
				 strcasecmp(attName, "shouldtransferfiles")  &&
				 strcasecmp(attName, "transferfiles") &&
				 strcasecmp(attName, "filesystemdomain") &&
				 strcasecmp(attName, "lastremotehost") 
				 )) {
		attr_type = CONDOR_TT_TYPE_STRING;
		return TRUE;

	} else if (!(strcasecmp(attName, "numckpts") && 
				 strcasecmp(attName, "numrestarts") &&
				 strcasecmp(attName, "numsystemholds") &&
				 strcasecmp(attName, "jobuniverse") &&
				 strcasecmp(attName, "minhosts") &&
				 strcasecmp(attName, "maxhosts") &&
				 strcasecmp(attName, "jobprio") &&
				 strcasecmp(attName, "coresize") &&
				 strcasecmp(attName, "executablesize") &&
				 strcasecmp(attName, "diskusage") &&
				 strcasecmp(attName, "numjobmatches") &&
				 strcasecmp(attName, "jobruncount") &&
				 strcasecmp(attName, "filereadcount") &&
				 strcasecmp(attName, "filereadbytes") &&
				 strcasecmp(attName, "filewritecount") &&
				 strcasecmp(attName, "filewritebytes") &&
				 strcasecmp(attName, "fileseekcount") &&
				 strcasecmp(attName, "totalsuspensions") &&
				 strcasecmp(attName, "imagesize") &&
				 strcasecmp(attName, "exitstatus") && 
				 strcasecmp(attName, "localusercpu") &&
				 strcasecmp(attName, "localsyscpu") &&
				 strcasecmp(attName, "remoteusercpu") &&
				 strcasecmp(attName, "remotesyscpu") &&
				 strcasecmp(attName, "bytessent") &&
				 strcasecmp(attName, "bytesrecvd") && 
				 strcasecmp(attName, "rscbytessent") &&
				 strcasecmp(attName, "rscbytesrecvd") &&
				 strcasecmp(attName, "exitcode") && 
				 strcasecmp(attName, "jobstatus") &&
				 strcasecmp(attName, "remotewallclocktime")
				 )) {
		attr_type = CONDOR_TT_TYPE_NUMBER;
		return TRUE;

	} else if (!(strcasecmp(attName, "qdate") &&
		  strcasecmp(attName, "lastmatchtime") &&		  
		  strcasecmp(attName, "jobstartdate") &&	
		  strcasecmp(attName, "jobcurrentstartdate") &&	
		  strcasecmp(attName, "enteredcurrentstatus") && 
		  strcasecmp(attName, "completiondate"))) {
		attr_type = CONDOR_TT_TYPE_TIMESTAMP;
		return TRUE;
	}

	attr_type = CONDOR_TT_TYPE_UNKNOWN;
	return FALSE;
}

} // extern "C"
