
#include "condor_common.h"
#include "condor_file.h"
#include "condor_file_warning.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

CondorFile::~CondorFile()
{
}

// Initialize all fields to null

void CondorFile::init() {
	fd = -1;
	readable = writeable = 0;
	kind = 0;
	name[0] = 0;
	size = 0;
	resume_count = 0;
	forced = 0;
	read_count = write_count = seek_count = 0;
	read_bytes = write_bytes = 0;
	ioctl_sig = 0;
	fcntl_fl = 0;
	fcntl_fd = 0;
}

// Display this file in the log

void CondorFile::dump() {
	dprintf(D_ALWAYS,
		"rfd: %d r: %d w: %d size: %d kind: '%s' name: '%s'",
		fd,readable,writeable,size,kind,name);
}

void CondorFile::abort( char *op ) {
	_condor_file_warning("Unable to %s %s %s (%s)\n",
		op, kind, name, strerror(errno));
	exit(-1);
}

// Nearly every kind of file needs to perform this initialization.

int CondorFile::open(const char *path, int flags, int mode) {

	strncpy(name,path,_POSIX_PATH_MAX);

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

	fd = ::open(path,flags,mode);
	if(fd<0) return fd;

	// Find the size of the file

	size = ::lseek(fd,0,SEEK_END);
	if(size==-1) {
		size=0;
	}
	::lseek(fd,0,SEEK_SET);

	return fd;
}


int CondorFile::close()
{
	::close(fd);
	fd = -1;
	return -1;
}

void CondorFile::report( int closing )
{
	if( MyImage.GetMode() != STANDALONE ) {
		REMOTE_syscall( CONDOR_report_file_info, kind, name, read_count, write_count, seek_count, read_bytes, write_bytes, closing );
	}
}

// Connect this file to an existing fd

int CondorFile::force_open( int f, char *n, int r, int w )
{
	fd = f;
	strcpy(name,n);
	readable = r;
	writeable = w;
	forced = 1;
}
