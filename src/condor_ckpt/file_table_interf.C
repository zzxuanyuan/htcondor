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

#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "condor_debug.h"

#include "file_table_interf.h"
#include "file_state.h"

// XXX What are the header files for these?

extern "C" sigset_t block_condor_signals(void);
extern "C" void restore_condor_sigmask(sigset_t omask);

extern "C" {

/*
This function is going the way of the dinosaur.  Operations on fds
need to pass through the file table so's we can redirect them to the
appropriate agent on a per-file basis.  An fd will not always map
to another fd in the same medium.

However, for system calls that are not routed through the open
file table, we'll provide this for now...
*/

int MapFd( int user_fd )
{
	if( MappingFileDescriptors() ) {
		return FileTab->map_fd_hack(user_fd);
	} else {
		return user_fd;
	}
}

int LocalAccess( int user_fd )
{
	if( MappingFileDescriptors() ) {
		return FileTab->local_access_hack(user_fd);
	} else {
		return LocalSysCalls();
	}
}

void DumpOpenFds()
{
	InitFileState();
	FileTab->dump();
}

void CheckpointFileState()
{
	InitFileState();
	FileTab->checkpoint();
}

void SuspendFileState()
{
	InitFileState();
	FileTab->suspend();
}

void ResumeFileState()
{
	InitFileState();
	FileTab->resume();
}

void InitFileState()
{
	if(!FileTab) {
		FileTab = new OpenFileTable();
		FileTab->init();
	}
}

int pre_open( int fd, int readable, int writable, int is_remote )
{
	InitFileState();
	sigset_t sigs = block_condor_signals();
	int result = FileTab->pre_open(fd,readable,writable,is_remote);
	restore_condor_sigmask(sigs);
}

int creat(const char *path, mode_t mode)
{
	return open((char*)path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

#if defined(OSF1)

/* Force isatty() to be undefined so programs that use it get it from
   the condor library rather than libc.a. */

int _condor_force_isatty_hack()
{
	return isatty(0);
}

/*
  This is some kind of cleanup routine for dynamically linked programs which
  is called by exit.  For some reason it occasionally cuases a SEGV
  when mixed with the condor checkpointing code.  Since condor programs
  are always statically linked, we just make a dummy here to avoid
  the problem.
*/
void ldr_atexit() {}

#endif

int _open( const char *path, int flags, ... )
{
	va_list ap;
	int rval,mode;
	va_start( ap, flags );
	mode = va_arg( ap, int );
	rval = open( path, flags, mode );
	va_end( ap );
	return rval;
}

int __open( const char *path, int flags, ... )
{
	va_list ap;
	int rval,mode;
	va_start( ap, flags );
	mode = va_arg( ap, int );
	rval = open( path, flags, mode );
	va_end( ap );
	return rval;
}

int _close( int fd )
{
	return close(fd);
}

int __close( int fd )
{
	return close(fd);
}
int open64( const char* path, int oflag, ... ) {
	va_list ap;
	int rval;
	va_start( ap, oflag );
	rval = open( path, oflag, ap );
	va_end( ap );
	return rval;
}

int _open64( const char* path, int oflag, ... ) {
	va_list ap;
	int rval;
	va_start( ap, oflag );
	rval = open( path, oflag, ap );
	va_end( ap );
	return rval;
}

int __open64( const char* path, int oflag, ... ) {
	va_list ap;
	int rval;
	va_start( ap, oflag );
	rval = open( path, oflag, ap );
	va_end( ap );
	return rval;
}


} // extern "C"
