#include <string.h>
#include <file_lock.h>
#include <condor_attrlist.h>
#include "quill_enums.h"
#ifndef FILESQL_H
#define FILESQL_H

class FILESQL 
{
private:

	bool 	is_open;
	bool 	is_locked;
	char *outfilename;
	int fileflags;
	int outfiledes;
	FileLock *lock;
	FILE *fp;
public:
	
	FILESQL();
	FILESQL(char *outfilename,int flags=O_WRONLY|O_CREAT|O_APPEND);
	~FILESQL();
	bool file_isopen();
	bool file_islocked();
	QuillErrCode file_open();
	QuillErrCode file_close();
	QuillErrCode file_lock();
	QuillErrCode file_unlock();
	QuillErrCode file_newEvent(const char *eventType, AttrList *info);
	QuillErrCode file_updateEvent(const char *eventType, AttrList *info, 
								  AttrList *condition);
	QuillErrCode file_deleteEvent(const char *eventType, AttrList *condition);
	int  file_readline(MyString *buf);
	AttrList  *file_readAttrList();
	QuillErrCode  file_truncate();
};

FILESQL *createInstance();

#endif
