#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "MyString.h"
#include "condor_td.h"

void
TransferD::reconfig(void)
{
}

void
TransferD::shutdown_fast(void)
{
}

void
TransferD::shutdown_graceful(void)
{
}

void
TransferD::transferd_exit(void)
{
}

int
TransferD::dump_state_handler(int cmd, Stream *sock)
{
	ClassAd state;
	MyString tmp;

	dprintf(D_ALWAYS, "Got a DUMP_STATE!\n");

	// what uid am I running under?
	tmp.sprintf("Uid = %d", getuid());
	state.InsertOrUpdate(tmp.Value());

	// count how many pending requests I've had
	tmp.sprintf("OutstandingTransferRequests = %d", m_treqs.getNumElements());
	state.InsertOrUpdate(tmp.Value());

	// add more later

	sock->encode();

	state.put(*sock);

	sock->eom();

	// all done with this stream, so close it.
	return !KEEP_STREAM;
}


int
TransferD::reaper_handler(int pid, int exit_status)
{
	if( WIFSIGNALED(exit_status) ) {
		dprintf( D_ALWAYS, "Process exited, pid=%d, signal=%d\n", pid,
				 WTERMSIG(exit_status) );
	} else {
		dprintf( D_ALWAYS, "Process exited, pid=%d, status=%d\n", pid,
				 WEXITSTATUS(exit_status) );
	}

	return TRUE;
}
