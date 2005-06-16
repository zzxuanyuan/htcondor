/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "globus_utils.h"
#include "condor_config.h"

#include "schedd_client.h"
#include "io_loop.h"


// How often we contact the schedd (secs)
extern int contact_schedd_interval;

// How often we check results in the pipe (secs)
extern int check_requests_interval;


char *mySubSystem = "C_GAHP";	// used by Daemon Core

char * myUserName = NULL;

extern char * ScheddAddr;

int io_loop_pid = -1;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

int my_fork();
extern int schedd_loop( void* arg, Stream * s);


void
usage( char *name )
{
	dprintf( D_ALWAYS,
		"Usage: condor_gahp -s <schedd name> [-P <pool name>]\n",
		condor_basename( name ) );
	DC_Exit( 1 );
}

int
main_init( int argc, char ** const argv )
{

	dprintf(D_FULLDEBUG, "Welcome to the C-GAHP\n");

	// handle specific command line args
	int i = 1;
	while ( i < argc ) {
		if ( argv[i][0] != '-' )
			usage( argv[0] );

		switch( argv[i][1] ) {
		case 's':
			// don't check parent for schedd addr. use this one instead
			if ( argc <= i + 1 )
				usage( argv[0] );
			ScheddAddr = strdup( argv[i + 1] );
			i++;
			break;
		case 'P':
			// specify what pool (i.e. collector) to lookup the schedd name
			if ( argc <= i + 1 )
				usage( argv[0] );
			ScheddPool = strdup( argv[i + 1] );
			i++;
			break;
		default:
			usage( argv[0] );
			break;
		}

		i++;
	}

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

	Init();
	Register();
	Reconfig();

	(void)daemonCore->Register_Timer( 1,
									  (TimerHandler)&my_fork,
									  "my_fork", 
									  NULL );

	return TRUE;
}



int
temp_stdin_handler (int pipe) {
	static char buff[500];
	buff[read (0, buff, 499)] = '\0';
	dprintf (D_FULLDEBUG, "stdout handler called %s\n", buff);
	return TRUE;
}


int
my_fork () {

	//
	// There are two main continuing tasks:
	// 1) Listen for GAHP commands on stdin and display results to stdout
	// 2) Collect tasks from #1 and send them to schedd periodically
	// We're going to fork once in this method using DC::Create_Thread().
	// This (parent) thread will do #2, while the child will run in a loop doing #1
	// Both processes will hang around until (hopefully):
	//		child receives QUIT -> exit()s -> reaper will trigger -> parent DC_Exit()s
	//	or if parent receives SIGTERM/SIGKILL it terminates the child
	//


	// Create two pipes that threads will talk via

	// TODO: These pipes should be non-blocking! At least all the pipes
	// to/from IO thread should be non-blocking, otherwise there's a
	// possibility of deadlock between IO and worker thread (when
	// each is waiting for other to finsish reading the pipe)

	inter_thread_io_t * inter_thread_io = (inter_thread_io_t*)malloc (sizeof (inter_thread_io_t));
	if (daemonCore->Create_Pipe (inter_thread_io->request_pipe, true) == FALSE ||
		daemonCore->Create_Pipe (inter_thread_io->result_pipe, true) == FALSE ||
		daemonCore->Create_Pipe (inter_thread_io->request_ack_pipe, true) == FALSE) {
		return FALSE;
	}


	// Register the reaper for the child process
	int reaper_id =
		daemonCore->Register_Reaper(
				"schedDClientIOReaper",
				(ReaperHandler)io_loop_reaper,
				"schedDClientIOReaper");


	char * c_gahp_name = param ("CONDOR_GAHP");
	MyString exec_name;
	exec_name.sprintf ("%s_io_thread", c_gahp_name);
	free (c_gahp_name);

	MyString args;
	args.sprintf ("%s %d %d %d", 
				  exec_name.Value(),
				  inter_thread_io->request_pipe[1],
				  inter_thread_io->result_pipe[0],
				  inter_thread_io->request_ack_pipe[0]);

	dprintf (D_FULLDEBUG, "Startig IO thread: %s %s\n", 
			 exec_name.Value(), 
			 args.Value());

	int std[3];
	std[0] = dup(STDIN_FILENO);
	std[1] = dup(STDOUT_FILENO);
	std[2] = -1;

	// We want IO thread to inherit these ends of pipes
	int inherit_fds[4];
	inherit_fds[0] = inter_thread_io->request_pipe[1];
	inherit_fds[1] = inter_thread_io->result_pipe[0];
	inherit_fds[2] = inter_thread_io->request_ack_pipe[0];
	inherit_fds[3] = 0;

	
		// Let's hope this inherits the pipes' fds on winblows
	int child_pid = 
		daemonCore->Create_Process (
								exec_name.Value(),
								args.Value(),
								PRIV_UNKNOWN,
								reaper_id,
								FALSE,			// no command port
								NULL,
								NULL,
								FALSE,
								NULL,
								std,
								0,				// nice inc
								0,				// job opt mask
								inherit_fds);
								

	if (child_pid > 0) {
		close (STDIN_FILENO);
		close (std[0]);
		close (inter_thread_io->result_pipe[0]);
		close (inter_thread_io->request_ack_pipe[0]);
	}

	// This (parent) thread will service schedd requests
	schedd_thread ((void*)inter_thread_io, NULL);	// This should set timers and return immediately
	free(inter_thread_io);

	return TRUE;
}

int
io_loop_reaper (Service*, int pid, int exit_status) {

	dprintf (D_ALWAYS, "Child process %d exited\n", pid);

	// Our child exited (presumably b/c we got a QUIT command),
	// so should we
	// If we're in this function there shouldn't be much cleanup to be done,
	// except the command queue
	if (pid == io_loop_pid) //it better...
		io_loop_pid = -1;

	DC_Exit(1);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Misc daemoncore crap

void
Init() {
	contact_schedd_interval = param_integer ("C_GAHP_CONTACT_SCHEDD_DELAY", 20);
	check_requests_interval = param_integer ("C_GAHP_CHECK_NEW_REQUESTS_DELAY", 5);

}

void
Register() {
}

void
Reconfig()
{
	useXMLClassads = param_boolean( "GAHP_USE_XML_CLASSADS", false );
}


int
main_config( bool is_full )
{
	Reconfig();
	return TRUE;
}

int
main_shutdown_fast()
{
	if (io_loop_pid != -1)
		kill(io_loop_pid, SIGKILL);
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
	if (io_loop_pid != -1)
		kill(io_loop_pid, SIGTERM);
	DC_Exit(0);
	return TRUE;	// to satify c++
}

void
main_pre_dc_init( int argc, char* argv[] )
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
