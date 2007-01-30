#ifndef FILE_TRANSFER_DB_H
#define FILE_TRANSFER_DB_H

#include "condor_classad.h"
#include "condor_attrlist.h"
#include "reli_sock.h"

typedef struct 
{
	char *fullname; /* file name in the destination */
	filesize_t   bytes; /* size in bytes */
	time_t elapsed; /* elapsed seconds */
	char *daemon; /* deamon doing the download */
	ReliSock *sockp; /* associated socket for getting source info */
	time_t transfer_time;  /* epoch time when transfer is initiated */	
} file_transfer_record;

void file_transfer_db(file_transfer_record *rp, ClassAd *ad);

#endif
