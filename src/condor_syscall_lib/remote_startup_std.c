/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

/*
  This is the startup routine for "normal" condor programs - that is
  linked for both remote system calls and checkpointing.  "Standalone"
  condor programs - those linked for checkpointing, but not remote
  system calls, get linked with a completely different version of
  MAIN() (found in the "condor_ckpt" directory.  We assume here that
  our parent will provide command line arguments which tell us how to
  attach to a "command stream", and then provide commands which control
  how we start things.

  The command stream here is a list of commands issued by the
  condor_starter to the condor user process on a special file
  descriptor which the starter will make available to the user process
  when it is born (via a pipe).  The purpose is to control the user
  process's initial execution state and the checkpoint, restart, and
  migrate functions.

  If the command line arguments look like:
  		<program_name> '-_condor_cmd_fd' <fd_number>
  then <fd_number> is the file descriptor from which the process should
  read its commands.

  If the command line arguments look like
		<program_name> '-_condor_cmd_file' <file_name>
  then <file_name> is the name of a *local* file which the program should
  open and read commands from.  This interface is useful for debugging
  since you can run the command from a shell or a debugger without
  the need for a parent process to set up a pipe.

  In any case, once the command stream processing is complete, main()
  will be called in such a way that it appears the above described
  arguments never existed.

  Commands are:

	iwd <pathname>
		Change working directory to <pathname>.  This is intended to
		get the process into the working directory specified in the
		user's job description file.

	fd <n> <pathname> <open_mode>
		Open the file <pathname> with the mode <open_mode>.  Set things up
		(using dup2()), so that the file is available at file descriptor
		number <n>.  This is intended for redirection of the standard
		files 0, 1, and 2.

	ckpt <pathname>
		The process should write its state information to the file
		<pathname> so that it can be restarted at a later time.
		We don't actually do a checkpoint here, we just set things
		up so that when we checkpoint, the given file name will
		be used.  The actual checkpoint is triggered by recipt of
		the signal SIGTSTP, or by the user code calling the ckpt()
		routine.

	restart <pathname>
		The process should read its state information from the file
		<pathname> and do a restart.  (In this case, main() will not
		get called, as we are jumping back to wherever the process
		left off at checkpoint time.)

	migrate_to <host_name> <port_number>
		A process on host <host_name> is listening at <port_number> for
		a TCP connection.  This process should connect to the given
		port, and write its state information onto the TCP connection
		exactly as it would for a ckpt command.  It is intended that
		the other process is running the same a.out file that this
		process is, and will use the state information to effect
		a restart on the new machine (a migration).

	migrate_from <fd>
		This process's parent (the condor_starter) has passed it an open
		file descriptor <fd> which is in reality a TCP socket.  The
		remote process will write its state information onto the TCP
		socket, which this process will use to effect a restart.  Since
		the restart is on a different machine, this is a migration.

	exit <status>
		The process should exit now with status <status>.  This
		is intended to be issued after a "ckpt" or "migrate_to"
		command.  We don't want to assume that the process
		should always exit after one of these commands because
		we want the flexibility to create interim checkpoints.
		(It's not clear why we would want to want to send a
		copy of a process to another machine and continue
		running on the current machine, but we have that
		flexibility too if we want it...)

	end
		We are temporarily at the end of commands.  Now it is time to
		call main() or effect the restart as requested.  Note that
		the process should not close the file descriptor at this point.
		Instead a signal handler will be set up to handle the signal
		SIGTSTP.  The signal handler will invoke the command stream
		interpreter again.  This is done so that the condor_starter can
		send a "ckpt" or a "migrate_to" command to the process after it
		has been running for some time.  In the case of the "ckpt"
		command the name of the file could have been known in advance, but
		in the case of the "migrate_to" command the name and port
		number of the remote host are presumably not known until
		the migration becomes necessary.
*/


#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "condor_debug.h"
#include "condor_file_info.h"
static char *_FileName_ = __FILE__;

enum result { NOT_OK = 0, OK = 1, END };

enum command {
	IWD,
	FD,
	CKPT,
	RESTART,
	MIGRATE_TO,
	MIGRATE_FROM,
	EXIT,
	END_MARKER,
	NO_COMMAND
};

typedef struct {
	enum command id;
	char *name;
} COMMAND;

COMMAND CmdTable[] = {
	IWD,			"iwd",
	FD,				"fd",
	CKPT,			"ckpt",
	RESTART,		"restart",
	MIGRATE_TO,		"migrate_to",
	MIGRATE_FROM,	"migrate_from",
	EXIT,			"exit",
	END_MARKER,		"end",
	NO_COMMAND,		"",
};

#if 0
	static int		DebugFd;
	void debug_msg( const char *msg );
#endif

void _condor_interp_cmd_stream( int fd );
static void scan_cmd( char *buf, int *argc, char *argv[] );
static enum result do_cmd( int argc, char *argv[] );
static enum command find_cmd( const char *name );
static void display_cmd( int argc, char *argv[] );
static BOOLEAN condor_iwd( const char *path );
static BOOLEAN condor_fd( const char *num, const char *path, const char *open_mode );
static BOOLEAN condor_ckpt( const char *path );
static BOOLEAN condor_restart( );
static BOOLEAN condor_migrate_to( const char *host_addr, const char *port_num );
static BOOLEAN condor_migrate_from( const char *fd_no );
static BOOLEAN condor_exit( const char *status );

/* Defined in remote_startup_common.c */
extern void unblock_signals();
extern void display_ip_addr( unsigned int addr );
extern void open_std_file( int which );
extern void set_iwd();
extern int open_ckpt_file( const char *name, int flags, size_t n_bytes );
extern void get_ckpt_name();

extern volatile int InRestart;
extern void InitFileState();
extern void _condor_disable_uid_switching();

int
#if defined(HPUX)
_START( int argc, char *argv[], char **envp )
#else
MAIN( int argc, char *argv[], char **envp )
#endif
{
	int		cmd_fd = -1;
	char	*cmd_name;
	char	*extra;
	int		scm;
	char	*argv0;
	char 	*argv1;
	
		/* 
		   Since we're the user job, we want to just disable any
		   priv-state switching.  We would, eventually, anyway, since
		   we'd realize we weren't root, but then we hit the somewhat
		   expensive init_condor_ids() code in uids.c, which is
		   segfaulting on HPUX10 on a machine using YP.  So, we just
		   call a helper function here that sets the flags in uids.c
		   how we want them so we don't do switching, period.
		   -Derek Wright 7/21/98 */
	_condor_disable_uid_switching();

#undef WAIT_FOR_DEBUGGER
#if defined(WAIT_FOR_DEBUGGER)
	int		do_wait = 1;
	while( do_wait )
		;
#endif

	/* Some platforms have very picky strcmp()'s which like
	 * to coredump (IRIX) so make certain argv[1] points to something 
	 * by using an intermediate argv1 variable where neccesary */
	 if ( argc < 2 ) 
		argv1 = "\0";
	 else
		argv1 = argv[1];

		/*
		We must be started by a parent providing a command stream,
		therefore there must be at least 3 arguments.
		So here we check and see if we have been started properly
		with a command stream, as a condor_starter would do.  If
		not, we set syscalls to local, disable dprintf debug messages,
		print out a warning, and attempt to run normally "outside"
		of condor (with no checkpointing, remote syscalls, etc).  This
		allows users to have one condor-linked binary which can be run
		either inside or outside of condor.  -Todd, 5/97.
		*/
	if ( (argc < 3) ||
		 ( (strcmp("-_condor_cmd_fd",argv1) != MATCH) &&
		   (strcmp("-_condor_cmd_file",argv1) != MATCH) ) ) {
				/* Run the job "normally" outside of condor */
				SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
				DebugFlags = 0;		/* disable dprintf messages */
				InitFileState();  /* to create a file_state table so no SEGV */
				if ( strcmp("-_condor_nowarn",argv1) != MATCH ) {
					fprintf(stderr,"WARNING: This binary has been linked for Condor.\nWARNING: Setting up to run outside of Condor...\n");
				} else {
					/* compensate argument vector for flag */
					argv0 = argv[0];
					argv++;
					argc--;
					argv[0] = argv0;
				}

				/* Now start running user code and forget about condor */
#if defined(HPUX)
				exit(_start( argc, argv, envp ));
#else
				exit( main( argc, argv, envp ));
#endif
	}

	
	/* now setup signal handlers, etc */
	_condor_prestart( SYS_REMOTE );


#define USE_PIPES 0

#if USE_PIPES
	init_syscall_connection( TRUE );
#else
	init_syscall_connection( FALSE );
#endif

#if 0
	dprintf( D_ALWAYS, "User process started\n" );
	dprintf( D_ALWAYS, "\nOriginal\n" );
	DumpOpenFds();
	dprintf( D_ALWAYS, "END\n\n" );
	delay();
#endif

	if( strcmp("-_condor_cmd_fd",argv[1]) == MATCH ) {
#if 0
		dprintf( D_ALWAYS, "Found condor_cmd_fd\n" );
		delay();
#endif
		cmd_fd = strtol( argv[2], &extra, 0 );
		if( extra[0] ) {
			dprintf( D_ALWAYS, "Can't parse cmd stream fd (%s)\n", argv[2]);
			exit( 1 );
		}
#if 0
		dprintf( D_ALWAYS, "fd number is %d\n", cmd_fd );
		delay();
#endif
		/* scm = SetSyscalls( SYS_LOCAL | SYS_MAPPED ); */
		pre_open( cmd_fd, TRUE, FALSE );

#if 0
		dprintf( D_ALWAYS, "\nBefore reading commands\n" );
		DumpOpenFds();
		dprintf( D_ALWAYS, "END\n\n" );
		delay();
#endif


	} else if( strcmp("-_condor_cmd_file",argv[1]) == MATCH ) {

		dprintf( D_FULLDEBUG, "Found condor_cmd_file\n" );
		/* scm = SetSyscalls( SYS_LOCAL | SYS_MAPPED ); */
		cmd_fd = open( argv[2], O_RDONLY);
		if( cmd_fd < 0 ) {
			dprintf( D_ALWAYS, "Can't read cmd file \"%s\"\n", argv[2] );
			Suicide();
		}

		/* Some error in the command line syntax */
	} else {
		dprintf( D_ALWAYS, "Error in command line syntax\n" );
		Suicide();
	}

#if 0
	dprintf( D_ALWAYS, "\nCalling cmd stream processor\n" );
	delay();
#endif
	_condor_interp_cmd_stream( cmd_fd );
#if 0
	dprintf( D_ALWAYS, "Done\n\n" );
	delay();
#endif
	cmd_name = argv[0];
	argv += 2;
	argc -= 2;

   /*
   The flag '-_condor_debug_wait' can be set to setup an infinite loop so
   that we can attach to the user job.  This argument can be specified in
   the submit file, but must be the first argument to the job.  The argv
   is compensated so that the job gets the vector it expected.  --RR
   */
   if ( (argc > 1) && (strcmp(argv[1], "-_condor_debug_wait") == MATCH) )
   {
       int i = 1;
       argv ++;
       argc --;
       while (i);
   }

	argv[0] = cmd_name;

	unblock_signals();
	SetSyscalls( SYS_REMOTE | SYS_MAPPED );

#if 0
	dprintf( D_ALWAYS, "\nBefore calling main()\n" );
	DumpOpenFds();
	dprintf( D_ALWAYS, "END\n\n" );
#endif

#if 0
	DebugFd = syscall( SYS_open, "/tmp/mike", O_WRONLY | O_CREAT | O_TRUNC, 0664 );
	syscall( SYS_dup2, DebugFd, 23 );
	DebugFd = 23;
	debug_msg( "Hello World!\n" );
#endif

	set_iwd();
	get_ckpt_name();
	open_std_file( 0 );
	open_std_file( 1 );
	open_std_file( 2 );

	InRestart = FALSE;
		/* Now start running user code */
#if defined(HPUX)
	exit(_start( argc, argv, envp ));
#else
	exit( main( argc, argv, envp ));
#endif
}

void
_condor_interp_cmd_stream( int fd )
{
	BOOLEAN	at_end = FALSE;
	FILE	*fp = fdopen( fd, "r" );
	char	buf[1024];
	int		argc;
	char	*argv[256];
	int		scm;

	while( fgets(buf,sizeof(buf),fp) ) {
		scan_cmd( buf, &argc, argv );
		switch( do_cmd(argc,argv) ) {
		  case OK:
			break;
		  case NOT_OK:
			dprintf( D_ALWAYS, "?\n" );
			break;
		  case END:
			return;
		}
	}
	dprintf( D_ALWAYS, "ERROR: EOF on command stream\n" );
	Suicide();
}

static void
scan_cmd( char *buf, int *argc, char *argv[] )
{
	int		i;

	argv[0] = strtok( buf, " \n" );
	if( argv[0] == NULL ) {
		*argc = 0;
		return;
	}

	for( i = 1; argv[i] = strtok(NULL," \n"); i++ )
		;
	*argc = i;
}


static enum result
do_cmd( int argc, char *argv[] )
{
	if( argc == 0 ) {
		return FALSE;
	}

	switch( find_cmd(argv[0]) ) {
	  case END_MARKER:
		return END;
	  case IWD:
		if( argc != 2 ) {
			return FALSE;
		}
		return condor_iwd( argv[1] );
	  case FD:
		assert( argc == 4 );
		return condor_fd( argv[1], argv[2], argv[3] );
	  case RESTART:
		if( argc != 1 ) {
			return FALSE;
		}
		return condor_restart();
	  case CKPT:
		if( argc != 2 ) {
			return FALSE;
		}
		return condor_ckpt( argv[1] );
	  case MIGRATE_TO:
		if( argc != 3 ) {
			return FALSE;
		}
		return condor_migrate_to( argv[1], argv[2] );
	  case MIGRATE_FROM:
		if( argc != 2 ) {
			return FALSE;
		}
		return condor_migrate_from( argv[1] );
	  case EXIT:
		if( argc != 2 ) {
			return FALSE;
		}
		return condor_exit( argv[1] );
	  default:
		return FALSE;
	}
}

static enum command
find_cmd( const char *str )
{
	COMMAND	*ptr;

	for( ptr = CmdTable; ptr->id != NO_COMMAND; ptr++ ) {
		if( strcmp(ptr->name,str) == MATCH ) {
			return ptr->id;
		}
	}
	return NO_COMMAND;
}

static BOOLEAN
condor_iwd( const char *path )
{
#if 0
	dprintf( D_ALWAYS, "condor_iwd: path = \"%s\"\n", path );
	delay();
#endif
	REMOTE_syscall( CONDOR_chdir, path );
	Set_CWD( path );
	return TRUE;
}

static BOOLEAN
condor_fd( const char *num, const char *path, const char *open_mode )
{
	/* no longer used  - ignore */
	return TRUE;
}

static BOOLEAN
condor_ckpt( const char *path )
{
	dprintf( D_ALWAYS, "condor_ckpt: filename = \"%s\"\n", path );
	init_image_with_file_name( path );

	return TRUE;
}


static BOOLEAN
condor_restart()
{
	int		fd;
	size_t	n_bytes;

	dprintf( D_ALWAYS, "condor_restart:\n" );

#if 0
	fd = open_ckpt_file( "", O_RDONLY, n_bytes );
	init_image_with_file_descriptor( fd );
#else
	get_ckpt_name();
#endif
	restart();

		/* Can never get here - restart() jumps back into user code */
	return FALSE;
}

static BOOLEAN
condor_migrate_to( const char *host_name, const char *port_num )
{
	char 	*extra;
	long	port;

	port = strtol( port_num, &extra, 0 );
	if( extra[0] ) {
		return FALSE;
	}

	dprintf( D_FULLDEBUG,
		"condor_migrate_to: host = \"%s\", port = %d\n", host_name, port
	);
	return TRUE;
}

static BOOLEAN
condor_migrate_from( const char *fd_no )
{
	long	fd;
	char	*extra;

	fd = strtol( fd_no, &extra, 0 );
	if( extra[0] ) {
		return FALSE;
	}
	
	dprintf( D_FULLDEBUG, "condor_migrate_from: fd = %d\n", fd );
	return TRUE;
}

static BOOLEAN
condor_exit( const char *status )
{
	long	st;
	char	*extra;

	st = strtol( status, &extra, 0 );
	if( extra[0] ) {
		return FALSE;
	}
	
	dprintf( D_FULLDEBUG, "condor_exit: status = %d\n", st );
	return TRUE;
}


