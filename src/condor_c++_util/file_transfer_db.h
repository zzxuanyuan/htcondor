#ifndef FILE_TRANSFER_DB_H
#define FILE_TRANSFER_DB_H

#include "condor_classad.h"
#include "condor_attrlist.h"
//#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "reli_sock.h"

typedef struct 
{
	char *fullname; /* file name in the destination */
	filesize_t   bytes;
	time_t elapsed;
	ReliSock *sockp;
	
} file_transfer_record;

void file_transfer_DbIns(file_transfer_record *rp, ClassAd *ad);

#endif
