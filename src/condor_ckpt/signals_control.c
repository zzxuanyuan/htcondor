
#include "signals_control.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"

sigset_t _condor_signals_disable()
{
	int sigscm;
	sigset_t mask, omask;

	/* Block signals requesting an exit or checkpoint for duration of
	   system call. */
	sigscm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	sigemptyset( &mask );
	sigaddset( &mask, SIGTSTP );
	sigaddset( &mask, SIGUSR1 );
	sigaddset( &mask, SIGUSR2 );
	if( sigprocmask(SIG_BLOCK,&mask,&omask) < 0 ) {
		dprintf(D_ALWAYS, "sigprocmask failed: %s\n", strerror(errno));
		return omask;
	}
	SetSyscalls( sigscm );
	return omask;
}

void _condor_signals_enable(sigset_t omask)
{
	int sigscm;

	sigscm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	/* Restore previous signal mask - generally unblocks TSTP and USR1 */
	if( sigprocmask(SIG_SETMASK,&omask,0) < 0 ) {
		dprintf(D_ALWAYS, "sigprocmask failed: %s\n", strerror(errno));
		return;
	}
	SetSyscalls( sigscm );
}	

