
#include "condor_common.h"
#include "condor_file_special.h"
#include "condor_file_warning.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"

CondorFileSpecial::CondorFileSpecial(char *k)
{
	init();
	kind = k;
}

void CondorFileSpecial::checkpoint()
{
	suspend();
}

void CondorFileSpecial::suspend()
{
	_condor_file_warning("A %s cannot be used across checkpoints.\n",kind);
	Suicide();
}
