#ifndef _DAP_SERVER_H
#define _DAP_SERVER_H

#include "condor_common.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_log.c++.h"
#include "dap_constants.h"
#include "sock.h"

// Timers

// Timer to check for, and possibly start idle jobs.
#define STORK_IDLE_JOB_MONITOR					"STORK_IDLE_JOB_MONITOR"
#define STORK_IDLE_JOB_MONITOR_DEFAULT			10
#define STORK_IDLE_JOB_MONITOR_MIN				1

// Timer to check for, and possibly start rescheduled jobs.
#define STORK_RESCHEDULED_JOB_MONITOR			"STORK_RESCHEDULED_JOB_MONITOR"
#define STORK_RESCHEDULED_JOB_MONITOR_DEFAULT	10
#define STORK_RESCHEDULED_JOB_MONITOR_MIN		1

// Timer to check for, and possibly kill hung jobs
#define STORK_HUNG_JOB_MONITOR					"STORK_HUNG_JOB_MONITOR"
#define STORK_HUNG_JOB_MONITOR_DEFAULT			300
#define STORK_HUNG_JOB_MONITOR_MIN				1

typedef enum {
	TERMINATE_GRACEFUL,
	TERMINATE_FAST
} terminate_t;

int initializations();
int terminate(terminate_t);
int read_config_file();
int call_main();
void startup_check_for_requests_in_process();
void regular_check_for_requests_in_process();
void regular_check_for_rescheduled_requests();

int handle_stork_submit(Service *, int command, Stream *s);
int handle_stork_remove(Service *, int command, Stream *s);
int handle_stork_status(Service *, int command, Stream *s);
int handle_stork_list(Service *, int command, Stream *s);

int transfer_dap_reaper(Service *,int pid,int exit_status);
int reserve_dap_reaper(Service *,int pid,int exit_status);
int release_dap_reaper(Service *,int pid,int exit_status);
int requestpath_dap_reaper(Service *,int pid,int exit_status);

int write_requests_to_file(ReliSock * sock);
int remove_requests_from_queue (ReliSock * sock);
int send_dap_status_to_client (ReliSock * sock);
int list_queue (ReliSock * sock);

void remove_credential (char * dap_id);
char * get_credential_filename (char * dap_id);
int get_cred_from_credd (const char * request, void *& buff, int & size);

int init_user_id_from_FQN (const char *owner);
void clean_job_queue(void);

#endif

