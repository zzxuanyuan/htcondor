
#ifndef CONDOR_FILE_SPECIAL_H
#define CONDOR_FILE_SPECIAL_H

#include "condor_file_local.h"

/**
There are lots of items which go into the file table that Condor
doesn't really understand.  In limited situations, we want to support
the use of pipes and sockets and record their use, but they certainly
can't be checkpointed.

This class allows access just like a local file, but checkpoint
and suspend will cause errors.
*/

class CondorFileSpecial : public CondorFileLocal {
public:
	CondorFileSpecial( char *kind );

	/* These methods will cause the program to die. */
	virtual void checkpoint();
	virtual void suspend();
};

#endif

