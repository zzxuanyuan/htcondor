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

#ifndef __STORAGE_OPTIMIZER_SERVER_H__
#define __STORAGE_OPTIMIZER_SERVER_H__

#include "classad/classad_stl.h"
#include "classad_log.h"

class StorageOptimizerServer: Service {
public:
	StorageOptimizerServer();
	~StorageOptimizerServer();

	void InitAndReconfig();
	void SetAttributeString(const MyString& Key, const MyString& AttrName, const MyString& AttrValue);
	bool GetAttributeString(const MyString& Key, const MyString& AttrName, MyString& AttrValue);
	void InitializeDB();
	void TestIterateDB();
	void TestReadDB();
	void TestWriteDB();

private:
	int m_test_iterate_db_timer;
	int m_test_read_db_timer;
	int m_test_write_db_timer;
	int m_dummy_reaper;
	std::string m_db_fname;
	ClassAdLog<std::string, ClassAd*> *m_log;
};

#endif
