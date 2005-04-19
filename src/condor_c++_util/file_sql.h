#include <string.h>
#include <file_lock.h>
#ifndef FILESQL_H
#define FILE_H

class FILESQL 
{
private:

	bool 	is_open;
	bool 	is_locked;
	char outfilename[257];
	int fileflags;
	int outfiledes;
	FileLock *lock;
public:
	
	FILESQL();
	FILESQL(char *outfilename,int flags);
	~FILESQL();
	bool file_isopen();
	bool file_islocked();
	long file_open();
	long file_close();
	long file_lock();
	long file_unlock();
	long file_sqlstmt(const char* statement);	

};

FILESQL *createInstance();

#endif
