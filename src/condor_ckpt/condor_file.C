
#include "condor_common.h"
#include "condor_file.h"
#include "condor_file_warning.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

// Initialize all fields to null

void CondorFile::init() {
	fd = -1;
	readable = writeable = seekable = bufferable = 0;
	kind = 0;
	name[0] = 0;
	size = 0;
	forced = 0;
	seek_count = read_count = write_count = read_bytes = write_bytes = 0;
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

	// Find the size of the file

	size = ::lseek(fd,0,SEEK_END);
	if(size==-1) {
		seekable=0;
		size=0;
	} else {
		seekable=1;
	}
	::lseek(fd,0,SEEK_SET);

	return fd;
}

// Anytime a file is closed, send back an I/O report 

int CondorFile::close()
{
	report_file_info();
	return -1;
}

// Update I/O records on every read.
 
int CondorFile::read( int pos, char *data, int length )
{
	read_count++;
	if(length>0) read_bytes+=length;
	return -1;
}

// Update I/O records on every write

int CondorFile::write( int pos, char *data, int length )
{
	write_count++;
	if(length>0) write_bytes+=length;
	return -1;
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

// Send an I/O report back to the shadow

void CondorFile::report_file_info()
{
	if(MyImage.GetMode()!=STANDALONE) {
		REMOTE_syscall( CONDOR_report_file_info,
			name,
			read_count,read_bytes,
			write_count,write_bytes,
			seek_count,size);
	}
}
