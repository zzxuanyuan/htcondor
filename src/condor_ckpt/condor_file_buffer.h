#ifndef CONDOR_FILE_BUFFER_H
#define CONDOR_FILE_BUFFER_H

#include "condor_file.h"

class CondorChunk;

class CondorFileBuffer : public CondorFile {
public:
	CondorFileBuffer( CondorFile *original );
	virtual ~CondorFileBuffer();

	virtual void dump();

	virtual int open(const char *path, int flags, int mode);
	virtual int close();
	virtual int read(int offset, char *data, int length);
	virtual int write(int offset, char *data, int length);

	virtual int fcntl( int cmd, int arg );
	virtual int ioctl( int cmd, int arg );
	virtual int ftruncate( size_t length ); 
	virtual int fsync();

	virtual void checkpoint();
	virtual void suspend();
	virtual void resume(int count);

	virtual int map_fd_hack();
	virtual int local_access_hack();

private:
	void trim();
	void flush( int deallocate );
	void evict( CondorChunk *c );
	void clean( CondorChunk *c );
	double benefit_cost( CondorChunk *c );

	CondorFile *original;
	CondorChunk *head;
	int time;
};

#endif
