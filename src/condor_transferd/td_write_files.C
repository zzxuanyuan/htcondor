#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_td.h"
#include "extArray.h"
#include "condor_classad.h"
#include "MyString.h"

// This handler is called when a client wishes to write files from the
// transferd's storage.
int
TransferD::write_files_handler(int cmd, Stream *sock) 
{
	dprintf(D_ALWAYS, "Got TRANSFERD_WRITE_FILES!\n");

	// soak the number of jobids to come.

	sock->eom();

	// soak the array of job ids

	sock->eom();

	// find the transfer request which is associated with all of these jobids
	// and associate that with the thread I'm about to start.

	// now create a thread, passing in the sock, which uses the file transfer
	// object to accept the files.

/*
            if ( transfer_reaper_id == -1 ) {
                transfer_reaper_id = daemonCore->Register_Reaper(
                        "write_files_reaper",
                        (ReaperHandlercpp) &Scheduler::write_files_reaper,
                        "write_files_reaper",
                        this
                    );
            }

            // Start a new thread (process on Unix) to do the work
            tid = daemonCore->Create_Thread(
                    (ThreadStartFunc) &Scheduler::write_files_thread,
                    (void *)thread_arg,
                    s,
                    transfer_reaper_id
                    );

*/
	// don't forget to associate the tid with the transfer request pointer
	// so the reaper can finish up and delete the transfer request when
	// the thread (fork) finishes.

	return CLOSE_STREAM;
}


int
TransferD::write_files_reaper(int tid, int exit_status)
{
}



