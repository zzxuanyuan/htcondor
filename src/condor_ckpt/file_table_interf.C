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
#include "signals_control.h"

extern "C" {

/*
This function is going the way of the dinosaur.  Operations on fds
need to pass through the file table so's we can redirect them to the
appropriate agent on a per-file basis.  An fd will not always map
to another fd in the same medium.

However, for system calls that are not routed through the open
file table, we'll provide this for now...
*/

int _condor_file_table_map( int user_fd )
{
	if( MappingFileDescriptors() ) {
		return FileTab->map_fd_hack(user_fd);
	} else {
		return user_fd;
	}
}

int _condor_file_is_local( int user_fd )
{
	if( MappingFileDescriptors() ) {
		return FileTab->local_access_hack(user_fd);
	} else {
		return LocalSysCalls();
	}
}

void _condor_file_table_dump()
{
	_condor_file_table_init();
	FileTab->dump();
}

void _condor_file_table_checkpoint()
{
	_condor_file_table_init();
	FileTab->checkpoint();
}

void _condor_file_table_suspend()
{
	_condor_file_table_init();
	FileTab->suspend();
}

void _condor_file_table_resume()
{
	_condor_file_table_init();
	FileTab->resume();
}

void _condor_file_table_init()
{
	if(!FileTab) {
		FileTab = new CondorFileTable();
		FileTab->init();
	}
}

int _condor_file_pre_open( int fd, char *name, int readable, int writable, int is_remote )
{
	_condor_file_table_init();
	_condor_signals_disable();
	int result = FileTab->pre_open(fd,name,readable,writable,is_remote);
	_condor_signals_enable();
	return result;
}

void _condor_file_table_cleanup()
{
	_condor_file_table_init();
	FileTab->flush();
	FileTab->disable_buffer();
	FileTab->close_all();
}

int creat(const char *path, mode_t mode)
{
	return open((char*)path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

#ifdef SYS_open64
int creat64(const char *path, mode_t mode)
{
	return open64((char*)path, O_WRONLY | O_CREAT | O_TRUNC, mode );
}
#endif

#if defined(OSF1)
/*
  This is some kind of cleanup routine for dynamically linked programs which
  is called by exit.  For some reason it occasionally cuases a SEGV
  when mixed with the condor checkpointing code.  Since condor programs
  are always statically linked, we just make a dummy here to avoid
  the problem.
*/

void ldr_atexit() {}
#endif

int isatty( int fd )
{
	if(fd<3) return 1;
	else return 0;
}

int _isatty( int fd )
{
	return isatty(fd);
}

int __isatty( int fd )
{
	return isatty(fd);
}

#if defined(Solaris)
int _so_socket( int a, int b, int c, int d, int e )
{
	int result;
	sigset_t sigs;

	_condor_signals_disable();

	if(MappingFileDescriptors()) {
		_condor_file_table_init();
		result = FileTab->socket(a,b,c);
	} else {
		if(LocalSysCalls()) {
			result = syscall( SYS_so_socket, a, b, c, d, e );
		} else {
			result = REMOTE_syscall( CONDOR_socket, a, b, c );
		}
	}

	_condor_signals_enable();

	return result;
}

#endif

} // extern "C"
