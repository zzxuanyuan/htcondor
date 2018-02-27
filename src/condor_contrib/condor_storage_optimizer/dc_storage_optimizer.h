/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_DC_STORAGE_OPTIMIZER_H
#define _CONDOR_DC_STORAGE_OPTIMIZER_H

#include "CondorError.h"
#include "daemon.h"

class DCStorageOptimizer : public Daemon
{
public:

	DCStorageOptimizer(const char * name = NULL, const char *pool = NULL);
	DCStorageOptimizer(const ClassAd* ad, const char* pool);

	~DCStorageOptimizer() {}

	int pingStorageOptimizer(std::string &storageOptimizer);
};

#endif // _CONDOR_DC_STORAGE_OPTIMIZER_H
