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
	prefetch_size = 0;
	resume_count = 0;
	got_buffer_info = 0;

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

	if(buffer) return;

	if(got_buffer_info) return;
	got_buffer_info = 1;

	if(REMOTE_syscall(CONDOR_get_buffer_info,&blocks,&blocksize,&prefetch_size)<0) {
		dprintf(D_ALWAYS,"get_buffer_info failed!");
		buffer = 0;
		return;
	}

	buffer = new CondorBufferCache( blocks, blocksize );

	dprintf(D_ALWAYS,"Buffer cache is %d blocks of %d bytes.\n",blocks,blocksize);
	dprintf(D_ALWAYS,"Prefetch size is %d\n",prefetch_size);

	if( !blocks || !blocksize ) {
		dprintf(D_ALWAYS,"Buffer is disabled\n.");
		buffer = 0;
		return;
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

int CondorFileTable::open( const char *path, int flags, int mode )
{
	int	x,kind,success;
	int	fd, new_fd;
	char	new_path[_POSIX_PATH_MAX];
	CondorFile	*f;

	// Find an open slot in the table

	fd = find_empty();
	if( fd<0 ) {
		errno = EMFILE;
		return -1;
	}

	if( LocalSysCalls() ) {

		f = new CondorFileLocal;
		if( f->open(path,flags,mode)<0 ) {
			delete f;
			return -1;
		}

	} else {

		init_buffer();

		char full_path[_POSIX_PATH_MAX];

		if(path[0]=='/') {
			strcpy(full_path,path);
		} else {
			REMOTE_syscall( CONDOR_getwd, full_path );
			strcat(full_path,"/");
			strcat(full_path,path);
		}

		// Always ask the shadow what to do with this file.

		kind = REMOTE_syscall( CONDOR_file_info, full_path, &new_fd, new_path );

		// Check to see if this file is already open

		int match = find_name(full_path);
		if(match>=0) {

			// If so, share the file object

			f = pointers[match]->file;

		} else {

			// Otherwise, create a new file object 

			switch( kind ) {
				case IS_PRE_OPEN:
				f = new CondorFileLocal;
				f->force_open( new_fd, "inherited stream", 1, 1 );
				break;

				case IS_NFS:
				case IS_AFS:
				case IS_LOCAL:
				f = new CondorFileLocal;
				break;

				case IS_RSC:
				default:
				f = new CondorFileRemote;
				f->enable_buffer();
				break;
			}

			// Do not buffer stderr
			if(fd==2) f->disable_buffer();

			// Open according to the access method.
			// Forced files are not re-opened.

			if( (kind!=IS_PRE_OPEN) && (f->open(new_path,flags,mode)<0) ) {
				delete f;
				return -1;
			}
			
			// If we want the whole file to be downloaded
			// and then accessed locally, then wrap an
			// agent around the file.  The agent is responsible
			// for uploading and downloading when necessary.

			// if( kind==IS_RSC ) {
			// 	f = new CondorFileAgent(f);
			//}
		}
	}

	// Opening files O_RDWR is not safe across checkpoints
	// However, there is no problem as far as the rest of the
	// file code is concerned.  We will catch it here,
	// but the rest of the file table and buffer has no problem.

	if( flags & O_RDWR )
		_condor_file_warning("Opening file '%s' for read and write is not safe in a program that may be checkpointed!  You should use separate files for reading and writing.",path);

	pointers[fd] = new CondorFilePointer(f);

	// Prefetch as allowed

	if( f->ok_to_buffer() && buffer && (prefetch_size>0) )
		buffer->prefetch(f,0,prefetch_size);

	return fd;
}

/*
There is a limited support for pipes as special endpoints.
We will create the pipe locally, install placeholders, and get out of
the way.  A CondorFileSpecial object is just like a CondorFileLocal, but it
cannot be checkpointed.
*/

int CondorFileTable::pipe(int fds[])
{
	int real_fds[2];
	CondorFileSpecial *a, *b;

	if(RemoteSysCalls()) {
		_condor_file_warning("pipe() is unsupported.\n");
		errno = EINVAL;
		return -1;
	}

	fds[0] = find_empty();
	fds[1] = find_empty();

	if( (fds[0]<0) || (fds[1]<0) ) {
		errno = EMFILE;
		return -1;
	}

	CondorFileSpecial *fa = new CondorFileSpecial("pipe endpoint");
	if(!fa) {
		errno = ENOMEM;
		return -1;
	}

	CondorFileSpecial *fb = new CondorFileSpecial("pipe endpoint");
	if(!fb) {
		delete fa;
		errno = ENOMEM;
		return -1;
	}

	CondorFilePointer *pa = new CondorFilePointer(fa);
	if(!pa) {
		delete fa;
		delete fb;
		errno = ENOMEM;
		return -1;
	}

	CondorFilePointer *pb = new CondorFilePointer(fb);
	if(!pb) {
		delete fa;
		delete fb;
		delete pa;
		errno = ENOMEM;
		return -1;
	}

	if(pipe(real_fds)<0) {
		delete fa;
		delete fb;
		delete pa;
		delete pb;
		return -1;
	}

	pointers[fds[0]] = pa;
	pointers[fds[1]] = pb;

	fa->force_open(real_fds[0],"unnamed",1,1);
	fb->force_open(real_fds[1],"unnamed",1,1);

	return 0;
}

/*
Socket is similar to pipe.  We will create an anonymous endpoint that
can be used just like a local file.  If a checkpoint is performed on
the endpoint, an error will occur.
*/

int CondorFileTable::socket( int domain, int type, int protocol )
{
	int fd = find_empty();
	if(fd<0) {
		errno = EMFILE;
		return -1;
	}

	CondorFileSpecial *f = new CondorFileSpecial("socket");
	if(!f) {
		errno = ENOMEM;
		return -1;
	}

	CondorFilePointer *fp = new CondorFilePointer(f);
	if(!fp) {
		errno = ENOMEM;
		delete f;
		return -1;
	}

	int scm = SetSyscalls( SYS_LOCAL|SYS_UNMAPPED ); 
	int real_fd = ::socket(domain,type,protocol);
	SetSyscalls(scm);
	if(!real_fd) {
		delete fp;
		delete f;
		return -1;
	}

	f->force_open( real_fd, "socket", 1, 1 );
	pointers[fd] = fp;

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
		if(buffer) buffer->flush(file);
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

	if( (offset>=size) || (nbyte==0) ) return 0;

	// If buffering is allowed, read from the buffer,
	// otherwise, go straight to the access method

	if( f->ok_to_buffer() && buffer ) {

		// The buffer doesn't know about file sizes,
		// so cut off extra long reads here

		if( (offset+nbyte)>size ) nbyte = size-offset;

		// Now fetch from the buffer

		actual = buffer->read( f, offset, (char*)data, nbyte );


	} else {
		actual = f->read( offset, (char*)data, nbyte );
	}

	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Update the offset
	fp->offset = offset+actual;

	// Return the number of bytes read
	return actual;
}

ssize_t CondorFileTable::write( int fd, const void *data, size_t nbyte )
{
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
		actual = f->write( offset, (char*)data, nbyte );
	}

	// If there is an error, don't touch the offset.
	if(actual<0) return -1;

	// Update the offset, and mark the file bigger if necessary
	if( (offset+actual)>f->get_size() )
		f->set_size(offset+actual);

	fp->offset = offset+actual;

	// Return the number of bytes written
	return actual;
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

	buffer->flush(pointers[fd]->file);

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

void CondorFileTable::checkpoint()
{
	dprintf(D_ALWAYS,"CondorFileTable::checkpoint\n");

	dump();

	if( MyImage.GetMode() != STANDALONE ) {
		REMOTE_syscall( CONDOR_getwd, working_dir );
	} else {
		getcwd( working_dir, _POSIX_PATH_MAX );
	}

	dprintf(D_ALWAYS,"working dir = %s\n",working_dir);

	for( int i=0; i<length; i++ )
	     if( pointers[i] ) pointers[i]->file->checkpoint();
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



