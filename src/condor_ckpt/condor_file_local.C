
#include "condor_common.h"
#include "condor_file_local.h"
#include "condor_file_warning.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

CondorFileLocal::CondorFileLocal()
{
	init();
	kind = "local file";
}

int CondorFileLocal::open(const char *path, int flags, int mode ) {

	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = CondorFile::open(path,flags,mode);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::close() {
	int result, scm;

	CondorFile::close();

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::close(fd);
	fd = -1;
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::read(int pos, char *data, int length) {
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::lseek(fd,pos,SEEK_SET);
	result = ::read(fd,data,length);
	SetSyscalls(scm);

	CondorFile::read(pos,data,result);

	return result;
}

int CondorFileLocal::write(int pos, char *data, int length) {
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::lseek(fd,pos,SEEK_SET);
	result = ::write(fd,data,length);
	SetSyscalls(scm);

	CondorFile::write(pos,data,result);

	return result;
}

/*
We can happily support any fcntl or ioctl command in local mode.
Remote mode is a different story, see below.
*/

int CondorFileLocal::fcntl( int cmd, int arg )
{
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::fcntl(fd,cmd,arg);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::ioctl( int cmd, int arg )
{
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::ioctl(fd,cmd,arg);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::ftruncate( size_t length )
{
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::ftruncate(fd,length);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::fsync()
{
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::fsync(fd);
	SetSyscalls(scm);

	return result;
}

void CondorFileLocal::checkpoint()
{
}

void CondorFileLocal::suspend()
{
	if( (fd==-1) || forced ) return;
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::close(fd);
	SetSyscalls(scm);
	fd=-1;
}

void CondorFileLocal::resume( int count )
{
	if( (count==resume_count) || forced ) return;
	resume_count = count;

	int flags;

	if( readable&&writeable )	flags = O_RDWR;
	else if( writeable )		flags = O_WRONLY;
	else				flags = O_RDONLY;

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	fd = ::open(name,flags);
	if( fd==-1 ) {
		_condor_file_warning("Unable to reopen local file %s after a checkpoint!\n",name);
	}
	SetSyscalls(scm);
}

int CondorFileLocal::local_access_hack()
{
	return 1;
}

int CondorFileLocal::map_fd_hack()
{
	return fd;
}