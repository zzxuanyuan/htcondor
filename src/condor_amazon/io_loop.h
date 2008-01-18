/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef IO_LOOP_H
#define IO_LOOP_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h" // For Stream decl
#include "gahp_common.h"
#include "PipeBuffer.h"

#define GAHP_COMMAND_ASYNC_MODE_ON "ASYNC_MODE_ON"
#define GAHP_COMMAND_ASYNC_MODE_OFF "ASYNC_MODE_OFF"
#define GAHP_COMMAND_RESULTS "RESULTS"
#define GAHP_COMMAND_QUIT "QUIT"
#define GAHP_COMMAND_VERSION "VERSION"
#define GAHP_COMMAND_COMMANDS "COMMANDS"

#define GAHP_RESULT_SUCCESS "S"
#define GAHP_RESULT_ERROR "E"
#define GAHP_RESULT_FAILURE "F"

class Worker : public Service {
 public:
	void Init (int _id) { id = _id; }

	int result_handler (int);
				   
	PipeBuffer request_buffer;
	PipeBuffer result_buffer;

	int result_pipe[2];
	int request_pipe[2];

    int flush_request_tid;
	int pid;

	int id;
};

#endif
