
#ifndef CONDOR_FILE_AGENT_H
#define CONDOR_FILE_AGENT_H

#include "condor_file_local.h"

/**
This object takes an existing CondorFile and arranges for
the entire thing to be moved to the local machine, where
it can be accessed locally.

When the file is suspended, it is put back in its original
place.  When the file is resumed, it is retreived again.
*/

class CondorFileAgent : public CondorFileLocal {
public:
	CondorFileAgent( CondorFile *f );
	~CondorFileAgent();

	virtual int close();

	virtual void report_file_info();

	virtual void checkpoint();
	virtual void suspend();
	virtual void resume(int count);

private:
	void open_temp();
	void close_temp();
	void pull_data();
	void push_data();

	CondorFile *original;
	char local_name[_POSIX_PATH_MAX];
};

#endif
