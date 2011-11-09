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

#define _FILE_OFFSET_BITS 64

#include <string>
#include <map>

enum DebugOutput
{
	FILE_OUT,
	STD_OUT,
	STD_ERR,
	OUTPUT_DEBUG_STR
};

struct DebugFileInfo
{
	DebugOutput outputTarget;
	FILE *debugFP;
	int debugFlags;
	std::string logPath;
	off_t maxLog;
	int maxLogNum;
	bool want_truncate;
	bool accepts_all;

	DebugFileInfo() : debugFlags(0), debugFP(0), maxLog(0), maxLogNum(0), outputTarget(DebugOutput::FILE_OUT), want_truncate(0), accepts_all(false) {}
	DebugFileInfo(const DebugFileInfo &debugFileInfo);
	~DebugFileInfo();
};

struct param_info
{
	int debugFlags;
	std::string logPath;
	off_t maxLog;
	int maxLogNum;
	bool want_truncate;
	bool accepts_all;

	param_info() : debugFlags(0), max_log(0), maxLogNum(0), want_truncate(0), accepts_all(false) {}
};