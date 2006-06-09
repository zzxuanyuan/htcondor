/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

// Stork config file entries

#ifndef _STORK_CONFIG_H_
#define _STORK_CONFIG_H_

#include "condor_common.h"	// for NULL
#include "condor_config.h"
#include "stork_job_ad.h"

// Stork requires the SPOOL directory.
#define SPOOL						"SPOOL"

// Stork requires an EXECUTE sandbox directory.
#define EXECUTE						"STORK_EXECUTE"

// Stork requires a STORK_MODULE_DIR or LIBEXEC directory.
#define STORK_MODULE_DIR			"STORK_MODULE_DIR"
#define LIBEXEC						"LIBEXEC"

// Stork module cache entry time to live, seconds.
// Default value = 1 hr.
#define STORK_MODULE_CACHE_TTL		"STORK_MODULE_CACHE_TTL"
#define STORK_MODULE_CACHE_TTL_DEFAULT	(60*60)
#define STORK_MODULE_CACHE_TTL_MIN	1

// Limits the number of concurrent data placements handled by Stork. Default
// value = 10.
#define STORK_MAX_NUM_JOBS			"STORK_MAX_NUM_JOBS"
#define STORK_MAX_NUM_JOBS_DEFAULT	10
#define STORK_MAX_NUM_JOBS_MIN		0

// Stork maintains the job queue on a given machine. It does so in a persistent
// way such that if Stork crashes, it can recover a valid state of the job
// queue. The mechanism it uses is a transaction-based log file (the
// $(SPOOL)/stork_job_queue.log file, not the SchedLog file). This file
// contains an initial state of the job queue, and a series of transactions
// that were performed on the queue (such as new jobs submitted, jobs
// completing, and checkpointing).  This macro is only read once, when Stork is
// started.  By default, this macro is the boolean value of true, enabling the 
// persistent job queue.
#define STORK_JOB_QUEUE						"STORK_JOB_QUEUE"
#define STORK_JOB_QUEUE_DEFAULT				true

// If STORK_JOB_QUEUE is defined, Stork will periodically go through the
// STORK_JOB_QUEUE, truncate all the transactions and create a new file with
// containing only the new initial state of the log. This is a somewhat
// expensive operation, but it speeds up Stork restarts, since there are fewer
// transactions it has to play to figure out what state the job queue is really
// in. This macro determines how often Stork should rework this queue to
// cleaning it up. It is defined in terms of seconds and defaults to 86400
// (once a day).
#define STORK_QUEUE_CLEAN_INTERVAL			"STORK_QUEUE_CLEAN_INTERVAL"
#define STORK_QUEUE_CLEAN_INTERVAL_DEFAULT	86400
#define STORK_QUEUE_CLEAN_INTERVAL_MIN		1

// Stork maintains the history of completed jobs on a given machine.  This
// macro is only read once, when Stork is started.  By default, this macro is
// the boolean value of true, enabling recording of completed job history.
#define STORK_JOB_HISTORY					"STORK_JOB_HISTORY"
#define STORK_JOB_HISTORY_DEFAULT			true

// Controls the maximum length in bytes to which the history file will be
// allowed to grow. The history file will grow to the specified length, then be
// saved to a file with the suffix .old. The .old file is overwritten each time
// the history file si saved, thus the maximum space devoted will be twice the
// maximum length of the history file. A value of 0 specifies that the file may
// grow without bounds. The default is 1 Gbyte.
#define MAX_STORK_HISTORY					"MAX_STORK_HISTORY"
#define MAX_STORK_HISTORY_DEFAULT			1073741824
#define MAX_STORK_HISTORY_MIN				0

// Define the period for rescheduling idle Stork jobs, seconds.  The default
// value is 5 seconds.
#define STORK_RESCHEDULE_INTERVAL			"STORK_RESCHEDULE_INTERVAL"
#define STORK_RESCHEDULE_INTERVAL_DEFAULT	5
#define STORK_RESCHEDULE_INTERVAL_MIN		2

// Define the minimum period for rescheduling idle Stork jobs, seconds.  In
// addition to the STORK_RESCHEDULE_INTERVAL periodic timer, various
// asynchronous events can also rescheduling jobs.  This parameter prevents
// rescheduling events from overwhelming the Stork server.  In the event this
// parameter is set greater than STORK_RESCHEDULE_INTERVAL, a value of half
// STORK_RESCHEDULE_INTERVAL is used.  The default value is 2 seconds.
#define STORK_MIN_RESCHEDULE_INTERVAL			"STORK_MIN_RESCHEDULE_INTERVAL"
#define STORK_MIN_RESCHEDULE_INTERVAL_DEFAULT	2
#define STORK_MIN_RESCHEDULE_INTERVAL_MIN		1

// Limit the number of jobs Stork starts each rescheduling interval.
// Small values for this macro can be used to distribute the load of large job
// submissions to the Stork server.
#define STORK_JOB_START_COUNT			"STORK_JOB_START_COUNT"
#define STORK_JOB_START_COUNT_DEFAULT	10
#define STORK_JOB_START_COUNT_MIN		0

// Define the requirements. for rescheduling idle jobs.  Internally, Stork
// always appends the additional requirement that jobs must be idle to be
// rescheduled.  Regardless, Stork will never run more than STORK_MAX_NUM_JOBS.
// This is done at the system level, across all users.
// Requirements is an expression, with a default value of "true".
#define STORK_RESCHEDULE_REQUIREMENTS			"STORK_RESCHEDULE_REQUIREMENTS"
#define STORK_RESCHEDULE_REQUIREMENTS_DEFAULT 	"true"

// A ClassAd -Point expression that states how to rank (sort) jobs which have
// already met the requirements expression. Essentially, rank expresses
// preference.  This is done at the system level, across all users.
// The default value for this exression prefers to run.
// oldest jobs.
#define STORK_RESCHEDULE_RANK			"STORK_RESCHEDULE_RANK"
#define STORK_RESCHEDULE_RANK_DEFAULT	"other." STORK_JOB_ATTR_SUBMIT_TIME

// Enable access checks of all files associated with a job, at submission time,
// e.g. user log file, output file, input file, etc.  Default is true.  Set
// this value to false to increase job submit performance, at your peril.
#define STORK_CHECK_ACCESS_FILES			"STORK_CHECK_ACCESS_FILES"
#define STORK_CHECK_ACCESS_FILES_DEFAULT	true

// Enable access checks of any X.509 proxy associated with job, at submit time.
// Default is true.  Set this value to false to increase job submit
// performance, at your peril.
#define STORK_CHECK_PROXY				"STORK_CHECK_PROXY"
#define STORK_CHECK_PROXY_DEFAULT		true

#if 0
// Limits the number of concurrent data placements handled by Stork.
#define STORK_MAX_NUM_JOBS					"STORK_MAX_NUM_JOBS"
#define STORK_MAX_NUM_JOBS_DEFAULT			10
#define STORK_MAX_NUM_JOBS_MIN				1

// Limits the number attempts for a data placement. For data transfers, this
// includes transfer attempts on the primary protocol and all alternate
// protocols, and all retries.
#define STORK_MAX_RETRY						"STORK_MAX_RETRY"
#define STORK_MAX_RETRY_DEFAULT				10
#define STORK_MAX_RETRY_MIN					1

// Limits the run time for a data placement job, after which the placement is
// considered failed. Units = minutes.
// Deprecated, use STORK_MAX_JOB_DURATION instead.
#define STORK_MAXDELAY_INMINUTES			"STORK_MAXDELAY_INMINUTES"
#define STORK_MAXDELAY_INMINUTES_DEFAULT	10
#define STORK_MAXDELAY_INMINUTES_MIN		1

// Limits the run time for a data placement job, after which the placement is
// considered failed.  This parameter defines the "hung job" timeout.
// Units = seconds.
#define STORK_MAX_JOB_DURATION				"STORK_MAX_JOB_DURATION"
#define STORK_MAX_JOB_DURATION_DEFAULT	(60 * STORK_MAXDELAY_INMINUTES_DEFAULT)
#define STORK_MAX_JOB_DURATION_MIN			1

// Interval for which "hung job" detection is performed.
// Units = seconds.
#define STORK_HUNG_JOB_MONITOR				"STORK_HUNG_JOB_MONITOR"
#define STORK_HUNG_JOB_MONITOR_DEFAULT		(5 * 60)
#define STORK_HUNG_JOB_MONITOR_MIN			1

// Interval for which rescheduled jobs are evaluated for execution.
// Units = seconds.
#define STORK_RESCHEDULED_JOB_MONITOR		"STORK_RESCHEDULED_JOB_MONITOR"
#define STORK_RESCHEDULED_JOB_MONITOR_DEFAULT	5
#define STORK_RESCHEDULED_JOB_MONITOR_MIN		1

// Temporary credential storage directory used by Stork.
#define STORK_TMP_CRED_DIR					"STORK_TMP_CRED_DIR"
#define STORK_TMP_CRED_DIR_DEFAULT			"/tmp"

// Directory containing Stork modules. If not defined, the value for the
// LIBEXEC  macro is used. It is a fatal error for both of these macros to be
// undefined.
#define STORK_MODULE_DIR					"STORK_MODULE_DIR"
#define LIBEXEC								"LIBEXEC"

// Logging directory.
#define	LOG									"LOG"
#define	LOG_DEFAULT							"/tmp"

// Stork saves data placement job standard output and standard error for
// logging in the STORK_STDIO_DIR.  Job standard I/O files are temporary, and
// are removed after job logging is complete.
#define STORK_STDIO_DIR						"STORK_STDIO_DIR"
#define STORK_STDIO_DIR_DEFAULT				"/tmp/stork-stdio"

// Job stdout and stderr outputs are truncated to STORK_MAX_STDIO_LOG
// characters prior to logging.
#define STORK_MAX_STDIO_LOG					"STORK_MAX_STDIO_LOG"
#define STORK_MAX_STDIO_LOG_DEFAULT			32
#endif

#endif /* _STORK_CONFIG_H_ */

