
#include "condor_common.h"
#include "file_transfer_db.h"
#include "condor_attributes.h"
#include "condor_constants.h"
#include "pgsqldatabase.h"
#include "basename.h"
#include "my_hostname.h"
#include "internet.h"
#include "odbc.h"

extern char *mySubSystem;
//extern PGSQLDatabase *DBObj;
extern ODBC *DBObj;

#define MAXSQLLEN 500
#define MAXMACHNAME 128

void file_transfer_DbIns(file_transfer_record *rp, ClassAd *ad)
{
	char *dst_host = NULL, 
		*dst_path = NULL,
		*globalJobId = NULL,
	    *src_name = NULL,
		*src_path = NULL,
		*job_name = NULL,
		*dst_name = NULL;
	int runId = -1;
	char src_host[MAXMACHNAME];
	bool inStarter  = FALSE;
	char sqltext[MAXSQLLEN];

	if (strcmp(mySubSystem, "STARTER") == 0)
		inStarter = TRUE;

		// globalJobId;
	ad->LookupString(ATTR_GLOBAL_JOB_ID, &globalJobId);

		// runId
	ad->LookupInteger("RunId", runId);

		// dst_host, dst_name and dst_path
	dst_host = my_full_hostname();
	dst_name = basename(rp->fullname);
	dst_path = dirname(rp->fullname);

		// src_host, src_name and src_path
	sprintf(src_host, "%s", sin_to_hostname(rp->sockp->endpoint(), NULL));
	if (inStarter && (strcmp(dst_name, CONDOR_EXEC) == 0)) {
		ad->LookupString(ATTR_ORIG_JOB_CMD, &job_name);
		if (fullpath(job_name)) {
			src_name = basename(job_name);
			src_path = dirname(job_name);
		} else
			src_name = job_name;
		
	}
	else 
		src_name = dst_name;
	
	if (!src_path) {
		if (inStarter)
			ad->LookupString(ATTR_ORIG_JOB_IWD, &src_path);
		else 
			ad->LookupString(ATTR_JOB_IWD, &src_path);
	}
	
	sprintf(sqltext, 
			"insert into transfers values ('%s', %d, '%s', '%s', '%s', '%s', '%s', '%s', %d, %d)", 
			globalJobId, runId, src_name, src_host, src_path, dst_name, dst_host,
			dst_path, (int)rp->bytes, (int)rp->elapsed);

	free(dst_path);
	free(globalJobId);

	if(job_name)
		free(job_name);

	free(src_path);

	dprintf (D_ALWAYS, "In file_transfer_DbIns. sqltext is: %s\n", sqltext);

//	DBObj->beginTransaction();

		//DBObj->execCommand(sqltext);
	DBObj->odbc_sqlstmt(sqltext);
//	DBObj->commitTransaction();
}
