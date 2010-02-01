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

#include "condor_common.h"
#include <iostream>
#ifdef WIN32
#include "windows.h"
#else
#include <fcntl.h>
#endif
#include "LockFile.h"

using namespace std;

LockFile::LockFile(){
	is_active = false;
	_base_dir = "";  
	_file_name = "";
	_link_name = NULL;
	_link_name_r = NULL;
}

LockFile::LockFile(char *base_dir, char *file_name){
	is_active = false;
	_file_name = file_name;
	_base_dir = base_dir;
	create_hash();
}

LockFile::~LockFile() {
	_base_dir = NULL;
	_file_name = NULL;
	is_active = false;
	if (_link_name != NULL)
		delete []_link_name;
	_link_name = NULL;
	if (_link_name_r != NULL)
		delete []_link_name_r;
	_link_name_r = NULL;
}


void LockFile::create_hash(){
	int orig_size = strlen(_file_name);

	_link_name = new char[512];
	_link_name_r = new char[512];
	unsigned long hash = 0;
	int c;
	for (int i = 0 ; i < orig_size; i++){
		c = _file_name[i];
		hash = c + (hash << 6) + (hash << 16) - hash;
	}
	sprintf(_link_name, "%s%u%s", _base_dir , hash, ".lock");
	sprintf(_link_name_r, "%s%u%s", _base_dir,  hash, ".rlock");
	

}

int LockFile::init(char *base_dir, char *file_name){
	_base_dir = base_dir;
	_file_name = file_name;
	create_hash();
	return 1;
}

#ifdef WIN32  // special conversion function to be able to use WIN32 data types 
wchar_t* LockFile::convertFromChar(char *orig){
	size_t origsize = strlen(orig) + 1;
   	const size_t newsize = origsize;
    size_t convertedChars = 0;
    wchar_t *wcstring = new wchar_t[newsize];
	mbstowcs_s(&convertedChars, wcstring, origsize, orig, _TRUNCATE);
	return wcstring;
}
#endif

/**
Idea for read lock: if read directory already exists, 
add a file (randomly named). Upon read completion, 
delete file and attempt to delete directory. 
RemoveDirectory and rmdir should only succeed if empty. 
So parent process just determines that directory cannot be 
deleted and will quit happily. 

Currently: only exclusive locks are issued.
*/

int LockFile::obtain(LOCKING_TYPE lt ){
	_tp = lt;
	if (is_active){
		return 0;
	}
	int tried = 0;
attempt:
		#ifdef WIN32
		wchar_t *f_name = convertFromChar(_file_name);
		wchar_t *l_name = convertFromChar(_link_name);
		
		#endif
	
	
	if (_tp < L_LOCK_R) {
		#ifdef WIN32
			// 1: successful, 0: failed
		fd = CreateHardLink(LPCWSTR(l_name), LPCWSTR(f_name), NULL);
		
		--fd; 
		#else 
		fd = link(_file_name, _link_name);
		#endif

	} else { //create lock directory

	#ifdef WIN32
	fd = CreateDirectory(LPCWSTR(l_name), NULL);
	--fd;		
	#else
	fd = mkdir(_link_name, S_IRWXU|S_IRGRP|S_IXGRP); 
	#endif		
	}
	#ifdef WIN32
	delete []f_name;
	delete []l_name;
	#endif
	
	if ( fd  < 0 ){
		
		++tried;
			// lock cannot be obtained .
		if (tried > MAX_RETRY_ATT){
			return -1;
		}
		#ifdef WIN32
		Sleep(2000);
		#else
		sleep(2);
		#endif
		goto attempt; // rather than retry in time cycles, have some kind of FIFO?
	} else {
		is_active = true;
		return 1;
	}
}

int LockFile::release(){
	if (is_active){
		#ifdef WIN32
		wchar_t *l_name = convertFromChar(_link_name);
		#endif
		if (_tp < L_LOCK_R) {
			#ifdef WIN32
			fd = DeleteFile(LPCWSTR(l_name));
			--fd;
			#else 
			fd = unlink(_link_name);	
			#endif			
		} else {
			#ifdef WIN32
			fd = RemoveDirectory(LPCWSTR(l_name));
			#else
			fd = rmdir(_link_name);
			#endif
		}
		#ifdef WIN32
		delete []l_name;
		#endif
		
		if (fd < 0){
			perror("lock release");
		}
		is_active = false;
		return 1;
	} else {
		return -1;
	}
}

bool LockFile::isActive(){
	return is_active;
}







