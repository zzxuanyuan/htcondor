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

#ifdef WIN32
#include "windows.h"
#endif
#include<iostream>
#include "LockFile.h"
#include "assert.h"
using namespace std;


void test_obtain(LockFile *lock, bool shallWork) {
	printf("[Obtain test] -- started \n");
	int result;
	
	result = lock->obtain();
	if (shallWork)
		assert(result == 1);
	else {
		assert(result < 0);
		printf("[Obtain test] -- passed \n");
		return;
	}
	result = lock->obtain();
	assert(result == 0);
	printf("[Obtain test] -- passed \n");
} 

void test_release(LockFile *lock) {
	printf("[Release test] -- started \n");
	int result;
	result = lock->obtain();
	if (result != 1) {
		int rresult = lock->release();
		if (result == 0) 
			assert(rresult == 1);
		else
			assert(rresult == -1);
	} else {
		result = lock->release();
		assert(result == 1);
	} 
	printf("[Release test] -- passed \n");
}

// currently : no threading/forking ; to be changed 
int main() {
#ifdef WIN32
	cout << "Win32" << endl;
#endif
	LockFile lock("", "test.txt");
	LockFile lock2("", "test.txt");
	
	test_obtain(&lock, true);
	test_obtain(&lock2, false);
	
	test_release(&lock);
	test_release(&lock2);

	test_obtain(&lock2, true);
	test_release(&lock2);
	
	return 0;

} 
