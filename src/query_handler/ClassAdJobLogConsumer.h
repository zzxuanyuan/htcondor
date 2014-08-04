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


#include "ClassAdLogReader.h"

#include "classad_collection.h"

class ClassAdJobLogConsumer: public ClassAdLogConsumer
{
private:

	ClassAdCollection m_collection;
	ClassAdLogReader *m_reader;

public:

	ClassAdJobLogConsumer();

	ClassAdCollection *GetClassAds() {return &m_collection;}

	void Reset();

	bool NewClassAd(const char *key,
					const char * type,
					const char * target);

	bool DestroyClassAd(const char *key);

	bool SetAttribute(const char *key,
					  const char *name,
					  const char *value);

	bool DeleteAttribute(const char *key,
						 const char *name);

	void SetJobLogReader(ClassAdLogReader *_reader) { m_reader = _reader; }
};

