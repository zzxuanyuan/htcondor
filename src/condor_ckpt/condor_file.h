#ifndef CONDOR_FILE_H
#define CONDOR_FILE_H

#include "condor_common.h"

/**
CondorFile is a virtual class which defines some of the operations that
can be performed on an fd.  Methods of accessing an fd (local,
remote, ioserver, etc.) are built be extending CondorFile.
<p>
Open, close, read, write, checkpoint, suspend, and resume must be
implemented by subclasses of CondorFile.  Esoteric operations
(fcntl, fstat, etc.) are not generally supported, but they
could be added in the same format if we find that users
want to perform them on exotic storage methods.
<p>
Notice that all the operations listed below operate on an fd.
Operations that work on a name instead of an fd are sent off
to the shadow or local system, as appropriate.
<p>
Caveats:
<dir>
<li> Each CondorFile stores a unique integer when a resume() is
performed, so that no file is accidentally resumed twice after
a checkpoint.  (See file_state.h for a instance where this
might happen.)  The open file table must provide a unique
integer every time a resume() is performed.
</dir>
*/

class CondorFile {
public:
	virtual ~CondorFile();

	virtual void dump();
	virtual void init();
	virtual void abort( char *why );

	virtual int open(const char *path, int flags, int mode);
	virtual int close();
	virtual int read(int offset, char *data, int length)=0;
	virtual int write(int offset, char *data, int length)=0;

	virtual int fcntl( int cmd, int arg )=0;
	virtual int ioctl( int cmd, int arg )=0;
	virtual int ftruncate( size_t length )=0; 
	virtual int fsync()=0;

	virtual void checkpoint()=0;
	virtual void suspend()=0;
	virtual void resume(int count)=0;

	int	get_resume_count()	{ return resume_count; }

	int	is_readable()		{ return readable; }
	int	is_writeable()		{ return writeable; }

	void	set_size(size_t s)	{ size = s; }
	int	get_size()		{ return size; }
	char	*get_kind()		{ return kind; }
	char	*get_name()		{ return name; }

	void	enable_buffer()		{ bufferable=1; }
	void	disable_buffer()	{ bufferable=0; }
	int	ok_to_buffer()		{ return bufferable && seekable; }

	/**
	Without performing an actual open, associate this
	object with an existing fd, and mark it readable or writable
	as indicated.
	*/

	int	force_open( int f, char *name, int r, int w );

	/**
	Return the real fd associated with this file.
	Returns -1 if the mapping is not trivial.
	*/

	virtual int map_fd_hack()=0;

	/**
	Returns true if this file can be accessed by
	referring to the fd locally.  Returns false
	otherwise.
	*/

	virtual int local_access_hack()=0;

protected:

	int	fd;		// the real fd used by this file
	char	*kind;		// text describing file type
	char	name[_POSIX_PATH_MAX];

	int	readable;	// can this file be read?
	int	writeable;	// can this file be written?
	int	seekable;	// can this file be seekd?
	int	bufferable;	// should this file be buffered?

	int	size;		// number of bytes in the file
	int	forced;		// was this created with force_open?
	int	resume_count;	// how many times have I been resumed?
};

#endif
