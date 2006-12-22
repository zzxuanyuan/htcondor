#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_td.h"
#include "extArray.h"
#include "condor_classad.h"
#include "MyString.h"

// This handler is called when a client wishes to read files from the
// transferd's storage.
int
TransferD::read_files_handler(int cmd, Stream *sock) 
{
	dprintf(D_ALWAYS, "Got TRANSFERD_READ_FILES!\n");

	return CLOSE_STREAM;
}



int
TransferD::read_files_reaper(int tid, int exit_status)
{
}

