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

#ifndef DAG_COMMAND_NAMES_H
#define DAG_COMMAND_NAMES_H

#define DAG_CMD_JOB			"JOB"
#define DAG_CMD_DAP			"DAP"
#define DAG_CMD_DATA		"DATA"
#define DAG_CMD_SUBDAG		"SUBDAG"
#define DAG_CMD_FINAL		"FINAL"
#define DAG_CMD_SCRIPT		"SCRIPT"
#define DAG_CMD_PARENT		"PARENT"
#define DAG_CMD_RETRY		"RETRY"
#define DAG_CMD_ABORTDAGON	"ABORT-DAG-ON"
#define DAG_CMD_DOT			"DOT"
#define DAG_CMD_VARS		"VARS"
#define DAG_CMD_PRIORITY	"PRIORITY"
#define DAG_CMD_CATEGORY	"CATEGORY"
#define DAG_CMD_MAXJOBS		"MAXJOBS"
#define DAG_CMD_CONFIG		"CONFIG"
#define DAG_CMD_DAGSUBCMD	"DAG_SUBMIT_COMMAND"
#define DAG_CMD_SPLICE		"SPLICE"
#define DAG_CMD_NODESTATUSFILE	"NODE_STATUS_FILE"
#define DAG_CMD_REJECT		"REJECT"
#define DAG_CMD_JOBSTATELOG	"JOBSTATE_LOG"
#define DAG_CMD_PRESKIP		"PRE_SKIP"
#define DAG_CMD_DONE		"DONE"

//TEMPTEMP -- what about "CHILD"?, "EXTERNAL", "DIR"?
//TEMPTEMP -- maybe rename this dag_strings.h?

#endif // DAG_COMMAND_NAMES_H

