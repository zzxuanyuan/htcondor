
#include "condor_common.h"

#include "image.h"

#include "file_types.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"
#include "condor_sys.h"

extern "C" void _condor_file_warning( char *format, ... )
{
	va_list	args;
	va_start(args,format);

	static char text[1024];
	vsprintf(text,format,args);

	if(MyImage.GetMode()==STANDALONE) {
		fprintf(stderr,"Condor Warning: %s\n",text);
	} else if(LocalSysCalls()) {
		dprintf(D_ALWAYS,"Condor Warning: %s\n",text);
	} else {
		REMOTE_syscall( CONDOR_report_error, text );
	}

	va_end(args);
}

extern "C" void _condor_file_info( char *format, ... )
{
	va_list	args;
	va_start(args,format);

	static char text[1024];
	vsprintf(text,format,args);

	if(MyImage.GetMode()!=STANDALONE) {
		REMOTE_syscall( CONDOR_report_error, text );
	}

	va_end(args);
}

extern "C" void __pure_virtual()
{
}

void CondorFile::dump() {
	dprintf(D_ALWAYS,
		"rfd: %d r: %d w: %d size: %d users: %d kind: '%s' name: '%s'",
		fd,readable,writeable,size,use_count,kind,name);
}

// Nearly every kind of file needs to perform this initialization.
// This method is provided so that implementors of this class can use
// these tools

int CondorFile::open(const char *path, int flags, int mode) {

	strncpy(name,path,_POSIX_PATH_MAX);

	fd = ::open(path,flags,mode);

	switch( flags & O_ACCMODE ) {
		case O_RDONLY:
			readable = 1;
			writeable = 0;
			break;
		case O_WRONLY:
			readable = 0;
			writeable = 1;
			break;
		case O_RDWR:
			readable = 1;
			writeable = 1;
			break;
		default:
			return -1;
	}

	// Find the size of the file and store it.

	size = lseek(fd,0,SEEK_END);
	if(size==-1) {
		seekable=0;
		size=0;
	} else {
		seekable=1;
	}
	lseek(fd,0,SEEK_SET);

	return fd;
}

int CondorFile::force_open( int f, int r, int w )
{
	fd = f;
	readable = r;
	writeable = w;
	forced = 1;
}

void CondorFile::print_info()
{
	_condor_file_info("%s '%s':\n%d reads, %d bytes read, %d writes, %d bytes written\n",kind,name,read_count,read_bytes,write_count,write_bytes);
}


CondorFileLocal::CondorFileLocal()
{
	fd = -1;
	readable = writeable = seekable = bufferable = 0;
	kind = "local file";
	name[0] = 0;
	size = 0;
	forced = 0;
	use_count = 0;
	resume_count=0;
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

	return result;
}

int CondorFileLocal::write(int pos, char *data, int length) {
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::lseek(fd,pos,SEEK_SET);
	result = ::write(fd,data,length);
	SetSyscalls(scm);

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

CondorFileRemote::CondorFileRemote()
{
	fd = -1;
	readable = writeable = seekable = bufferable = 0;
	kind = "remote file";
	name[0] = 0;
	size = 0;
	forced = 0;
	use_count = 0;
	resume_count=0;
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
	result = ::close(fd);
	SetSyscalls(scm);

	return result;
}

int CondorFileRemote::read(int pos, char *data, int length) {
	// Change this to a switch call when it exists.
       	return REMOTE_syscall( CONDOR_lseekread, fd, pos, SEEK_SET, data, length );
}

int CondorFileRemote::write(int pos, char *data, int length) {
	// Change this to switch call when it exists
	return REMOTE_syscall( CONDOR_lseekwrite, fd, pos, SEEK_SET, data, length );
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

CondorFileSpecial::CondorFileSpecial(char *k)
{
	fd = -1;
	readable = writeable = 0;
	kind = k;
	name[0] = 0;
	size = 0;
	use_count = 0;
	forced = 0;
	bufferable = 0;
	resume_count=0;
}

void CondorFileSpecial::checkpoint()
{
	suspend();
}

void CondorFileSpecial::suspend()
{
	_condor_file_warning("A %s cannot be used across checkpoints.\n",kind);
	Suicide();
}




