
#include "condor_common.h"

#include "file_types.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"
#include "condor_sys.h"

extern "C" void file_warning( char *format, ... )
{
	va_list	args;
	va_start(args,format);

	static char text[1024];
	vsprintf(text,format,args);

	dprintf(D_ALWAYS,text);
	fprintf(stderr,text);

	if(RemoteSysCalls()) REMOTE_syscall( CONDOR_perm_error, text );

	va_end(args);
}

extern "C" void __pure_virtual()
{
}

void File::dump() {
	dprintf(D_ALWAYS,
		"rfd: %d r: %d w: %d size: %d users: %d kind: '%s' name: '%s'",
		fd,readable,writeable,size,use_count,kind,name);
}

int File::illegal( char *op )
{
	file_warning("File system call '%s' is not supported for file '%s', which is mapped to a %s.\n",op,get_name(),get_kind());
	errno = EINVAL;
	return -1;
}

int File::map_fd_hack()                       { return fd; }
int File::local_access_hack()                 { return 0; }
int File::fstat( struct stat *buf )           { return illegal("fstat"); }
int File::ioctl( int cmd, int arg )           { return illegal("ioctl"); }
int File::flock( int op )                     { return illegal("flock"); }
int File::fstatfs( struct statfs *buf )       { return illegal("fstatfs"); }
int File::fchown( uid_t owner, gid_t group )  { return illegal("fchown"); }
int File::fchmod( mode_t mode )               { return illegal("fchmod"); }
int File::ftruncate( size_t length )          { return illegal("ftruncate"); }
int File::fsync()                             { return illegal("fsync"); }
int File::fcntl(int cmd, int arg)             { return illegal("fcntl"); }

LocalFile::LocalFile()
{
	fd = -1;
	readable = writeable = 0;
	kind = "local file";
	name[0] = 0;
	size = 0;
	use_count = 0;
	forced = 0;
	bufferable = 0;
	resume_count=0;
}

int LocalFile::open(const char *path, int flags, int mode ) {

	strncpy(name,path,_POSIX_PATH_MAX);

	IN_LOCAL_MODE( fd = ::open(path,flags,mode); )

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

	IN_LOCAL_MODE( size = ::lseek( fd, 0 , SEEK_END ); )
	if(size==-1) size=0;
	IN_LOCAL_MODE( lseek( fd, 0, SEEK_SET ); )

	return fd;
}

int LocalFile::close() {
	int result;
	IN_LOCAL_MODE( result = ::close(fd); )
	return result;
}

int LocalFile::read(int pos, char *data, int length) {
	// XXX Optimize this where possible
	int result;
	IN_LOCAL_MODE(
		::lseek(fd,pos,SEEK_SET);
		result = ::read(fd,data,length);
		)
	return result;
}

int LocalFile::write(int pos, char *data, int length) {
	// XXX Optimize this where possible
	int result;
	IN_LOCAL_MODE(
		::lseek(fd,pos,SEEK_SET);
		result = ::write(fd,data,length);
		)
	return result;
}

int LocalFile::fcntl( int cmd, int arg )
{
	int result;
	IN_LOCAL_MODE( result = ::fcntl(fd,cmd,arg); )
	return result;
}

int LocalFile::fstat( struct stat *buf )
{
	int result;
	IN_LOCAL_MODE( result = ::fstat(fd,buf); )
	return result;
}

int LocalFile::ioctl( int cmd, int arg )
{
	int result;
	IN_LOCAL_MODE( result = ::ioctl(fd,cmd,arg); )
	return result;
}

/* If a struct flock exists, c++ gets very confused between
   the structure and the system call.  This should fix the
   problem up. */

extern "C" int flock(int fd, int op);

int LocalFile::flock( int op )
{

	#ifdef CONDOR_USE_FLOCK
		int result;
		IN_LOCAL_MODE( result = ::flock(fd,op); )
		return result;
	#else
		File::flock(op);
	#endif
}

int LocalFile::fstatfs( struct statfs *buf )
{
	int result;
	#if defined(IRIX) || defined(Solaris)
		IN_LOCAL_MODE( result = ::fstatfs(fd,buf,sizeof(struct statfs),0); )
	#elif defined(OSF1)
		IN_LOCAL_MODE( result = ::fstatfs(fd,buf,sizeof(struct statfs)); )
	#else
		IN_LOCAL_MODE( result = ::fstatfs(fd,buf); )
	#endif
	return result;
}

int LocalFile::fchown( uid_t owner, gid_t group )
{
	int result;
	IN_LOCAL_MODE( result = ::fchown(fd,owner,group) );
	return result;
}

int LocalFile::fchmod( mode_t mode )
{
	int result;
	IN_LOCAL_MODE( result = ::fchmod(fd,mode) );
	return result;
}

int LocalFile::ftruncate( size_t length )
{
	size = length;
	int result;
	IN_LOCAL_MODE( result = ::ftruncate(fd,length) );
	return result;
}

int LocalFile::fsync()
{
	int result;
	IN_LOCAL_MODE( result = ::fsync(fd) );
	return result;
}

void LocalFile::checkpoint()
{
}

void LocalFile::suspend()
{
	if( (fd==-1) || forced ) return;
	IN_LOCAL_MODE( ::close(fd); )
	fd=-1;
}

void LocalFile::resume( int count )
{
	if( (count==resume_count) || forced ) return;
	resume_count = count;

	int flags;

	if( readable&&writeable )	flags = O_RDWR;
	else if( writeable )		flags = O_WRONLY;
	else				flags = O_RDONLY;

	IN_LOCAL_MODE( fd = ::open(name,flags); )
	if( fd==-1 ) {
		file_warning("Unable to reopen local file %s after a checkpoint!\n",name);
	}
}

int LocalFile::local_access_hack()
{
	return 1;
}

RemoteFile::RemoteFile()
{
	fd = -1;
	readable = writeable = 0;
	kind = "remote file";
	name[0] = 0;
	size = 0;
	use_count = 0;
	forced = 0;
	bufferable = 0;
	resume_count = 0;
}

int RemoteFile::open(const char *path, int flags, int mode )
{
	strncpy(name,path,_POSIX_PATH_MAX);

	fd = REMOTE_syscall( CONDOR_open, path, flags, mode );

	if( fd<0 ) return fd;

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

	size = REMOTE_syscall( CONDOR_lseek, fd, 0, SEEK_END );
	if(size<0) size=0;
	REMOTE_syscall( CONDOR_lseek, fd, 0, SEEK_SET );

	return fd;
}

int RemoteFile::close() {
	return REMOTE_syscall( CONDOR_close, fd );
}

int RemoteFile::read(int pos, char *data, int length) {
	// XXX optimize this to to read when possible
       	return REMOTE_syscall( CONDOR_lseekread, fd, pos, SEEK_SET, data, length );
}

int RemoteFile::write(int pos, char *data, int length) {
	// XXX optimize this to do write when possible
	return REMOTE_syscall( CONDOR_lseekwrite, fd, pos, SEEK_SET, data, length );
}

void RemoteFile::checkpoint()
{
}

void RemoteFile::suspend()
{
	if( (fd==-1) || forced ) return;
	REMOTE_syscall( CONDOR_close, fd );
	fd = -1;
}

void RemoteFile::resume( int count )
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

	fd=REMOTE_syscall( CONDOR_open, name, flags, 0 );
	if(fd<0) {
		file_warning("Unable to re-open remote file %s after checkpoint!\n",name);
	}
}

int RemoteFile::fcntl( int cmd, int arg )
{
	return REMOTE_syscall( CONDOR_fcntl, fd, cmd, arg );
}

int RemoteFile::fstat( struct stat *buf )
{
	return REMOTE_syscall( CONDOR_fstat, fd, buf );
}

int RemoteFile::ioctl( int cmd, int arg )
{
	return REMOTE_syscall( CONDOR_ioctl, fd, cmd, arg );
}

int RemoteFile::flock( int op )
{
	return REMOTE_syscall( CONDOR_flock, fd, op );
}

int RemoteFile::fstatfs( struct statfs *buf )
{
	return REMOTE_syscall( CONDOR_fstatfs, fd, buf );
}

int RemoteFile::fchown( uid_t owner, gid_t group )
{
	return REMOTE_syscall( CONDOR_fchown, fd, owner, group );
}

int RemoteFile::fchmod( mode_t mode )
{
	return REMOTE_syscall( CONDOR_fchmod, fd, mode );
}

int RemoteFile::ftruncate( size_t length )
{
	size = length;
	return REMOTE_syscall( CONDOR_ftruncate, fd, length );
}

int RemoteFile::fsync()
{
	return REMOTE_syscall( CONDOR_fsync, fd );
}



