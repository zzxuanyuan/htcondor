#ifndef SCHEDD_FILES_H
#define SCHEDD_FILES_H

#include "condor_classad.h"
#include "condor_attrlist.h"
//#include "../condor_daemon_core.V6/condor_daemon_core.h"

// procad: the class ad for the job
// preExec: called before job execution
// oldad: old ad that doesn't have macro replaced, e.g. $$(OPSYS)
void schedd_files_DbIns(ClassAd *procad, bool preExec, ClassAd *oldAd = NULL);

#endif
