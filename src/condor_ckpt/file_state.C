/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "image.h"

#include "condor_common.h"
#include "condor_syscalls.h"
#include "condor_sys.h"
#include "condor_file_info.h"
#include "condor_syscall_mode.h"

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

#include "file_state.h"
#include "file_table_interf.h"
#include "condor_file.h"
#include "condor_file_local.h"
#include "condor_file_remote.h"
#include "condor_file_special.h"
#include "condor_file_agent.h"
#include "condor_file_warning.h"

#include <stdarg.h>
#include <sys/errno.h>

// XXX Where is the header for this?
extern int errno;

CondorFileTable *FileTab=0;

/*
This init function could be called from just about anywhere, perhaps
even when there is no shadow to talk to.  We will delay initializing
a buffer until we do the first remote open.
*/

void CondorFileTable::init()
{
	buffer = 0;
	resume_count = 0;
	got_buffer_info = 0;

	read_count = read_bytes = write_count = write_bytes = seek_count = 0;
	actual_read_count = actual_read_bytes = 0;
	actual_write_count = actual_write_bytes = 0;

	int scm = SetSyscalls( SYS_UNMAPPED | SYS_LOCAL );
	length = sysconf(_SC_OPEN_MAX);
	SetSyscalls(scm);

	pointers = new (CondorFilePointer *)[length];
	if(!pointers) {
		EXCEPT("Condor Error: CondorFileTable: Out of memory!\n");
		Suicide();
	}

	for( int i=0; i<length; i++ ) pointers[i]=0;

	/* Until we know better, identity map std files */

	pre_open( 0, "standard input", 1, 0, 0 );
	pre_open( 1, "standard output", 0, 1, 0 );
	pre_open( 2, "standard error", 0, 1, 0 );

	if(atexit(_condor_file_table_cleanup)<0) {
		_condor_file_warning("atexit() failed.  Files will not be cleaned up correctly at exit.\n");
	}
}

void CondorFileTable::init_buffer()
{
	int blocks=0, blocksize=0;

	// This value is thrown away
	int prefetch_size;

	if(buffer) return;

	if(got_buffer_info) return;
	got_buffer_info = 1;

	if(REMOTE_syscall(CONDOR_get_buffer_info,&blocks,&blocksize,&prefetch_size)<0) {
		dprintf(D_ALWAYS,"get_buffer_info failed!");
		buffer = 0;
		return;
	}

	if( !blocks || !blocksize ) {
		dprintf(D_ALWAYS,"Buffer is disabled\n.");
		buffer = 0;
	} else {
		dprintf(D_ALWAYS,"Buffer cache is %d blocks of %d bytes.\n",blocks,blocksize);
		buffer = new CondorBufferCache( blocks, blocksize );
	}
}


void CondorFileTable::disable_buffer()
{
	if(buffer) {
		delete buffer;
		buffer = 0;
	}
}

void CondorFileTable::flush()
{
	if(buffer) buffer->flush();
}

void CondorFileTable::dump()
{
	dprintf(D_ALWAYS,"\nOPEN FILE TABLE:\n");

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {
			dprintf(D_ALWAYS,"fd: %d offset: %d dups: %d ",
				i,
				pointers[i]->offset,
				count_pointer_uses(pointers[i]));
			pointers[i]->file->dump();
		}
		dprintf(D_ALWAYS,"\n");
	}
}

void CondorFileTable::close_all()
{
	for( int i=0; i<length; i++ )
		close(i);
}

int CondorFileTable::pre_open( int fd, char *name, int readable, int writeable, int is_remote )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]) ) {
		errno = EBADF;
		return -1;
	}

	CondorFile *f;

	if( is_remote ) f = new CondorFileRemote;
	else f = new CondorFileLocal;

	f->force_open(fd,name,readable,writeable);
	pointers[fd] = new CondorFilePointer(f);

	return fd;	
}

int CondorFileTable::find_name( const char *name )
{
	for( int fd=0; fd<length; fd++ ) {
		if( pointers[fd] && !strcmp(pointers[fd]->file->get_name(),name) ) {
			return fd;
		}
	}
	return -1;
}

int CondorFileTable::find_empty()
{
	for( int fd=0; fd<length; fd++ )
		if( !pointers[fd] )
			return fd;

	return -1;
}

void CondorFileTable::replace_file( CondorFile *oldfile, CondorFile *newfile )
{
	for( int fd=0; fd<length; fd++ )
		if(pointers[fd] && (pointers[fd]->file==oldfile))
			pointers[fd]->file==newfile;
}

int CondorFileTable::count_pointer_uses( CondorFilePointer *p )
{
	int count=0;

	for( int fd=0; fd<length; fd++ )
		if(pointers[fd]==p)
			count++;	

	return count;
}

int CondorFileTable::count_file_uses( CondorFile *f )
{
	int count=0;

	for( int fd=0; fd<length; fd++ )
		if(pointers[fd] && (pointers[fd]->file==f))
			count++;	

	return count;
}

int CondorFileTable::open( const char *logical_name, int flags, int mode )
{
	int	fd;
	char	full_path[_POSIX_PATH_MAX];
	char	url[_POSIX_PATH_MAX];
	char	method[_POSIX_PATH_MAX];

	CondorFile	*f;

	// Opening files O_RDWR is not safe across checkpoints
	// However, there is no problem as far as the rest of the
	// file code is concerned.  We will catch it here,
	// but the rest of the file table and buffer has no problem.

	if( flags & O_RDWR )
		_condor_file_warning("Opening file '%s' for read and write is not safe in a program that may be checkpointed!  You should use separate files for reading and writing.",logical_name);

	// Find an open slot in the table

	fd = find_empty();
	if( fd<0 ) {
		errno = EMFILE;
		return -1;
	}

	// Decide how to access the file.
	// In local syscalls, always use local.
	// In remote, ask the shadow for a complete url.

	if( LocalSysCalls() ) {
		strcpy(method,"local");
		if(logical_name[0]!='/') {
			getcwd(full_path,_POSIX_PATH_MAX);
			strcat(full_path,"/");
			strcat(full_path,logical_name);
		} else {
			strcpy(full_path,logical_name);
		}
	} else {
		init_buffer();
		REMOTE_syscall( CONDOR_get_file_info, logical_name, url );
		sscanf(url,"%[^:]:%s",method,full_path);
	}

	// If a file by this name is already open, share the file object.
	// Otherwise, create a new one according to the given method.

	int match = find_name(full_path);
	if(match>=0) {
		f = pointers[match]->file;
	} else {
		if(!strcmp(method,"local")) {
			f = new CondorFileLocal;
		} else if(!strcmp(method,"remote")) {
			f = new CondorFileRemote;
			f->enable_buffer();
		} else if(!strcmp(method,"remotefetch")) {
			f = new CondorFileAgent(new CondorFileRemote);
		} else {
			_condor_file_warning("I don't know how to access file '%s'",url);
			errno = ENOENT;
			return -1;
		}

		// Open the file according to its method.
		fd = f->open(full_path,flags,mode);
		if(fd<0) return -1;

		// Do not buffer stderr
		if(fd==2) f->disable_buffer();
	}

	// Install a new pointer in the file table, and return
	// the new file descriptor

	pointers[fd] = new CondorFilePointer(f);

	return fd;
}

/* Install a CondorFileSpecial on an fd, and return that fd. */

int CondorFileTable::install_special( char * kind )
{
	int fd = find_empty();
	if(!fd) {
		errno = EMFILE;
		return -1;
	}

	CondorFileSpecial *f = new CondorFileSpecial(kind);
	if(!f) {
		errno = ENOMEM;
		return -1;
	}

	CondorFilePointer *fp = new CondorFilePointer(f);
	if(!fp) {
		delete f;
		errno = ENOMEM;
		return -1;
	}

	pointers[fd] = fp;
	return fd;
}

/*
pipe() works by invoking a local pipe, and then installing a
CondorFileSpecial on those two fds.  A CondorFileSpecial is just like 
a local file, but checkpointing is prohibited will it exists.
*/

int CondorFileTable::pipe(int fds[])
{
	int real_fds[2];

	fds[0] = install_special("pipe");
	if(fds[0]<0) {
		return -1;
	}

	fds[1] = install_special("pipe");
	if(fds[1]<0) {
		close(fds[0]);
		return -1;
	}

	int scm = SetSyscalls( SYS_LOCAL|SYS_UNMAPPED );
	int result = pipe(real_fds);
	SetSyscalls(scm);

	if(result<0) {
		close(fds[0]);
		close(fds[1]);
		return -1;
	}

	pointers[fds[0]]->file->force_open(real_fds[0],"0",1,1);
	pointers[fds[1]]->file->force_open(real_fds[1],"1",1,1);

	return 0;
}

/*
Socket is similar to pipe.  We will perform a local socket(), and then
install a CondorFileSpecial on that fd to access it locally and inhibit
checkpointing in the meantime.
*/

int CondorFileTable::socket( int domain, int type, int protocol )
{
	int fd = install_special("socket");
	if(fd<0) {
		return -1;
	}

	int scm = SetSyscalls( SYS_LOCAL|SYS_UNMAPPED ); 
	int result = ::socket(domain,type,protocol);
	SetSyscalls(scm);

	if(result<0) {
		close(fd);
		return -1;
	}

	pointers[fd]->file->force_open( result, "", 1, 1 );

	return fd;
}

/*
Close is a little tricky.
The file pointer might be in use by several dups,
or the file itself might be in use by several opens.

So, count all uses of the file.  If there is only one,
close and delete.  Same goes for the file pointer.
*/

int CondorFileTable::close( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	CondorFilePointer *pointer = pointers[fd];
	CondorFile *file = pointers[fd]->file;

	// If this is the last use of the file, flush, close, and delete
	if(count_file_uses(file)==1) {
		if(buffer) buffer->flush(pointers[fd]->file);
		file->close();
		delete file;
	}

	// If this is the last use of the pointer, delete it
	if(count_pointer_uses(pointer)==1) {
		delete pointer;
	}

	// In any case, mark the fd as unused.
	pointers[fd]=0;

	return 0;
}

ssize_t CondorFileTable::read( int fd, void *data, size_t nbyte )
{
	read_count++;

	if( (fd>=length) || (fd<0) || (pointers[fd]==0) ||
	    (!pointers[fd]->file->is_readable()) ) {
		errno = EBADF;
		return -1;
	}
	
	if( (!data) || (nbyte<0) ) {
		errno = EINVAL;
		return -1;
	}

	CondorFilePointer *fp = pointers[fd];
	CondorFile *f = fp->file;

	// First, figure out the appropriate place to read from.

	int offset = fp->offset;
	int size = f->get_size();
	int actual;

	if( nbyte==0 ) return 0;

	// If buffering is allowed, read from the buffer,
	// otherwise, go straight to the access method

	if( f->ok_to_buffer() && buffer ) {

		// The buffer doesn't know about file sizes,
		// so cut off extra long reads here
		if( (offset+nbyte)>size ) nbyte = size-offset;

		// If there is nothing to read, that is not an error
		if(nbyte==0) return 0;

		// Now fetch from the buffer
		actual = buffer->read( f, offset, (char*)data, nbyte );
	} else {
		actual = read_unbuffered( f, offset, data, nbyte );
	}

	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Update the offset and summary
	fp->offset = offset+actual;
	read_bytes+=actual;

	// Return the number of bytes read
	return actual;
}

ssize_t CondorFileTable::read_unbuffered( CondorFile *f, off_t offset, void *data, size_t nbyte )
{
	int result;
	actual_read_count++;
 	result = f->read( offset, (char*)data, nbyte );
	if(result>0) actual_read_bytes+=result;
	return result;
}

ssize_t CondorFileTable::write( int fd, const void *data, size_t nbyte )
{
	write_count++;

	if( (fd>=length) || (fd<0) || (pointers[fd]==0) ||
	    (!pointers[fd]->file->is_writeable()) ) {
		errno = EBADF;
		return -1;
	}

	if( (!data) || (nbyte<0) ) {
		errno = EINVAL;
		return -1;
	}

	if( nbyte==0 ) return 0;

	CondorFilePointer *fp = pointers[fd];
	CondorFile *f = fp->file;

	// First, figure out the appropriate place to write to.

	int offset = fp->offset;
	int actual;

	// If buffering is allowed, write to the buffer,
	// otherwise, go straight to the access method

	if( f->ok_to_buffer() && buffer ) {
      		actual = buffer->write( f, offset, (char*)data, nbyte );
	} else {
		actual = write_unbuffered( f, offset, data, nbyte );
	}

	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Update the offset, and mark the file bigger if necessary
	if( (offset+actual)>f->get_size() )
		f->set_size(offset+actual);

	fp->offset = offset+actual;
	write_bytes += actual;

	// Return the number of bytes written
	return actual;
}

ssize_t CondorFileTable::write_unbuffered( CondorFile *f, off_t offset, const void *data, size_t nbyte )
{
	int result;
	actual_write_count++;
	result = f->write( offset, (char*)data, nbyte );
	actual_write_bytes += result;
	return result;
}

off_t CondorFileTable::lseek( int fd, off_t offset, int whence )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	CondorFilePointer *fp = pointers[fd];
	CondorFile *f = fp->file;
	int temp;

	// Compute the new offset first.
	// If the new offset is illegal, don't change it.

	if( whence == SEEK_SET ) {
		temp = offset;
	} else if( whence == SEEK_CUR ) {
	        temp = fp->offset+offset;
	} else if( whence == SEEK_END ) {
		temp = f->get_size()+offset;
	} else {
		errno = EINVAL;
		return -1;
	}

	if(temp<0) {
		errno = EINVAL;
		return -1;
	} else {
		fp->offset = temp;
		seek_count++;
		return temp;
	}
}

/* This function does a local dup2 one way or another. */
static int _condor_internal_dup2( int oldfd, int newfd )
{
	#if defined(SYS_dup2)
		return syscall( SYS_dup2, oldfd, newfd );
	#elif defined(SYS_fcntl) && defined(F_DUP2FD)
		return syscall( SYS_fcntl, oldfd, F_DUP2FD, newfd );
	#elif defined(SYS_fcntl) && defined(F_DUPFD)
		syscall( SYS_close, newfd );
		return syscall( SYS_fcntl, oldfd, F_DUPFD, newfd );
	#else
		#error "_condor_internal_dup2: I need SYS_dup2 or F_DUP2FD or F_DUPFD!"
	#endif
}

int CondorFileTable::dup( int fd )
{
	return search_dup2( fd, 0 );
}

int CondorFileTable::search_dup2( int fd, int search )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ||
	    (search<0) || (search>=length) ) {
		errno = EBADF;
		return -1;
	}

	int i;

	for( i=search; i<length; i++ )
		if(!pointers[i]) break;

	if( i==length ) {
		errno = EMFILE;
		return -1;
	} else {
		return dup2(fd,i);
	}
}

int CondorFileTable::dup2( int fd, int nfd )
{
	int result;

	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ||
	    (nfd<0) || (nfd>=length) ) {
		errno = EBADF;
		return -1;
	}

	if( fd==nfd ) return fd;

	if( pointers[nfd]!=0 ) close(nfd);

	pointers[nfd] = pointers[fd];

	/* If we are in standalone checkpointing mode,
	   we need to perform a real dup.  Because we
	   are constructing this file table identically
	   to the real system table, the result of
	   the syscall _ought_ to be the same as the
	   result of this virtual dup, but we will
	   check just to be sure. */

	if( MyImage.GetMode()==STANDALONE ) {
		result = _condor_internal_dup2(fd,nfd);
		if(result!=nfd) _condor_file_warning("internal_dup2(%d,%d) returned %d but I wanted %d!\n",fd,nfd,result,nfd);
	}
	    
	return nfd;
}

/*
fchdir is a little sneaky.  A particular fd might
be remapped to any old file name or access method, which
may or may not support changing directories.

(furthermore, what does it mean to fchdir to a file that
is mapped locally to, say, /tmp/foo?)

So, we will just avoid the problem by extracting the name
of the file, and calling chdir.
*/

int CondorFileTable::fchdir( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	dprintf(D_ALWAYS,"CondorFileTable::fchdir(%d) will try chdir(%s)\n",
		fd, pointers[fd]->file->get_name() );

	return ::chdir( pointers[fd]->file->get_name() );
}

/*
ioctls don't affect the open file table, so we will pass them
along to the individual access method, which will decide
if it can be supported.
*/

int CondorFileTable::ioctl( int fd, int cmd, int arg )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->file->ioctl(cmd,arg);
}

int CondorFileTable::ftruncate( int fd, size_t length )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	/* The below length check is a hack to patch an f77 problem on
	   OSF1.  - Jim B. */

	if( length<0 ) return 0;

	if(buffer) buffer->flush(pointers[fd]->file);

	return pointers[fd]->file->ftruncate(length);
}

/*
fcntl does all sorts of wild stuff.
Some operations affect the fd table.
Perform those here.  Others merely modify
an individual file.  Pass these along to
the access method, which may support the operation,
or fail with its own error.
*/

int CondorFileTable::fcntl( int fd, int cmd, int arg )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	switch(cmd) {
		#ifdef F_DUPFD
		case F_DUPFD:
		#endif
			return search_dup2(fd,arg);

		#ifdef F_DUP2FD
		case F_DUP2FD:
		#endif
			return dup2(fd,arg);

		default:
			return pointers[fd]->file->fcntl(cmd,arg);
			break;
	}
}

int CondorFileTable::fsync( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	if(buffer) buffer->flush(pointers[fd]->file);

	pointers[fd]->file->fsync();
}

int CondorFileTable::poll( struct pollfd *fds, int nfds, int timeout )
{
	struct pollfd *realfds;

	realfds = (struct pollfd *) malloc( sizeof(struct pollfd)*nfds );
	if(!realfds) {
		errno = ENOMEM;
		return -1;
	}

	for( int i=0; i<nfds; i++ ) {
		if(_condor_file_is_local(fds[i].fd)) {
			realfds[i].fd = _condor_file_table_map(fds[i].fd);
			realfds[i].events = fds[i].events;
		} else {
			realfds[i].fd = -1;
		}
	}

	int scm = SetSyscalls( SYS_LOCAL|SYS_UNMAPPED );
	int result = ::poll( realfds, nfds, timeout );
	SetSyscalls(scm);

	for( int i=0; i<nfds; i++ ) {
		fds[i].revents = realfds[i].revents;
	}

	free(realfds);

	return result;
}

void CondorFileTable::report_info()
{
	if( MyImage.GetMode()!=STANDALONE ) {
		REMOTE_syscall( CONDOR_report_file_info, read_count, read_bytes, write_count, write_bytes, seek_count, actual_read_count, actual_read_bytes, actual_write_count, actual_write_bytes );
	}
}

void CondorFileTable::checkpoint()
{
	dprintf(D_ALWAYS,"CondorFileTable::checkpoint\n");

	dump();

	if( MyImage.GetMode() != STANDALONE ) {
		REMOTE_syscall( CONDOR_getwd, working_dir );
	} else {
		::getcwd( working_dir, _POSIX_PATH_MAX );
	}

	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	for( int i=0; i<length; i++ )
	     if( pointers[i] ) pointers[i]->file->checkpoint();

	report_info();
}

void CondorFileTable::suspend()
{
	dprintf(D_ALWAYS,"CondorFileTable::suspend\n");

	dump();

	if( MyImage.GetMode() != STANDALONE ) {
		REMOTE_syscall( CONDOR_getwd, working_dir );
	} else {
		::getcwd( working_dir, _POSIX_PATH_MAX );
	}

	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	for( int i=0; i<length; i++ )
	     if( pointers[i] ) pointers[i]->file->suspend();

	report_info();
}

void CondorFileTable::resume()
{
	int result;

	resume_count++;

	dprintf(D_ALWAYS,"CondorFileTable::resume_count=%d\n");
	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	if(MyImage.GetMode()!=STANDALONE) {
		result = REMOTE_syscall(CONDOR_chdir,working_dir);
	} else {
		result = ::chdir( working_dir );
	}

	if(result<0) _condor_file_warning("After checkpointing, I couldn't find %s again!",working_dir);

	/* Resume works a little differently, depending on the image mode.
	   In the standard condor universe, we just go through each file
	   object and call its resume method to reopen the file. */

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {

			/* No matter what the mode, we tell the file to
			   resume itself. */

			pointers[i]->file->resume(resume_count);

			/* In standalone mode, we check to see if fd i shares
			   an fp with a lower numbered fd.  If it does, then
			   we need to dup the lower number into the upper number.
			   Just like dup2, we need to check that the result of
			   the syscall is what we expected. */

			if( MyImage.GetMode()==STANDALONE ) {
				for( int j=0; j<i; j++ ) {
					if( pointers[j]==pointers[i] ) {
						int result = _condor_internal_dup2(j,i);
						if(result!=i) _condor_file_warning("internal_dup2(%d,%d) returned %d but I wanted %d!\n",j,i,result,i);
						break;
					}
				}
			}	
		}
	}

	dump();
}

int CondorFileTable::map_fd_hack( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->file->map_fd_hack();
}

int CondorFileTable::local_access_hack( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->file->local_access_hack();
}



