
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
	char *tmpp;

	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	char tmp[512];
 
		// this function access the following pointers
	if  (!rp || !ad || !FILEObj)
		return;

		// check if we are in starter process
	if (mySubSystem && strcmp(mySubSystem, "STARTER") == 0)
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
		(tmpp = sin_to_hostname(rp->sockp->endpoint(), NULL))) {
		snprintf(src_host, MAXMACHNAME, "%s", tmpp);
	}

		// src_name, src_path
	if (inStarter && dst_name && (strcmp(dst_name, CONDOR_EXEC) == 0)) {
		ad->LookupString(ATTR_ORIG_JOB_CMD, &job_name);
		if (job_name && fullpath(job_name)) {
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

	snprintf(tmp, 512, "globalJobId = \"%s\"", globalJobId);
	tmpClP1->Insert(tmp);			
	
	snprintf(tmp, 512, "src_name = \"%s\"", src_name);
	tmpClP1->Insert(tmp);

	snprintf(tmp, 512, "src_host = \"%s\"", src_host);
	tmpClP1->Insert(tmp);

	snprintf(tmp, 512, "src_path = \"%s\"", src_path);
	tmpClP1->Insert(tmp);

	snprintf(tmp, 512, "dst_name = \"%s\"", dst_name);
	tmpClP1->Insert(tmp);

	snprintf(tmp, 512, "dst_host = \"%s\"", dst_host);
	tmpClP1->Insert(tmp);

	snprintf(tmp, 512, "dst_path = \"%s\"", dst_path);
	tmpClP1->Insert(tmp);

	snprintf(tmp, 512, "size = %d", (int)rp->bytes);
	tmpClP1->Insert(tmp);

	snprintf(tmp, 512, "elapsed = %d", (int)rp->elapsed);
	tmpClP1->Insert(tmp);

	FILEObj->file_newEvent("Transfers", tmpClP1);

	if (dst_path) free(dst_path);
	if (globalJobId) free(globalJobId);
	if(job_name) free(job_name);
	if (src_path) free(src_path);
}
