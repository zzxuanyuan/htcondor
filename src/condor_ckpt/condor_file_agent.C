
#include "condor_file_agent.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"

#define KB 1024
#define TRANSFER_BLOCK_SIZE (512*KB)

static char buffer[TRANSFER_BLOCK_SIZE];

CondorFileAgent::CondorFileAgent( CondorFile *file )
{
	init();
	kind = "local copy";
	strcpy(name,file->get_name());
	original = file;
	readable = original->is_readable();
	writeable = original->is_writeable();
	seekable = 1;
	bufferable = 0;
	size = original->get_size();
	open_temp();
	pull_data();
}

CondorFileAgent::~CondorFileAgent()
{
	delete original;
}

int CondorFileAgent::close()
{
	push_data();
	close_temp();
	return original->close();
}

void CondorFileAgent::report_file_info()
{
	original->report_file_info();
}

void CondorFileAgent::checkpoint()
{
	push_data();
	original->checkpoint();
}

void CondorFileAgent::suspend()
{
	push_data();
	close_temp();
	original->suspend();
}

void CondorFileAgent::resume( int count )
{
	original->resume( count );

	if( (count==resume_count) || forced ) return;
	resume_count = count;

	open_temp();
	pull_data();
}

void CondorFileAgent::open_temp()
{
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	tmpnam(local_name);
	SetSyscalls(scm);

	dprintf(D_ALWAYS,"CondorFileAgent: %s is local copy of %s %s\n",
		local_name, original->get_kind(), original->get_name() );

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	fd = ::open( local_name, O_RDWR|O_CREAT|O_TRUNC, 0700 );
	if(fd<0) abort("open");
	SetSyscalls(scm);
}

void CondorFileAgent::close_temp()
{
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::close(fd);
	SetSyscalls(scm);

	fd = -1;

	dprintf(D_ALWAYS,"CondorFileAgent: %s is closed.\n",local_name);
}

void CondorFileAgent::pull_data()
{
	readable = original->is_readable();
	writeable = original->is_writeable();
	bufferable = 0;
	seekable = 1;

	if(!original->is_readable()) return;

	dprintf(D_ALWAYS,"CondorFileAgent: Loading %s with %s %s\n",
		local_name, original->get_kind(), original->get_name() );

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);

	int pos=0,chunk=0,result=0;

	do {
		chunk = original->read(pos,buffer,TRANSFER_BLOCK_SIZE);
		if(chunk<0) abort("read");

		if(chunk==0) break;

		result = ::write(fd,buffer,chunk);
		if(result<0) original->abort("write");
		
		pos += chunk;
	} while(chunk==TRANSFER_BLOCK_SIZE);

	SetSyscalls(scm);
}

void CondorFileAgent::push_data()
{
	if(!original->is_writeable()) return;

	dprintf(D_ALWAYS,"CondorFileAgent: Putting %s back into %s %s.\n",
		local_name, original->get_kind(), original->get_name());

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);

	::lseek(fd,0,SEEK_SET);

	int pos=0,chunk=0,result=0;

	do {
		chunk = ::read(fd,buffer,TRANSFER_BLOCK_SIZE);
		if(chunk<0) abort("read");

		if(chunk==0) break;

		result = original->write(pos,buffer,chunk);
		if(result<0) original->abort("write");
			
		pos += chunk;
	} while(chunk==TRANSFER_BLOCK_SIZE);

	SetSyscalls(scm);
}
