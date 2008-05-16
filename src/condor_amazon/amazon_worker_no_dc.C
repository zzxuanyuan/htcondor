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
#include "condor_debug.h"
#include "MyString.h"
#include "PipeBuffer.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"

// Buffer for reading requests from the IO thread
PipeBuffer request_buffer;

int RESULT_OUTBOX = 1; // stdout
int ERROR_OUTBOX = 2; // stderr
int REQUEST_INBOX = 0; // stdin

void
usage()
{
	fprintf( stderr, "Usage: amazon-gahp_worker_thread\n");
	exit( 1 );
}

static void
enqueue_result(const char *buffer, int length)
{
	if(!buffer || !length ) {
		return;
	}

	write(RESULT_OUTBOX, buffer, length);
}

static int
handle_gahp_command(char ** argv, int argc) 
{
	// Assume it's been verified
	if( argc < 2 ) {
		dprintf (D_ALWAYS, "Invalid request\n");
		return FALSE;
	}

	if( !verify_request_id(argv[1]) ) {
		dprintf (D_ALWAYS, "Invalid request ID\n");
		return FALSE;
	}

	MyString output_string;
	bool result = executeWorkerFunc(argv[0], argv, argc, output_string);

	if( output_string.IsEmpty() == false ) {
		enqueue_result(output_string.Value(), output_string.Length());
	}

	return result;
}

static int
waitForCommand(void)
{
	MyString* next_line = NULL;
	while ((next_line = request_buffer.GetNextLine()) != NULL) {

		dprintf (D_FULLDEBUG, "got work request: %s\n", next_line->Value());

		Gahp_Args args;

		// Parse the command...
		if (!( parse_gahp_command (next_line->Value(), &args) &&
					handle_gahp_command (args.argv, args.argc) )) {
			dprintf (D_ALWAYS, "ERROR processing %s\n", next_line->Value());
		}

		// Clean up...
		delete  next_line;
	}

	// check for an error in GetNextLine
	if (request_buffer.IsError() || request_buffer.IsEOF()) {
		dprintf (D_ALWAYS, "Request pipe closed. Exiting...\n");
		exit (1);
	}

	return TRUE;
}

static bool
registerAllAmazonCommands(void)
{
	if( numofAmazonCommands() > 0 ) {
		dprintf(D_ALWAYS, "There are already registered commands\n");
		return false;
	}

	// EC2 Commands

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_START, 
			AmazonVMStart::ioCheck, AmazonVMStart::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_STOP, 
			AmazonVMStop::ioCheck, AmazonVMStop::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_REBOOT, 
			AmazonVMReboot::ioCheck, AmazonVMReboot::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_STATUS, 
			AmazonVMStatus::ioCheck, AmazonVMStatus::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_STATUS_ALL, 
			AmazonVMStatusAll::ioCheck, AmazonVMStatusAll::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_RUNNING_KEYPAIR, 
			AmazonVMRunningKeypair::ioCheck, AmazonVMRunningKeypair::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_CREATE_GROUP, 
			AmazonVMCreateGroup::ioCheck, AmazonVMCreateGroup::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_DELETE_GROUP, 
			AmazonVMDeleteGroup::ioCheck, AmazonVMDeleteGroup::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_GROUP_NAMES, 
			AmazonVMGroupNames::ioCheck, AmazonVMGroupNames::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_GROUP_RULES, 
			AmazonVMGroupRules::ioCheck, AmazonVMGroupRules::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_ADD_GROUP_RULE, 
			AmazonVMAddGroupRule::ioCheck, AmazonVMAddGroupRule::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_DEL_GROUP_RULE, 
			AmazonVMDelGroupRule::ioCheck, AmazonVMDelGroupRule::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_CREATE_KEYPAIR, 
			AmazonVMCreateKeypair::ioCheck, AmazonVMCreateKeypair::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_DESTROY_KEYPAIR, 
			AmazonVMDestroyKeypair::ioCheck, AmazonVMDestroyKeypair::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_KEYPAIR_NAMES, 
			AmazonVMKeypairNames::ioCheck, AmazonVMKeypairNames::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_REGISTER_IMAGE, 
			AmazonVMRegisterImage::ioCheck, AmazonVMRegisterImage::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_VM_DEREGISTER_IMAGE, 
			AmazonVMDeregisterImage::ioCheck, AmazonVMDeregisterImage::workerFunction);


	// S3 Commands
	registerAmazonGahpCommand(AMAZON_COMMAND_S3_ALL_BUCKETS,
			AmazonS3AllBuckets::ioCheck, AmazonS3AllBuckets::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_S3_CREATE_BUCKET,
			AmazonS3CreateBucket::ioCheck, AmazonS3CreateBucket::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_S3_DELETE_BUCKET,
			AmazonS3DeleteBucket::ioCheck, AmazonS3DeleteBucket::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_S3_LIST_BUCKET,
			AmazonS3ListBucket::ioCheck, AmazonS3ListBucket::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_S3_UPLOAD_FILE,
			AmazonS3UploadFile::ioCheck, AmazonS3UploadFile::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_S3_UPLOAD_DIR,
			AmazonS3UploadDir::ioCheck, AmazonS3UploadDir::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_S3_DELETE_FILE,
			AmazonS3DeleteFile::ioCheck, AmazonS3DeleteFile::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_S3_DOWNLOAD_FILE,
			AmazonS3DownloadFile::ioCheck, AmazonS3DownloadFile::workerFunction);

	registerAmazonGahpCommand(AMAZON_COMMAND_S3_DOWNLOAD_BUCKET,
			AmazonS3DownloadBucket::ioCheck, AmazonS3DownloadBucket::workerFunction);

	return true;
}

int
main( int argc, char ** const argv )
{
	// All log should be printed to stderr
	set_gahp_log_file(NULL);	

	// get env
	MyString debug_string = getenv("DebugLevel");
	if( debug_string.IsEmpty() == false ) {
		set_debug_flags( (char* )debug_string.GetCStr());
	}

#define AMAZON_HTTP_PROXY	"AMAZON_HTTP_PROXY"
	// Try to get proxy server information
	MyString amazon_proxy_server = getenv(AMAZON_HTTP_PROXY);

	if( amazon_proxy_server.IsEmpty() == false ) {
		// Set http_proxy environment variable which will be used for perl program	
		SetEnv("HTTP_PROXY", amazon_proxy_server.Value());
	}

	// For Testing for exec perl
	//set_amazon_lib_path("/scratch/amazonCompile/src/condor_amazon/ec2_lib");
	
	dprintf(D_FULLDEBUG, "Welcome to the AMAZON-GAHP\n");

	// Save current working_dir;
	char tmpCwd[_POSIX_PATH_MAX];
	if( !getcwd(tmpCwd, _POSIX_PATH_MAX) ) {
		dprintf(D_ALWAYS, "Failed to getcwd\n");
		exit(1);
	}
	set_working_dir(tmpCwd);

	// Register all amazon commands
	if( registerAllAmazonCommands() == false ) {
		dprintf(D_ALWAYS, "Can't register Amazon Commands\n");
		exit( 1 );
	}

	request_buffer.setPipeEnd( REQUEST_INBOX );

	for(;;) {
		waitForCommand();
	}

	return 0;
}
