#include <string.h>
#include <file_lock.h>
#include <condor_attrlist.h>
#ifndef FILESQL_H
#define FILE_H

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
	long file_open();
	long file_close();
	long file_lock();
	long file_unlock();
	long file_newEvent(const char *eventType, AttrList *info);
	long file_updateEvent(const char *eventType, AttrList *info, AttrList *condition);
	long file_deleteEvent(const char *eventType, AttrList *condition);
	int  file_readline(MyString *buf);
	AttrList  *file_readAttrList();
	int  file_truncate();
};

FILESQL *createInstance();

#endif
