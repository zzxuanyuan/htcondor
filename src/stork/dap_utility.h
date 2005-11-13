#ifndef _DAP_UTILITY_H
#define  _DAP_UTILITY_H

#include "condor_common.h"


void parse_url(char *url, char *protocol, char *host, char *filename);
char *strip_str(char *str);

// Create a predictable unique path, given a directory, basename, job id, and
// pid.  The return value points to a statically allocated string.  This
// function is not reentrant.
const char *
job_filepath(
		const char *basename,
		const char *suffix,
		const char *dap_id,
		pid_t pid
);

#endif
