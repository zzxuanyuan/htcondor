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
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "string_list.h"
#include "MyString.h"
#include "PipeBuffer.h"
#include "io_loop.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"

#define AMAZON_GAHP_VERSION	"0.0.1"

const char * version = "$GahpVersion " AMAZON_GAHP_VERSION " Aug 28 2008 Condor\\ AMAZONGAHP $";

char *mySubSystem = "AMAZON_GAHP";	// used by Daemon Core

int async_mode = 0;
int new_results_signaled = 0;

int flush_request_tid = -1;

// The list of results ready to be output to IO
StringList result_list;

// pipe buffers
PipeBuffer stdin_buffer; 

#define NUMBER_WORKERS 1
Worker workers[NUMBER_WORKERS];

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

// forwarding declaration
static int stdin_pipe_handler(Service*, int);
static int result_pipe_handler(int id);
static void gahp_output_return_error();
static void gahp_output_return_success();
static void gahp_output_return (const char ** results, const int count);
static int verify_gahp_command(char ** argv, int argc);
static void flush_request (int worker_id, const char * request);
static int flush_pending_requests();
static int worker_thread_reaper (Service*, int pid, int exit_status);


#ifdef WIN32
int STDIN_FILENO = fileno(stdin);
#endif


#if defined(WIN32)
int forwarding_pipe = -1;
unsigned __stdcall pipe_forward_thread(void *)
{
	const int FORWARD_BUFFER_SIZE = 4096;
	char buf[FORWARD_BUFFER_SIZE];
	
	// just copy everything from stdin to the forwarding pipe
	while (true) {

		// read from stdin
		int bytes = read(0, buf, FORWARD_BUFFER_SIZE);
		if (bytes == -1) {
			dprintf(D_ALWAYS, "pipe_forward_thread: error reading from stdin\n");
			daemonCore->Close_Pipe(forwarding_pipe);
			return 1;
		}

		// close forwarding pipe and exit on EOF
		if (bytes == 0) {
			daemonCore->Close_Pipe(forwarding_pipe);
			return 0;
		}

		// write to the forwarding pipe
		char* ptr = buf;
		while (bytes) {
			int bytes_written = daemonCore->Write_Pipe(forwarding_pipe, ptr, bytes);
			if (bytes_written == -1) {
				dprintf(D_ALWAYS, "pipe_forward_thread: error writing to forwarding pipe\n");
				daemonCore->Close_Pipe(forwarding_pipe);
				return 1;
			}
			ptr += bytes_written;
			bytes -= bytes_written;
		}
	}

}
#endif

void Init() {}

void Register() {}

void Reconfig() {
	MyString tmp_string;
	if( find_amazon_lib(tmp_string) == false ) {
		DC_Exit( 1 );
	}
}


int
main_config( bool )
{
	Reconfig();
	return TRUE;
}

int
main_shutdown_fast()
{

	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
	daemonCore->Cancel_And_Close_All_Pipes();

	for (int i=0; i<NUMBER_WORKERS; i++) {
		daemonCore->Send_Signal (workers[i].pid, SIGKILL);
	}

	return TRUE;	// to satify c++
}

void
main_pre_dc_init( int, char*[] )
{
}

void
main_pre_command_sock_init( )
{
}

// This function is called by dprintf - always display our pid in our
// log entries.
extern "C"
int
display_dprintf_header(FILE *fp)
{
	static pid_t mypid = 0;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	fprintf( fp, "[%ld] ", (long)mypid );

	return TRUE;
}

void
usage()
{
	dprintf( D_ALWAYS, "Usage: amazon-gahp\n");
	DC_Exit( 1 );
}

int
main_init( int argc, char ** const argv )
{

	dprintf(D_FULLDEBUG, "AMAZON-GAHP IO thread\n");

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

	Init();
	Register();
	Reconfig();

	// Find the name of worker thread
	MyString exec_name;
	char * amazon_gahp_worker_thread = param("AMAZON_GAHP_WORKER");
	if (amazon_gahp_worker_thread) {
		exec_name = amazon_gahp_worker_thread;
		free(amazon_gahp_worker_thread);
	}
	else {
		char * amazon_gahp_name = param("AMAZON_GAHP");
		ASSERT(amazon_gahp_name);
		exec_name.sprintf ("%s_worker_thread", amazon_gahp_name);
		free (amazon_gahp_name);
	}

	if( check_read_access_file(exec_name.Value()) == false ) {
		dprintf(D_ALWAYS, "Can't read worker thread(%s)\n", exec_name.Value());
		DC_Exit( 1 );
	}
	
	// Find the name of amazon library (e.g. perl script)
	MyString tmp_string;
	if( find_amazon_lib(tmp_string) == false ) {
		DC_Exit( 1 );
	}

	// Register all amazon commands
	if( registerAllAmazonCommands() == false ) {
		dprintf(D_ALWAYS, "Can't register Amazon Commands\n");
		DC_Exit( 1 );
	}
	
	int stdin_pipe = -1;
#if defined(WIN32)
	// if our parent is not DaemonCore, then our assumption that
	// the pipe we were passed in via stdin is overlapped-mode
	// is probably wrong. we therefore create a new pipe with the
	// read end overlapped and start a "forwarding thread"
	char* tmp;
	if ((tmp = daemonCore->InfoCommandSinfulString(daemonCore->getppid())) == NULL) {

		dprintf(D_FULLDEBUG, "parent is not DaemonCore; creating a forwarding thread\n");

		int pipe_ends[2];
		if (daemonCore->Create_Pipe(pipe_ends, true) == FALSE) {
			EXCEPT("failed to create forwarding pipe");
		}
		forwarding_pipe = pipe_ends[1];
		HANDLE thread_handle = (HANDLE)_beginthreadex(NULL,                   // default security
		                                              0,                      // default stack size
		                                              pipe_forward_thread,    // start function
		                                              NULL,                   // arg: write end of pipe
		                                              0,                      // not suspended
													  NULL);                  // don't care about the ID
		if (thread_handle == NULL) {
			EXCEPT("failed to create forwarding thread");
		}
		CloseHandle(thread_handle);
		stdin_pipe = pipe_ends[0];
	}
#endif

	if (stdin_pipe == -1) {
		// create a DaemonCore pipe from our stdin
		stdin_pipe = daemonCore->Inherit_Pipe(fileno(stdin),
		                                      false,    // read pipe
		                                      true,     // registerable
		                                      false);   // blocking
	}

	stdin_buffer.setPipeEnd(stdin_pipe);

	(void)daemonCore->Register_Pipe (stdin_buffer.getPipeEnd(),
					"stdin pipe",
					(PipeHandler)&stdin_pipe_handler,
					"stdin_pipe_handler");

	int i;
	for (i=0; i<NUMBER_WORKERS; i++) {

		workers[i].Init(i);

			// The IO (this) process cannot block, otherwise it's poosible
			// to create deadlock between these two pipes
		if (!daemonCore->Create_Pipe (workers[i].request_pipe,
					      true,	// read end registerable
					      false,	// write end not registerable
					      false,	// read end blocking
					      true	// write end blocking
					     ) ||
		    !daemonCore->Create_Pipe (workers[i].result_pipe,
					      true	// read end registerable
					     ) )
		{
			return -1;
		}

		workers[i].request_buffer.setPipeEnd(workers[i].request_pipe[1]);
		workers[i].result_buffer.setPipeEnd(workers[i].result_pipe[0]);

		(void)daemonCore->Register_Pipe (workers[i].result_buffer.getPipeEnd(),
										 "result pipe",
										 (PipeHandlercpp)&Worker::result_handler,
										 "Worker::result_handler",
										 (Service*)&workers[i]);
	}

	// Create child process
	// Register the reaper for the child process
	int reaper_id =
		daemonCore->Register_Reaper(
							"worker_thread_reaper",
							(ReaperHandler)&worker_thread_reaper,
							"worker_thread_reaper",
							NULL);



	flush_request_tid = 
		daemonCore->Register_Timer (1,
									1,
									(TimerHandler)&flush_pending_requests,
									"flush_pending_requests",
									NULL);
									
									  
	for (i=0; i<NUMBER_WORKERS; i++) {
		ArgList args;

		args.AppendArg(exec_name.Value());
		args.AppendArg("-f");

		MyString args_string;
		args.GetArgsStringForDisplay(&args_string);
		dprintf (D_FULLDEBUG, "Staring worker # %d: %s\n", i, args_string.Value());

			// We want IO thread to inherit these ends of pipes
		int std_fds[3];
		std_fds[0] = workers[i].request_pipe[0];
		std_fds[1] = workers[i].result_pipe[1];
		std_fds[2] = fileno(stderr);

		workers[i].pid = 
			daemonCore->Create_Process (
										exec_name.Value(),
										args,
										PRIV_UNKNOWN,
										reaper_id,
										FALSE,			// no command port
										NULL,
										NULL,
										NULL,
										NULL,
										std_fds);


		if (workers[i].pid > 0) {
			close (workers[i].request_pipe[0]);
			close (workers[i].result_pipe[1]);
		}
	}
			
		// Print out the GAHP version to the screen
		// now we're ready to roll
	printf ("%s\n", version);
	fflush(stdout);

	dprintf (D_FULLDEBUG, "AMAZON-GAHP IO initialized\n");

	return TRUE;
}

static int
stdin_pipe_handler(Service*, int) {

	MyString* line;
	while ((line = stdin_buffer.GetNextLine()) != NULL) {

		const char * command = line->Value();

		dprintf (D_ALWAYS, "got stdin: %s\n", command);

		Gahp_Args args;

		if (parse_gahp_command (command, &args) &&
			verify_gahp_command (args.argv, args.argc)) {

				// Catch "special commands first
			if (strcasecmp (args.argv[0], GAHP_COMMAND_RESULTS) == 0) {
					// Print number of results
				MyString rn_buff;
				rn_buff+=result_list.number();
				const char * commands [] = {
					GAHP_RESULT_SUCCESS,
					rn_buff.Value() };
				gahp_output_return (commands, 2);

					// Print each result line
				char * next;
				result_list.rewind();
				while ((next = result_list.next()) != NULL) {
					printf ("%s\n", next);
					fflush(stdout);
					result_list.deleteCurrent();
				}

				new_results_signaled = FALSE;
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_VERSION) == 0) {
				printf ("S %s\n", version);
				fflush (stdout);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				DC_Exit(0);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0) {
				async_mode = TRUE;
				new_results_signaled = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
				async_mode = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				return 0; // exit
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_COMMANDS) == 0) {
				StringList amazon_commands;
				int num_commands = 0;

				num_commands = allAmazonCommands(amazon_commands);
				num_commands += 7;

				const char *commands[num_commands];
				int i = 0;
				commands[i++] = GAHP_RESULT_SUCCESS;
				commands[i++] = GAHP_COMMAND_ASYNC_MODE_ON;
				commands[i++] = GAHP_COMMAND_ASYNC_MODE_OFF;
				commands[i++] = GAHP_COMMAND_RESULTS;
				commands[i++] = GAHP_COMMAND_QUIT;
				commands[i++] = GAHP_COMMAND_VERSION;
				commands[i++] = GAHP_COMMAND_COMMANDS;

				amazon_commands.rewind();
				char *one_amazon_command = NULL;

				while( (one_amazon_command = amazon_commands.next() ) != NULL ) {
					commands[i++] = one_amazon_command;
				}

				gahp_output_return (commands, i);
			} else {
				flush_request (0,	// general worker 
							   command);
				gahp_output_return_success(); 
			}
			
		} else {
			gahp_output_return_error();
		}

		delete line;
	}

	// check if GetNextLine() returned NULL because of an error or EOF
	if (stdin_buffer.IsError() || stdin_buffer.IsEOF()) {
		dprintf (D_ALWAYS, "stdin buffer closed, exiting\n");
		DC_Exit (1);
	}

	return TRUE;
}

static int 
result_pipe_handler(int id) {

	MyString* line;
	while ((line = workers[id].result_buffer.GetNextLine()) != NULL) {

		dprintf (D_FULLDEBUG, "Received from worker:\"%s\"\n", line->Value());

			// Add this to the list
		result_list.append (line->Value());

		if (async_mode) {
			if (!new_results_signaled) {
				printf ("R\n");
				fflush (stdout);
			}
			new_results_signaled = TRUE;	// So that we only do it once
		}

		delete line;
	}

	if (workers[id].result_buffer.IsError() || workers[id].result_buffer.IsEOF()) {
		DC_Exit(1);
	}

	return TRUE;
}

/*
void
process_next_request() {
	if (worker_thread_ready) {
		if (flush_next_request(request_pipe_out_fd)) {
			worker_thread_ready = false;
		}
	}
}
*/

int
worker_thread_reaper (Service*, int pid, int exit_status) {

	dprintf (D_ALWAYS, "Worker process pid=%d exited with status %d\n", 
			 pid, 
			 exit_status);

	// Our child exited (presumably b/c we got a QUIT command),
	// so should we
	// If we're in this function there shouldn't be much cleanup to be done,
	// except the command queue

	DC_Exit(1);
	return TRUE;
}

// Check the parameters to make sure the command
// is syntactically correct
static int
verify_gahp_command(char ** argv, int argc) {
	// Special Commands First
	if (strcasecmp (argv[0], GAHP_COMMAND_RESULTS) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_VERSION) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_COMMANDS) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
		// These are no-arg commands
		return verify_number_args (argc, 1);
	}

	return executeIOCheckFunc(argv[0], argv, argc); 
}

void
flush_request (int worker_id, const char * request) {

	dprintf (D_FULLDEBUG, "Sending %s to worker %d\n", 
			 request,
			 worker_id);

	MyString strRequest = request;
	strRequest += "\n";

	workers[worker_id].request_buffer.Write(strRequest.Value());

	daemonCore->Reset_Timer (flush_request_tid, 0, 1);
}


int flush_pending_requests() {
	for (int i=0; i<NUMBER_WORKERS; i++) {
		workers[i].request_buffer.Write();

		if (workers[i].request_buffer.IsError()) {
			dprintf (D_ALWAYS, "Worker %d request buffer error, exiting...\n", i);
			DC_Exit (1);
		}
	}

	return TRUE;
}

void
gahp_output_return (const char ** results, const int count) {
	int i=0;

	for (i=0; i<count; i++) {
		printf ("%s", results[i]);
		if (i < (count - 1 )) {
			printf (" ");
		}
	}


	printf ("\n");
	fflush(stdout);
}

static void
gahp_output_return_success() {
	const char* result[] = {GAHP_RESULT_SUCCESS};
	gahp_output_return (result, 1);
}

static void
gahp_output_return_error() {
	const char* result[] = {GAHP_RESULT_ERROR};
	gahp_output_return (result, 1);
}

int
Worker::result_handler(int) {
	return result_pipe_handler(id);
}

