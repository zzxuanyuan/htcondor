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

 

#ifndef _FILE_STATE_H
#define _FILE_STATE_H

#include "condor_common.h"
#include "file_types.h"
#include "buffer_cache.h"

class FilePointer;

/**
This class multiplexes number of UNIX file system calls.
The fd's used to index this table are "virtual file descriptors",
and could refer to real fds, remote fds, ioservers, sockets,
or who knows what.
<p>
It only makes sense to use this table when MappingFileDescriptors()
is in effect.  If it is not, just perform syscalls remotely or
locally, as the SyscallMode indicates.
<p>
This class does several things:
<dir> 
	<li> checks validity of vfds  
	<li> multiplexes vfds to file types
	<li> buffers seek pointers   
	<li> talks to the buffer cache
	<li> oversees checkpoint and restore
</dir>

This class does _not_:
<dir>
	<li> Decide whether mapping is in effect.
	     The system call stubs do that.
	<li> Implement read, write, ioctl, etc. for _any_ file.
	     Subclasses of File do that (file_types.C)
	<li> Implement buffering.
	     A BufferCache does that (buffer_cache.C)
	<li> Perform operations on names (i.e. stat()).
	     Those are handled transparently by the syscall switches.
</dir>

The file table has two sub-structures, File and FilePointer.
<p>
Each integer file descriptor (fd) indexes a file pointer (fp) in
the open file table.  Each fp stores a current seek pointer
and a pointer to the file object (fo) associated with that fd.
<p>
So, when a dup is performed, two fds will point to the same fp,
which points to a single fo.  All operations on the same fd will
manipulate the same seek pointer and the same file data.
<pre>
fd  fd
|  /
fp
|
fo
</pre>
<p>
If, however, the same file is opened again, a new fd and new fp
are allocated, but the same file object is shared with the previous
fds.  This allows the separation of seek pointers, but operations on
the third fd will still affect the data present in the first two.
<pre>
fd  fd  fd
|  /    |
fp      fp
|      /
|    /
|  /
fo
</pre>
<p>
Various implementations of File can be found in file_types.[hC].
<p>
When in standalone checkpointing mode, the structure above is
maintained so that we can use the same object and caching sceme.
However, we need to construct dups so that functions which do
not explicitly map the fd will still find the right fo.
<p>
In standalone mode, dup() and dup2() will perform a real
dup syscall.  When the file table is recovered, the lowest
numbered fd referring to an fp will be created using open().
The remaining fds which point to the same fp will be
dup()d to get the same effect.
<p>
Notice that this scheme may reverse the original creation
of the dup, but this should have no effect on the program.
<pre>
fd  fd  fd                 fd  fd fd
  \ |  /     would be      |  /  /
    fp       restored      | / /
    |        like this:    fp
    |                      |
    fo                     fo
</pre>
*/

class OpenFileTable {
public:

	/** Prepare the table for use */
	void	init();

	/** Write out any buffered data */
	void	flush();

	/** Display debug info */
	void	dump();

	/** Configure and use the buffer */
	void	init_buffer();

	/** Turn off buffering */
	void	disable_buffer();

	/** Map a virtual fd to the same real fd.  This is generally only
	    used by the startup code to bootstrap a usable stdin/stdout until
	    things get rolling.  You don't want to use this function unless
	    you want an fd visible by _both_ Condor and the user.  */
	int	pre_open( int fd, int readable, int writable, int is_remote );

	/** If in LocalSyscalls, just perform a UNIX open.  If in
	    RemoteSyscalls, ask the shadow for the appropriate 
	    access method, and then use that method for the open. */
	int	open( const char *path, int flags, int mode );

	/** Close this file with UNIX semantics */
	int	close( int fd );

	/** Read with UNIX semantics */
	ssize_t	read( int fd, void *data, size_t length );

	/** Write with UNIX semantics */
	ssize_t	write( int fd, const void *data, size_t length );
       
	/** Seek with UNIX semantics */
	off_t	lseek( int fd, off_t offset, int whence );

	/** Dup with UNIX semantics */
	int	dup( int old );

	/** Dup2 with UNIX semantics, even if dup2 is not supported
	    on this platform. */
	int	dup2( int old, int nfd );

	/** Similar to dup2, but will dup to any free fd >= search */
	int	search_dup2( int old, int search );

	/** Find the name of this file, and then chdir() to that
	    name.  This is not exactly UNIX semantics. */
	int	fchdir( int fd );

	/** Handle known fcntl values by modifying the table, or passing
	    the value to the appropriate file object.  Unknown fcntls
	    or those with non-integer third arguments fail with a 
	    warning message and EINVAL. */
	int	fcntl( int fd, int cmd, int arg );

	/** See comments for fcntl. */
	int	ioctl( int fd, int cmd, int arg );

	/** Truncate with UNIX semantics */
	int	ftruncate( int fd, size_t length );

	/** Flush any Condor-buffered data on this file, then
	    perform a UNIX fsync/fdsync/fdatasync as appropriate. */
	int	fsync( int fd );
 
	/** Perform a periodic checkpoint. */
	void	checkpoint();

	/** Suspend everything I know in preparation for a vacate and exit. */
	void	suspend();

	/** A checkpoint has resumed, so open everything up again. */
	void	resume();

	/** Returns the real fd corresponding to this virtual
	    fd.  If the mapping is non trivial, -1 is returned. */
	int	map_fd_hack( int fd );

	/** Returns true if this file can be accessed by local
	    syscalls.  Returns false otherwise. */
	int	local_access_hack( int fd );

private:

	int	find_name(const char *path);
	int	find_empty();

	FilePointer	**pointers;
	int		length;
	BufferCache	*buffer;
	char		local_working_dir[_POSIX_PATH_MAX];
	char		remote_working_dir[_POSIX_PATH_MAX];
	int		prefetch_size;
	int		resume_count;
	int		got_buffer_info;
};

/** This is a pointer to the single global instance of the file
    table.  The only user of this pointer should be the system call
    switches, who need to send some syscalls to the open file table. */

extern OpenFileTable *FileTab;

#endif



