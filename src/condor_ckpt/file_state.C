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
#include "file_types.h"
#include "file_table_interf.h"

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

	pre_open( 0, 1, 0, 0 );
	pre_open( 1, 0, 1, 0 );
	pre_open( 2, 0, 1, 0 );
}

static void _condor_flush_and_disable_buffer()
{
	_condor_file_table_init();
	FileTab->flush();
	FileTab->disable_buffer();
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

	// If the program ends via exit() or return from main,
	// then flush the buffers.  Furthermore, disable any further buffering,
	// because iostream or stdio will flush _after_ us.

	if(atexit(_condor_flush_and_disable_buffer)<0) {
		_condor_file_warning("atexit() failed.  Buffering is disabled.\n");
		delete buffer;
		buffer = 0;
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
				i,pointers[i]->get_offset(),
				pointers[i]->get_use_count());
			pointers[i]->get_file()->dump();
		}
		dprintf(D_ALWAYS,"\n");
	}
}

int CondorFileTable::pre_open( int fd, int readable, int writeable, int is_remote )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]) ) {
		errno = EBADF;
		return -1;
	}

	CondorFile *f;

	if( is_remote ) f = new CondorFileRemote;
	else f = new CondorFileLocal;

	f->force_open(fd,readable,writeable);
	f->add_user();

	pointers[fd] = new CondorFilePointer(f);
	pointers[fd]->add_user();

	return fd;	
}

int CondorFileTable::find_name( const char *path )
{
	for( int fd=0; fd<length; fd++ ) {
		if( pointers[fd] && !strcmp(pointers[fd]->get_file()->get_name(),path) ) {
			return fd;
		}
	}
	return -1;
}

int CondorFileTable::find_empty()
{
	for( int fd=0; fd<length; fd++ ) {
		if( !pointers[fd] ) return fd;
	}
	return -1;
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

		char full_path[_POSIX_PATH_MAX];

		init_buffer();

		if(path[0]=='/') {
			strcpy(full_path,path);
		} else {
			REMOTE_syscall( CONDOR_getwd, full_path );
			strcat(full_path,"/");
			strcat(full_path,path);
		}

		dprintf(D_ALWAYS,"full_path = %s\n", full_path );
					 
		kind = REMOTE_syscall( CONDOR_file_info, full_path, &new_fd, new_path );
		int match = find_name(new_path);
		if( match>=0 ) {

			// A file with this remapped name is already open,
			// so share the file object with the existing pointer.

			f = pointers[match]->get_file();

		} else {

			// The file is not already open, so create a new file object

			switch( kind ) {
				case IS_PRE_OPEN:
				f = new CondorFileLocal;
				f->force_open( new_fd, 1, 1 );
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
		}
	}

	// Opening files O_RDWR is not safe across checkpoints
	// However, there is no problem as far as the rest of the
	// file code is concerned.  We will catch it here,
	// but the rest of the file table and buffer has no problem.

	if( flags & O_RDWR )
		_condor_file_warning("Opening file '%s' for read and write is not safe in a program that may be checkpointed!  You should use separate files for reading and writing.\n",path);

	// Install a new fp and update the use counts 

	pointers[fd] = new CondorFilePointer(f);
	pointers[fd]->add_user();
	pointers[fd]->get_file()->add_user();

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

	fa->force_open(real_fds[0],1,1);
	fa->add_user();

	fb->force_open(real_fds[1],1,1);
	fb->add_user();

	pa->add_user();
	pb->add_user();

	return 0;
}
	
/* 
Close is a little tricky.
Find the fp corresponding to the fd.
If I am the last user of this fp:
	Find the file corresponding to this fp.
	If I am the last user of this file:
		Flush the buffer
		Delete the file
	Delete the fp
In any case, zero the appropriate entry of the table.
*/

int CondorFileTable::close( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}
	
	pointers[fd]->remove_user();
	if( pointers[fd]->get_use_count()<=0 ) {
		pointers[fd]->get_file()->remove_user();
		if( pointers[fd]->get_file()->get_use_count()<=0 ) {
			if(buffer) buffer->flush(pointers[fd]->get_file());
			pointers[fd]->get_file()->close();
			delete pointers[fd]->get_file();
		}
		delete pointers[fd];
	}

	pointers[fd] = 0;

	return 0;
}


ssize_t CondorFileTable::read( int fd, void *data, size_t nbyte )
{
	if( (fd>=length) || (fd<0) || (pointers[fd]==0) ||
	    (!pointers[fd]->get_file()->is_readable()) ) {
		errno = EBADF;
		return -1;
	}
	
	if( (!data) || (nbyte<0) ) {
		errno = EINVAL;
		return -1;
	}

	CondorFilePointer *fp = pointers[fd];
	CondorFile *f = fp->get_file();

	// First, figure out the appropriate place to read from.

	int offset = fp->get_offset();
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
	fp->set_offset(offset+actual);

	// Return the number of bytes read
	return actual;
}

ssize_t CondorFileTable::write( int fd, const void *data, size_t nbyte )
{
	if( (fd>=length) || (fd<0) || (pointers[fd]==0) ||
	    (!pointers[fd]->get_file()->is_writeable()) ) {
		errno = EBADF;
		return -1;
	}

	if( (!data) || (nbyte<0) ) {
		errno = EINVAL;
		return -1;
	}

	if( nbyte==0 ) return 0;

	CondorFilePointer *fp = pointers[fd];
	CondorFile *f = fp->get_file();

	// First, figure out the appropriate place to write to.

	int offset = fp->get_offset();
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

	fp->set_offset(offset+actual);

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
	CondorFile *f = fp->get_file();
	int temp;

	// Compute the new offset first.
	// If the new offset is illegal, don't change it.

	if( whence == SEEK_SET ) {
		temp = offset;
	} else if( whence == SEEK_CUR ) {
	        temp = fp->get_offset()+offset;
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
		fp->set_offset(temp);
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

	pointers[fd]->add_user();
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
		fd, pointers[fd]->get_file()->get_name() );

	return ::chdir( pointers[fd]->get_file()->get_name() );
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

	return pointers[fd]->get_file()->ioctl(cmd,arg);
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

	buffer->flush(pointers[fd]->get_file());

	return pointers[fd]->get_file()->ftruncate(length);
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
			return pointers[fd]->get_file()->fcntl(cmd,arg);
			break;
	}
}

int CondorFileTable::fsync( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	if(buffer) buffer->flush(pointers[fd]->get_file());

	pointers[fd]->get_file()->fsync();
}

void CondorFileTable::checkpoint()
{
	dprintf(D_ALWAYS,"CondorFileTable::checkpoint\n");

	dump();

	int scm = GetSyscallMode();
	getcwd( local_working_dir, _POSIX_PATH_MAX );
	SetSyscalls(scm);

	if( MyImage.GetMode() != STANDALONE )
		REMOTE_syscall( CONDOR_getwd, remote_working_dir );
	else
		remote_working_dir[0]=0;

	dprintf(D_ALWAYS,"local wd=%s\nremote wd=%s\n",local_working_dir,remote_working_dir);

	for( int i=0; i<length; i++ )
	     if( pointers[i] ) pointers[i]->get_file()->checkpoint();
}

void CondorFileTable::suspend()
{
	dprintf(D_ALWAYS,"CondorFileTable::suspend\n");

	dump();

	int scm = GetSyscallMode();
	getcwd( local_working_dir, _POSIX_PATH_MAX );
	SetSyscalls(scm);

	if( MyImage.GetMode() != STANDALONE )
		REMOTE_syscall( CONDOR_getwd, remote_working_dir );
	else
		remote_working_dir[0]=0;

	dprintf(D_ALWAYS,"local wd=%s\nremote wd=%s\n",local_working_dir,remote_working_dir);

	for( int i=0; i<length; i++ )
	     if( pointers[i] ) pointers[i]->get_file()->suspend();
}

void CondorFileTable::resume()
{
	int result;

	resume_count++;

	dprintf(D_ALWAYS,"CondorFileTable::resume_count=%d\n");
	dprintf(D_ALWAYS,"local wd=%s\nremote wd=%s\n",local_working_dir,remote_working_dir);
	result = syscall( SYS_chdir, local_working_dir );

	if(result<0) _condor_file_warning("After checkpointing, I couldn't find %s again!",local_working_dir);

	if(MyImage.GetMode()!=STANDALONE) {
		result = REMOTE_syscall(CONDOR_chdir,remote_working_dir);
	}

	if(result<0) _condor_file_warning("After checkpointing, I couldn't find %s again!",remote_working_dir);

	/* Resume works a little differently, depending on the image mode.
	   In the standard condor universe, we just go through each file
	   object and call its resume method to reopen the file. */

	for( int i=0; i<length; i++ ) {
		if( pointers[i] ) {

			/* No matter what the mode, we tell the file to
			   resume itself. */

			pointers[i]->get_file()->resume(resume_count);

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

	return pointers[fd]->get_file()->map_fd_hack();
}

int CondorFileTable::local_access_hack( int fd )
{
	if( (fd<0) || (fd>=length) || (pointers[fd]==0) ) {
		errno = EBADF;
		return -1;
	}

	return pointers[fd]->get_file()->local_access_hack();
}


