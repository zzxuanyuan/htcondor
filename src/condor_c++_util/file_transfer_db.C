
#include "condor_common.h"
#include "file_transfer_db.h"
#include "condor_attributes.h"
#include "condor_constants.h"
#include "pgsqldatabase.h"
#include "basename.h"
#include "my_hostname.h"
#include "internet.h"
#include "file_sql.h"

extern char *mySubSystem;
extern FILESQL *FILEObj;

#define MAXSQLLEN 500
#define MAXMACHNAME 128

void file_transfer_db(file_transfer_record *rp, ClassAd *ad)
{
	char *dst_host = NULL, 
		*dst_path = NULL,
		*globalJobId = NULL,
	    *src_name = NULL,
		*src_path = NULL,
		*job_name = NULL,
		*dst_name = NULL;
	char src_host[MAXMACHNAME];
	bool inStarter  = FALSE;
	char sqltext[MAXSQLLEN];
	char *tmp;

		// this function access the following pointers
	if  (!rp || !ad || !FILEObj)
		return;

		// check if we are in starter process
	if (strcmp(mySubSystem, "STARTER") == 0)
		inStarter = TRUE;

		// globalJobId, it should be freed later
	ad->LookupString(ATTR_GLOBAL_JOB_ID, &globalJobId);

		// dst_host, dst_name and dst_path, since file_transfer_db
		// is called in the destination process, dst_host is my
		// hostname
	dst_host = my_full_hostname();
	dst_name = basename(rp->fullname);
	dst_path = dirname(rp->fullname);

		// src_host
	src_host[0] = '\0';
	if (rp->sockp && 
		(tmp = sin_to_hostname(rp->sockp->endpoint(), NULL))) {
		sprintf(src_host, "%s", tmp);
	}

		// src_name, src_path
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
	
		// make the sql statement
	sprintf(sqltext, 
			"insert into transfers values ('%s', '%s', '%s', '%s', '%s', '%s', '%s', %d, %d)", 
			globalJobId, src_name, src_host, src_path, dst_name, dst_host,
			dst_path, (int)rp->bytes, (int)rp->elapsed);

	if (dst_path) free(dst_path);
	if (globalJobId) free(globalJobId);
	if(job_name) free(job_name);
	if (src_path) free(src_path);

	dprintf (D_FULLDEBUG, "In file_transfer_db. sqltext is: %s\n", sqltext);

	FILEObj->file_lock();

	FILEObj->file_sqlstmt(sqltext);
	
	FILEObj->file_unlock();
}
