
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
	MyString dst_host = "", 
		dst_path = "",
		globalJobId = "",
		src_name = "",
		src_path = "",
		job_name = "",
		dst_name = "",
		src_fullname = "";

	char src_host[MAXMACHNAME];
	bool inStarter  = FALSE;
	char *tmpp;

	struct stat file_status;

	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	MyString tmp;

		// this function access the following pointers
	if  (!rp || !ad || !FILEObj)
		return;

		// check if we are in starter process
	if (mySubSystem && strcmp(mySubSystem, "STARTER") == 0)
		inStarter = TRUE;

	ad->LookupString(ATTR_GLOBAL_JOB_ID, globalJobId);

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
	if (inStarter && !dst_name.IsEmpty() &&
		(strcmp(dst_name.GetCStr(), CONDOR_EXEC) == 0)) {
		ad->LookupString(ATTR_ORIG_JOB_CMD, job_name);
		if (!job_name.IsEmpty() && fullpath(job_name.GetCStr())) {
			src_name = basename(job_name.GetCStr());
			src_path = dirname(job_name.GetCStr());
		} else
			src_name = job_name;
		
	}
	else 
		src_name = dst_name;
	
	if (src_path.IsEmpty()) {
		if (inStarter)
			ad->LookupString(ATTR_ORIG_JOB_IWD, src_path);
		else 
			ad->LookupString(ATTR_JOB_IWD, src_path);
	}

  // Get the file status (contains last modified time)
  // Get this info from the schedd/shadow side
  if (inStarter) { // use src info
    src_fullname = src_path;
    src_fullname += "/";
    src_fullname += src_name;
    if (stat(src_fullname.GetCStr(), &file_status) < 0) {
      dprintf(D_ALWAYS, "ERROR: File %s can not be accessed.\n", 
			  src_fullname.GetCStr());
    }
  } else { // use dst info
    if (stat(rp->fullname, &file_status) < 0) {
      dprintf(D_ALWAYS, "ERROR: File %s can not be accessed.\n", rp->fullname);
    }
  }

	tmp.sprintf("globalJobId = \"%s\"", globalJobId.GetCStr());
	tmpClP1->Insert(tmp.GetCStr());			
	
	tmp.sprintf("src_name = \"%s\"", src_name.GetCStr());
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("src_host = \"%s\"", src_host);
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("src_path = \"%s\"", src_path.GetCStr());
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("dst_name = \"%s\"", dst_name.GetCStr());
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("dst_host = \"%s\"", dst_host.GetCStr());
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("dst_path = \"%s\"", dst_path.GetCStr());
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("transfer_size = %d", (int)rp->bytes);
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("elapsed = %d", (int)rp->elapsed);
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("dst_daemon = %s", rp->daemon);
	tmpClP1->Insert(tmp.GetCStr());

	tmp.sprintf("f_ts = %d", (int)file_status.st_mtime);
	tmpClP1->Insert(tmp.GetCStr());

	FILEObj->file_newEvent("Transfers", tmpClP1);

}
