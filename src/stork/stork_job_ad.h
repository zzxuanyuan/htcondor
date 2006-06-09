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


// Stork job interfaces

#ifndef _STORK_JOB_AD_H_
#define _STORK_JOB_AD_H_

#include "condor_common.h"

// Stork Job ClassAd attributes.

// The following job attributes are set by the user:

// job type
#define STORK_JOB_ATTR_TYPE					"type"
#define STORK_JOB_TYPE_TRANSFER				"transfer"

// Module arguments
#define STORK_JOB_ATTR_ARGUMENTS			"arguments"

// Job environment
#define STORK_JOB_ATTR_ENVIRONMENT			"environment"

// job standard I/O
#define STORK_JOB_ATTR_INPUT				"input"
#define STORK_JOB_ATTR_OUTPUT				"output"
#define STORK_JOB_ATTR_ERROR				"err"

// user log file
#define STORK_JOB_ATTR_LOG					"log"

// user log XML format
#define STORK_JOB_ATTR_LOGXML				"log_xml"

// user log notes
#define STORK_JOB_ATTR_LOGNOTES				"LogNotes"

// transfer attributes
#define STORK_JOB_ATTR_SRC_URL				"src_url"
#define STORK_JOB_ATTR_DEST_URL				"dest_url"


// The following job attributes are set by the user, and possibly rewritten by
// the Stork server:

// X.509 proxy location
#define STORK_JOB_ATTR_X509PROXY			"x509proxy"
// Instruct Stork to look for the proxy in the default locations:
#define STORK_JOB_X509PROXY_DEFAULT			"default"

// Module path
#define STORK_JOB_ATTR_MODULE				"module"

// The following job attributes are written by the Stork server:

// id contains numeric job id
#define STORK_JOB_ATTR_ID					"id"

// job user@domain
#define STORK_JOB_ATTR_USER					"remote_user"

// job owner, for privilege control
#define STORK_JOB_ATTR_OWNER				"owner"

// NT Domain
#define STORK_JOB_ATTR_DOMAIN				"domain"

// timestamp contains last ad update timestamp
#define STORK_JOB_ATTR_SUBMIT_TIME			"submit_time"

// submit host:port sinful string
#define STORK_JOB_ATTR_SUBMIT_HOST			"submit_host"

// execute host:port sinful string
#define STORK_JOB_ATTR_EXECUTE_HOST			"execute_host"

// last job start time
#define STORK_JOB_ATTR_START_TIME			"start_time"

// status is a string specifying job state
#define STORK_JOB_ATTR_STATUS				"status"
#define STORK_JOB_STATUS_IDLE				"idle"
#define STORK_JOB_STATUS_RUN				"run"
#define STORK_JOB_STATUS_COMPLETE			"completed"
#define STORK_JOB_STATUS_REMOVE				"removed"
#define STORK_JOB_STATUS_FAIL				"failed"
#define STORK_JOB_STATUS_MAXJOBID			"maxJobId"

// Job run attempt count
#define STORK_JOB_ATTR_NUM_ATTEMPTS			"num_attempts"

// error code is a string with the last error code.  This attribute is
// optional.
//#define STORK_JOB_ATTR_ERROR_STATUS			"error_status"

// Starting transfer protocol index
//#define STORK_JOB_ATTR_USE_PROTOCOL			"use_protocol"

// Job submit timestamp
#define STORK_JOB_ATTR_SUBMIT_TIME			"submit_time"

// Job run directory
#define STORK_JOB_ATTR_RUN_DIR				"run_dir"

// Module exec arguments, created from user supplied STORK_JOB_ATTR_ARGUMENTS,
// and passed to module when exec'ed.
#define STORK_JOB_ATTR_EXEC_ARGS			"exec_args"

// Module exec environment, created from user supplied
// STORK_JOB_ATTR_ENVIRONMENT, and passed to module when exec'ed.
#define STORK_JOB_ATTR_EXEC_ENV				"exec_env"

// Last exit status
#define STORK_JOB_ATTR_EXIT_STATUS			"exit_status"

#endif /* _STORK_JOB_AD_H_ */

