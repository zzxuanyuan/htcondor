/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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

#ifndef __LOCK_FILE__
#define __LOCK_FILE__

#include "condor_common.h"

#define MAX_RETRY_ATT 10

enum LOCKING_TYPE {L_LINK_R, L_LINK_W, L_LOCK_R, L_LOCK_W};

class LockFile {

	public: 
		LockFile();
		LockFile(char *base_dir, char *file_name);
		~LockFile();
		int init(char *base_dir, char *file_name);
		int obtain(LOCKING_TYPE lt = L_LOCK_W);
		int release();
		bool isActive();
		
	private : 
		char *_base_dir; // currently the same for file and lock
		char *_file_name;
		char *_link_name;
		char *_link_name_r;
		bool is_active;
		LOCKING_TYPE _tp;
		int fd;
		void create_hash();
#ifdef WIN32
		wchar_t* convertFromChar(char *);
#endif
		
};

#endif
