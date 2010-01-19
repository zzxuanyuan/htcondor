#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "dmtcpaware.h"

#ifdef __cplusplus
extern "C" {
#endif

void post_ckpt_atomic_exit(void);

/* This gets called by DMTCP after the checkpoints are finished and before
	returning to user code. Any changes to the process state I make in 
	here are lost, If USR1 was actually pending when I got in here because
	the application is actually using it, it'll still
	be pending when the checkpoint restarts, even though I reimagined
	the semantics of that signal in this function. We use USR1 here 
	because USR2 is utilized inside DMTCP. */
void post_ckpt_atomic_exit(void)
{
	sigset_t mask;
	struct sigaction sa;

	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);

	/* reset the handler, and I don't need to preserve the previous one since
		the process is already checkpointed. */
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, NULL);
	
	pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

	/* default handler will kill us so we exit with the correct status */
	raise(SIGUSR1);
	while(1) { }
}

/* C entry point */
void ckpt_and_exit(void)
{
	int ret; 

	if (!dmtcpIsEnabled()) {
		return;
	}

	dmtcpInstallHooks(NULL, post_ckpt_atomic_exit, NULL);

	/* The checkpointing thread will exit in the post checkpoint handler
		after all threads are stopped and checkpointed */
	ret = dmtcpCheckpoint();

	/* however, we will maintain a barrier here for the invoking thread until
		the restart happens (and ret changes to a different constant).
		This prevents the thread which induced the checkpointing from
		continuing beyond this point.  In sequential programs, it ia assumed
		that control flow stops at Condor's ckpt_and_exit().
		XXX add in blocked signals too. */
	
	while(ret == DMTCP_AFTER_CHECKPOINT) {
		pause();
	}

	dmtcpInstallHooks(NULL, NULL, NULL);
}

/* fortran entry point */
void ckpt_and_exit_(void)
{
	ckpt_and_exit();
}

/* fortran entry point */
void ckpt_and_exit__(void)
{
	ckpt_and_exit();
}

/* C entry point */
void ckpt(void)
{
	dmtcpCheckpoint();
}

/* fortran entry point */
void ckpt_(void)
{
	ckpt();
}

/* fortran entry point */
void ckpt__(void)
{
	ckpt();
}

#ifdef __cplusplus
}
#endif
