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


#include "condor_common.h"
#include "openssl/evp.h"
#include "cr_hash.h"
#define TEMP_FILE_NAME "/tmp/condor_test_cr_hash.txt"
#define TEST_STRING_1 "Condor is da bomb.\n"
#define TEST_RESULT_1 "f7791e0dbd1ec873a3b59be238839790a3b614de"

int main(int argc, char **argv)
{
	OpenSSL_add_all_digests();
	int fd = safe_open_wrapper(TEMP_FILE_NAME, O_CREAT|O_TRUNC|O_WRONLY, 
							   S_IRWXU);
	if(fd == -1) {
		fprintf(stderr, "error creating test file '" TEMP_FILE_NAME "'.\n");
		exit(1);
	}
	unsigned int rv = write(fd, TEST_STRING_1, strlen(TEST_STRING_1));
	if(rv != strlen(TEST_STRING_1)) {
		fprintf(stderr, "error writing to test file.\n");
		exit(1);
	}
	close(fd);

	char *mysum = get_hash_of_file(TEMP_FILE_NAME, "sha1");
	if(mysum == NULL) {
		fprintf(stderr, "Got error calculating hash.\n");
		exit(1);
	}
	//fprintf(stderr, "%s\n",mysum);
	if(strncmp(mysum,TEST_RESULT_1,strlen(TEST_RESULT_1))) {
		fprintf(stderr, "Whoops!  Test result different from expected.\n");
		fprintf(stderr, "Expected: '" TEST_RESULT_1 "'.\n");
		fprintf(stderr, "Actual: '%s'.\n", mysum);
		free(mysum);
		exit(1);
	} else {
		fprintf(stderr, "Success!\n");
		free(mysum);
	}		
	
	if(unlink(TEMP_FILE_NAME)) {
		perror("Can't delete file '" TEMP_FILE_NAME "'.\n");
		exit(1);
	}
	exit(0);
}
