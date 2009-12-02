#include <stdio.h>
#include <stdlib.h>
#include "dmtcpaware.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C entry point */
void ckpt_and_exit(void)
{
	if (dmtcpCheckpoint () == DMTCP_AFTER_CHECKPOINT) {
		exit (EXIT_SUCCESS);
	}
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
