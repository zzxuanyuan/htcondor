
#include "condor_common.h"
#include "condor_file_remote.h"
#include "condor_file_warning.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

CondorFileRemote::CondorFileRemote()
{
	init();
	kind = "remote file";
}

int CondorFileRemote::open(const char *path, int flags, int mode )
{
	int scm, result;

	scm = SetSyscalls(SYS_REMOTE|SYS_UNMAPPED);
	result = CondorFile::open(path,flags,mode);
	SetSyscalls(scm);

	return result;
}

int CondorFileRemote::close() {
	int scm,result;

	scm = SetSyscalls(SYS_REMOTE|SYS_UNMAPPED);
	CondorFile::close();
	SetSyscalls(scm);

	return result;
}

int CondorFileRemote::read(int pos, char *data, int length) {
	int result;
	// Change this to a switch call when it exists.
       	result = REMOTE_syscall( CONDOR_lseekread, fd, pos, SEEK_SET, data, length );
	CondorFile::read(pos,data,result);
	return result;
}

int CondorFileRemote::write(int pos, char *data, int length) {
	int result;
	// Change this to switch call when it exists
	result = REMOTE_syscall( CONDOR_lseekwrite, fd, pos, SEEK_SET, data, length );
	CondorFile::write(pos,data,result);
	return result;
}

void CondorFileRemote::checkpoint()
{
}

void CondorFileRemote::suspend()
{
	if( (fd==-1) || forced ) return;
	int scm = SetSyscalls(SYS_REMOTE|SYS_UNMAPPED);
	::close(fd);
	SetSyscalls(scm);
	fd = -1;
}

void CondorFileRemote::resume( int count )
{
	if( (count==resume_count) || forced ) return;
	resume_count = count;

	int flags;

	if( readable&&writeable ) {
		flags = O_RDWR;
	} else if( writeable ) {
		flags = O_WRONLY;
	} else {
		flags = O_RDONLY;
	}

	int scm = SetSyscalls(SYS_REMOTE|SYS_UNMAPPED);
	fd = ::open(name,flags,0);
	SetSyscalls(scm);
	if(fd<0) {
		_condor_file_warning("Unable to re-open remote file %s after checkpoint!\n",name);
	}
}

/*
In remote mode, we can only support fcntl and ioctl
commands that have a single integer argument.  Others
are a lost cause...
*/

int CondorFileRemote::fcntl( int cmd, int arg )
{
	int result, scm;

	struct flock *f;

	switch(cmd) {
		#ifdef F_GETFD
		case F_GETFD:
		#endif

		#ifdef F_GETFL
		case F_GETFL:
		#endif

		#ifdef F_SETFD
		case F_SETFD:
		#endif
		
		#ifdef F_SETFL
		case F_SETFL:
		#endif
			scm = SetSyscalls(SYS_REMOTE|SYS_UNMAPPED);
			result = ::fcntl(fd,cmd,arg);
			SetSyscalls(scm);
			return result;

		#ifdef F_FREESP
		case F_FREESP:
		#endif

		#ifdef F_FREESP64
		case F_FREESP64:
		#endif

			/* When all fields of the lockarg are zero,
			   this is the same as truncate, and we know
			   how to do that already. */

			f = (struct flock *)arg;
			if( (f->l_whence==0) && (f->l_start==0) && (f->l_len==0) ) {
				return ftruncate(0);
			}

			/* Otherwise, fall through here. */

		default:

			_condor_file_warning("fcntl(%d,%d,...) is not supported for remote files.",fd,cmd);
			errno = EINVAL;
			return -1;
	}
}

int CondorFileRemote::ioctl( int cmd, int arg )
{
	_condor_file_warning("ioctl(%d,%d,...) is not supported for remote files.",fd,cmd);
	errno = EINVAL;
	return -1;
}

int CondorFileRemote::ftruncate( size_t length )
{
	int scm,result;

	set_size(length);
	scm = SetSyscalls(SYS_REMOTE|SYS_UNMAPPED);
	result = ::ftruncate(fd,length);
	SetSyscalls(scm);

	return result;
}

int CondorFileRemote::fsync()
{
	int scm,result;

	scm = SetSyscalls(SYS_REMOTE|SYS_UNMAPPED);
	result = ::fsync(fd);
	SetSyscalls(scm);

	return result;
}

int CondorFileRemote::local_access_hack()
{
	return 0;
}

int CondorFileRemote::map_fd_hack()
{
	return fd;
}
