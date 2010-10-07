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
#include <limits.h>
#include <string.h>
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_cronmgr.h"
#include "condor_cron.h"
#include "condor_string.h"

// Size of the buffer for reading from the child process
#define STDOUT_READBUF_SIZE	1024
#define STDOUT_LINEBUF_SIZE	8192
#define STDERR_READBUF_SIZE	128

// Cron's Line StdOut Buffer constructor
CronJobOut::CronJobOut( class CronJobBase *job_arg ) :
		LineBuffer( STDOUT_LINEBUF_SIZE )
{
	this->job = job_arg;
}

// Output function
int
CronJobOut::Output( const char *buf, int len )
{
	// Ignore empty lines
	if ( 0 == len ) {
		return 0;
	}

	// Check for record delimitter
	if ( '-' == buf[0] ) {
		return 1;
	}

	// Build up the string
	const char	*prefix = job->GetPrefix( );
	int		fulllen = len;
	if ( prefix ) {
		fulllen += strlen( prefix );
	}
	char	*line = (char *) malloc( fulllen + 1 );
	if ( NULL == line ) {
		dprintf( D_ALWAYS,
				 "cronjob: Unable to duplicate %d bytes\n",
				 fulllen );
		return -1;
	}
	if ( prefix ) {
		strcpy( line, prefix );
	} else {
		*line = '\0';
	}
	strcat( line, buf );

	// Queue it up, get out
	lineq.enqueue( line );

	// Done
	return 0;
}

// Get size of the queue
int
CronJobOut::GetQueueSize( void )
{
	return lineq.Length( );
}

// Flush the queue
int
CronJobOut::FlushQueue( void )
{
	int		size = lineq.Length( );
	char	*line;

	// Flush out the queue
	while( ! lineq.dequeue( line ) ) {
		free( line );
	}

	// Return the size
	return size;
}

// Get next queue element
char *
CronJobOut::GetLineFromQueue( void )
{
	char	*line;

	if ( ! lineq.dequeue( line ) ) {
		return line;
	} else {
		return NULL;
	}
}

// Cron's Line StdErr Buffer constructor
CronJobErr::CronJobErr( class CronJobBase *job_arg ) :
		LineBuffer( 128 )
{
	this->job = job_arg;
}

// StdErr Output function
int
CronJobErr::Output( const char *buf, int   /*len*/ )
{
	dprintf( D_FULLDEBUG, "%s: %s\n", job->GetName( ), buf );

	// Done
	return 0;
}

// CronJob constructor
CronJobBase::CronJobBase( const char *   /*mgrName*/, const char *jobName )
		: m_period( UINT_MAX ),
		  m_runTimer( -1 ),
		  m_mode( CRON_ILLEGAL ),
		  m_state( CRON_NOINIT ),
		  m_pid( -1 ),
		  m_stdOut( -1 ),
		  m_stdErr( -1 ),
		  //m_childFds( [-1,-1,-1] ),
		  m_reaperId( -1 ),
		  m_stdOutBuf( NULL ),
		  m_stdErrBuf( NULL ),
		  m_marked( false ),
		  m_killTimer( -1 ),
		  m_numOutputs( 0 ),			// No data produced yet
		  m_eventHandler( NULL ),
		  m_eventService( NULL ),
		  m_optKill( false ),
		  m_optReconfig( false ),
		  m_optIdle( false )
{
	for( int i = 0; i < 3;  i++ ) {
		m_childFds[i] = -1;
	}

	// Build my output buffers
	m_stdOutBuf = new CronJobOut( this );
	m_stdErrBuf = new CronJobErr( this );

# if CRONJOB_PIPEIO_DEBUG
	TodoBufSize = 20 * 1024;
	TodoWriteNum = TodoBufWrap = TodoBufOffset = 0;
	TodoBuffer = (char *) malloc( TodoBufSize );
# endif

	// Store the name, etc.
	SetName( jobName );

	// Register my reaper
	m_reaperId = daemonCore->Register_Reaper( 
		"Cron_Reaper",
		(ReaperHandlercpp) &CronJobBase::Reaper,
		"Cron Reaper",
		this );
}

// CronJob destructor
CronJobBase::~CronJobBase( )
{
	dprintf( D_ALWAYS, "Cron: Deleting job '%s' (%s)\n",
			 GetName(), GetPath() );
	dprintf( D_FULLDEBUG, "Cron: Deleting timer for '%s'; ID = %d\n",
			 GetName(), m_runTimer );

	// Delete the timer FIRST
	if ( m_runTimer >= 0 ) {
		daemonCore->Cancel_Timer( m_runTimer );
	}

	// Kill job if it's still running
	KillJob( true );

	// Close FDs
	CleanAll( );

	// Delete the buffers
	delete m_stdOutBuf;
	delete m_stdErrBuf;
}

// Initialize
int
CronJobBase::Initialize( )
{
	// If we're already initialized, do nothing...
	if ( m_state != CRON_NOINIT )	{
		return 0;
	}

	// Update our state to idle..
	m_state = CRON_IDLE;

	dprintf( D_ALWAYS, "Cron: Initializing job '%s' (%s)\n", 
			 GetName(), GetPath() );

	// Schedule & see if we should run...
	return Schedule( );
}

// Set job characteristics: Name
int
CronJobBase::SetName( const char *newName )
{
	if ( NULL == newName ) {
		return -1;
	}
	m_name = newName;
	return 0;
}

// Set job characteristics: Prefix
int
CronJobBase::SetPrefix( const char *newPrefix )
{
	if ( NULL == newPrefix ) {
		return -1;
	}
	m_prefix = newPrefix;
	return 0;
}

// Set job characteristics: Path
int
CronJobBase::SetPath( const char *newPath )
{
	if ( NULL == newPath ) {
		return -1;
	}
	m_path = newPath;
	return 0;
}

// Set job characteristics: Command line args
int
CronJobBase::SetArgs( ArgList const &new_args )
{
	m_args.Clear();
	return AddArgs(new_args);
}

// Set job characteristics: Path
int
CronJobBase::SetConfigVal( const char *newPath )
{
	if ( NULL == newPath ) {
		m_configValProg = "";
	} else {
		m_configValProg = newPath;
	}
	return 0;
}

// Set job characteristics: Environment
int
CronJobBase::SetEnv( char const * const *env_array )
{
	m_env.Clear();
	return !m_env.MergeFrom(env_array);
}

int
CronJobBase::AddEnv( char const * const *env_array )
{
	return !m_env.MergeFrom(env_array);
}

// Set job characteristics: CWD
int
CronJobBase::SetCwd( const char *newCwd )
{
	m_cwd = newCwd;
	return 0;
}

// Set job characteristics: Kill option
int
CronJobBase::SetKill( bool kill )
{
	m_optKill = kill;
	return 0;
}

// Set job characteristics: Reconfig option
int
CronJobBase::SetReconfig( bool reconfig )
{
	m_optReconfig = reconfig;
	return 0;
}

// Add to the job's path
int
CronJobBase::AddPath( const char *newPath )
{
	if ( m_path.Length() ) {
		m_path += ":";
	}
	m_path += newPath;
	return 0;
}

// Add to the job's arg list
int
CronJobBase::AddArgs( ArgList const &new_args )
{
	m_args.AppendArgsFromArgList(new_args);
	return 1;
}

// Set job died handler
int
CronJobBase::SetEventHandler(
	CronEventHandler	NewHandler,
	Service				*s )
{
	// No nests, etc.
	if( m_eventHandler ) {
		return -1;
	}

	// Just store 'em & go
	m_eventHandler = NewHandler;
	m_eventService = s;
	return 0;
}

// Set job characteristics
int
CronJobBase::SetPeriod( CronJobMode newMode, unsigned newPeriod )
{
	// Verify that the mode seleted is valid
	if (  ( CRON_WAIT_FOR_EXIT != newMode ) && ( CRON_PERIODIC != newMode )  ) {
		dprintf( D_ALWAYS, "Cron: illegal mode selected for job '%s'\n",
				 GetName() );
		return -1;
	} else if (  ( CRON_PERIODIC == newMode ) && ( 0 == newPeriod )  ) {
		dprintf( D_ALWAYS, 
				 "Cron: Job '%s'; Periodic requires non-zero period\n",
				 GetName() );
		return -1;
	}

	// Any work to do?
	if (  ( m_mode == newMode ) && ( m_period == newPeriod )  ) {
		return 0;
	}

	// Mode change; cancel the existing timer
	if (  ( m_mode != newMode ) && ( m_runTimer >= 0 )  ) {
		daemonCore->Cancel_Timer( m_runTimer );
		m_runTimer = -1;
	}

	// Store the period; is it a periodic?
	m_mode = newMode;
	m_period = newPeriod;

	// Schedule a run..
	return Schedule( );
}

// Reconfigure a running job
int
CronJobBase::Reconfig( void )
{
	// Only do this to running jobs with the reconfig option set
	if (  ( ! m_optReconfig ) || ( CRON_RUNNING != m_state )  ) {
		return 0;
	}

	// Don't send the HUP before it's first output block
	if ( ! m_numOutputs ) {
		dprintf( D_ALWAYS,
				 "Not HUPing '%s' pid %d before it's first output\n",
				 GetName(), m_pid );
		return 0;
	}

	// HUP it; if it dies it'll get the new config when it restarts
	if ( m_pid >= 0 )
	{
			// we want this D_ALWAYS, since it's pretty rare anyone
			// actually wants a SIGHUP, and to aid in debugging, it's
			// best to always log it when we do so everyone sees it.
		dprintf( D_ALWAYS, "Cron: Sending HUP to '%s' pid %d\n",
				 GetName(), m_pid );
		return daemonCore->Send_Signal( m_pid, SIGHUP );
	}

	// Otherwise, all ok
	return 0;
}

// Schedule a run?
int
CronJobBase::Schedule( void )
{
	// If we're not initialized yet, do nothing...
	if ( CRON_NOINIT == m_state ) {
		return 0;
	}

	// Now, schedule the job to run..
	int	status = 0;

	// It's not a periodic -- just start it
	if ( CRON_WAIT_FOR_EXIT == m_mode ) {
		status = StartJob( );

	} else {				// Periodic
		// Set the job's timer
		status = SetTimer( m_period, m_period );

		// Start the first run..
		if (  ( 0 == status ) && ( CRON_IDLE == m_state )  ) {
			status = RunJob( );
		}
	}

	// Nothing to do for now
	return status;
}

// Schdedule the job to run
int
CronJobBase::RunJob( void )
{

	// Make sure that the job is idle!
	if ( ( m_state != CRON_IDLE ) && ( m_state != CRON_DEAD ) ) {
		dprintf( D_ALWAYS, "Cron: Job '%s' is still running!\n", GetName() );

		// If we're not supposed to kill the process, just skip this timer
		if ( m_optKill ) {
			return KillJob( false );
		} else {
			return -1;
		}
	}

	// Check output queue!
	if ( m_stdOutBuf->FlushQueue( ) ) {
		dprintf( D_ALWAYS, "Cron: Job '%s': Queue not empty!\n", GetName() );
	}

	// Job not running, just start it
	dprintf( D_JOB, "Cron: Running job '%s' (%s)\n",
			 GetName(), GetPath() );

	// Start it up
	return RunProcess( );
}

// Handle the kill timer
void
CronJobBase::KillHandler( void )
{

	// Log that we're here
	dprintf( D_FULLDEBUG, "Cron: KillHandler for job '%s'\n", GetName() );

	// If we're idle, we shouldn't be here.
	if ( CRON_IDLE == m_state ) {
		dprintf( D_ALWAYS, "Cron: Job '%s' already idle (%s)!\n", 
			GetName(), GetPath() );
		return;
	}

	// Kill it.
	KillJob( false );
}

// Start a job
int
CronJobBase::StartJob( void )
{
	if ( CRON_IDLE != m_state ) {
		dprintf( D_ALWAYS, "Cron: Job '%s' not idle!\n", GetName() );
		return 0;
	}
	dprintf( D_JOB, "Cron: Starting job '%s' (%s)\n",
			 GetName(), GetPath() );

	// Check output queue!
	if ( m_stdOutBuf->FlushQueue( ) ) {
		dprintf( D_ALWAYS, "Cron: Job '%s': Queue not empty!\n", GetName() );
	}

	// Run it
	return RunProcess( );
}

// Child reaper
int
CronJobBase::Reaper( int exitPid, int exitStatus )
{
	if( WIFSIGNALED(exitStatus) ) {
		dprintf( D_FULLDEBUG, "Cron: '%s' (pid %d) exit_signal=%d\n",
				 GetName(), exitPid, WTERMSIG(exitStatus) );
	} else {
		dprintf( D_FULLDEBUG, "Cron: '%s' (pid %d) exit_status=%d\n",
				 GetName(), exitPid, WEXITSTATUS(exitStatus) );
	}

	// What if the PIDs don't match?!
	if ( exitPid != m_pid ) {
		dprintf( D_ALWAYS, "Cron: WARNING: Child PID %d != Exit PID %d\n",
				 m_pid, exitPid );
	}
	m_pid = 0;

	// Read the stderr & output
	if ( m_stdOut >= 0 ) {
		StdoutHandler( m_stdOut );
	}
	if ( m_stdErr >= 0 ) {
		StderrHandler( m_stdErr );
	}

	// Clean up it's file descriptors
	CleanAll( );

	// We *should* be in CRON_RUNNING state now; check this...
	switch ( m_state )
	{
		// Normal death
	case CRON_RUNNING:
		m_state = CRON_IDLE;				// Note it's death
		if ( CRON_WAIT_FOR_EXIT == m_mode ) {
			if ( 0 == m_period ) {			// ExitTime mode, no delay
				StartJob( );
			} else {						// ExitTime mode with delay
				SetTimer( m_period, TIMER_NEVER );
			}
		}
		break;

		// Huh?  Should never happen
	case CRON_IDLE:
	case CRON_DEAD:
		dprintf( D_ALWAYS, "CronJob::Reaper:: Job %s in state %s: Huh?\n",
				 GetName(), StateString() );
		break;							// Do nothing

		// Waiting for it to die...
	case CRON_TERMSENT:
	case CRON_KILLSENT:
		break;							// Do nothing at all

		// We've sent the process a signal, waiting for it to die
	default:
		m_state = CRON_IDLE;			// Note that it's dead

		// Cancel the kill timer if required
		KillTimer( TIMER_NEVER );

		// Re-start the job
		if ( CRON_PERIODIC == m_mode ) {	// Periodic
			RunJob( );
		} else if ( 0 == m_period ) {		// ExitTime mode, no delay
			StartJob( );
		} else {							// ExitTime mode with delay
			SetTimer( m_period, TIMER_NEVER );
		}
		break;

	}

	// Note that we're dead
	if ( CRON_KILLED == m_mode ) {
		m_state = CRON_DEAD;
	}

	// Process the output
	ProcessOutputQueue( );

	// Finally, notify my manager
	if( m_eventHandler ) {
		(m_eventService->*m_eventHandler)( this, CONDOR_CRON_JOB_DIED );
	}

	return 0;
}

// Publisher
int
CronJobBase::ProcessOutputQueue( void )
{
	int		status = 0;
	int		linecount = m_stdOutBuf->GetQueueSize( );

	// If there's data, process it...
	if ( linecount != 0 ) {
		dprintf( D_FULLDEBUG, "%s: %d lines in Queue\n",
				 GetName(), linecount );

		// Read all of the data from the queue
		char	*linebuf;
		while( ( linebuf = m_stdOutBuf->GetLineFromQueue( ) ) != NULL ) {
			int		tmpstatus = ProcessOutput( linebuf );
			if ( tmpstatus ) {
				status = tmpstatus;
			}
			free( linebuf );
			linecount--;
		}

		// Sanity checks
		int		tmp = m_stdOutBuf->GetQueueSize( );
		if ( 0 != linecount ) {
			dprintf( D_ALWAYS, "%s: %d lines remain!!\n",
					 GetName(), linecount );
		} else if ( 0 != tmp ) {
			dprintf( D_ALWAYS, "%s: Queue reports %d lines remain!\n",
					 GetName(), tmp );
		} else {
			// The NULL output means "end of block", so go publish
			ProcessOutput( NULL );
			m_numOutputs++;				// Increment # of valid output blocks
		}
	}
	return 0;
}

// Start a job
int
CronJobBase::RunProcess( void )
{
	ArgList final_args;

	// Create file descriptors
	if ( OpenFds( ) < 0 ) {
		dprintf( D_ALWAYS, "Cron: Error creating FDs for '%s'\n",
				 GetName() );
		return -1;
	}

	// Add the name to the argument list, then any specified in the config
	final_args.AppendArg( m_name.Value() );
	if( m_args.Count() ) {
		final_args.AppendArgsFromArgList( m_args );
	}

	// Create the priv state for the process
	priv_state priv;
# ifdef WIN32
	// WINDOWS
	priv = PRIV_CONDOR;
# else
	// UNIX
	priv = PRIV_USER_FINAL;
	uid_t uid = get_condor_uid( );
	if ( uid == (uid_t) -1 )
	{
		dprintf( D_ALWAYS, "Cron: Invalid UID -1\n" );
		return -1;
	}
	gid_t gid = get_condor_gid( );
	if ( gid == (uid_t) -1 )
	{
		dprintf( D_ALWAYS, "Cron: Invalid GID -1\n" );
		return -1;
	}
	set_user_ids( uid, gid );
# endif

	// Create the process, finally..
	m_pid = daemonCore->Create_Process(
		m_path.Value(),		// Path to executable
		final_args,			// argv
		priv,				// Priviledge level
		m_reaperId,			// ID Of reaper
		FALSE,				// Command port?  No
		&m_env, 			// Env to give to child
		m_cwd.Value(),		// Starting CWD
		NULL,				// Process family info
		NULL,				// Socket list
		m_childFds,			// Stdin/stdout/stderr
		0 );				// Nice increment

	// Restore my priv state.
	uninit_user_ids( );

	// Close the child FDs
	CleanFd( &m_childFds[0] );
	CleanFd( &m_childFds[1] );
	CleanFd( &m_childFds[2] );

	// Did it work?
	if ( m_pid <= 0 ) {
		dprintf( D_ALWAYS, "Cron: Error running job '%s'\n", GetName() );
		CleanAll( );
		return -1;
	}

	// All ok here
	m_state = CRON_RUNNING;

	// Finally, notify my manager
	if( m_eventHandler ) {
		(m_eventService->*m_eventHandler)( this, CONDOR_CRON_JOB_START );
	}

	// All ok!
	return 0;
}

// Debugging
# if CRONJOB_PIPEIO_DEBUG
void
CronJobBase::TodoWrite( void )
{
	char	fname[1024];
	FILE	*fp;
	snprintf( fname, 1024, "todo.%s.%06d.%02d", name, getpid(), TodoWriteNum++ );
	dprintf( D_ALWAYS, "%s: Writing input log '%s'\n", GetName(), fname );

	if ( ( fp = safe_fopen_wrapper( fname, "w" ) ) != NULL ) {
		if ( TodoBufWrap ) {
			fwrite( TodoBuffer + TodoBufOffset,
					TodoBufSize - TodoBufOffset,
					1,
					fp );
		}
		fwrite( TodoBuffer, TodoBufOffset, 1, fp );
		fclose( fp );
	}
	TodoBufOffset = 0;
	TodoBufWrap = 0;
}
# endif

// Data is available on Standard Out.  Read it!
//  Note that we set the pipe to be non-blocking when we created it
int
CronJobBase::StdoutHandler ( int   /*pipe*/ )
{
	char			buf[STDOUT_READBUF_SIZE];
	int				bytes;
	int				reads = 0;

	// Read 'til we suck up all the data (or loop too many times..)
	while ( ( ++reads < 10 ) && ( m_stdOut >= 0 ) ) {

		// Read a block from it
		bytes = daemonCore->Read_Pipe( m_stdOut, buf, STDOUT_READBUF_SIZE );

		// Zero means it closed
		if ( bytes == 0 ) {
			dprintf(D_FULLDEBUG, "Cron: STDOUT closed for '%s'\n", GetName());
			daemonCore->Close_Pipe( m_stdOut );
			m_stdOut = -1;
		}

		// Positve value is byte count
		else if ( bytes > 0 ) {
			const char	*bptr = buf;

			// TODO
# 		  if CRONJOB_PIPEIO_DEBUG
			if ( TodoBuffer ) {
				char	*OutPtr = TodoBuffer + TodoBufOffset;
				int		Count = bytes;
				char	*InPtr = buf;
				while( Count-- ) {
					*OutPtr++ = *InPtr++;
					if ( ++TodoBufOffset >= TodoBufSize ) {
						TodoBufOffset = 0;
						TodoBufWrap++;
						OutPtr = TodoBuffer;
					}
				}
			}
#		  endif
			// End TODO

			// stdOutBuf->Output() returns 1 if it finds '-', otherwise 0,
			// so that's what Buffer returns, too...
			while ( m_stdOutBuf->Buffer( &bptr, &bytes ) > 0 ) {
				ProcessOutputQueue( );
			}
		}

		// Negative is an error; check for EWOULDBLOCK
		else if (  ( EWOULDBLOCK == errno ) || ( EAGAIN == errno )  ) {
			break;			// No more data; break out; we're done
		}

		// Something bad
		else {
			dprintf( D_ALWAYS,
					 "Cron: read STDOUT failed for '%s' %d: '%s'\n",
					 GetName(), errno, strerror( errno ) );
			return -1;
		}
	}
	return 0;
}

// Data is available on Standard Error.  Read it!
//  Note that we set the pipe to be non-blocking when we created it
int
CronJobBase::StderrHandler ( int   /*pipe*/ )
{
	char			buf[STDERR_READBUF_SIZE];
	int				bytes;

	// Read a block from it
	bytes = daemonCore->Read_Pipe( m_stdErr, buf, STDERR_READBUF_SIZE );

	// Zero means it closed
	if ( bytes == 0 )
	{
		dprintf( D_FULLDEBUG, "Cron: STDERR closed for '%s'\n", GetName() );
		daemonCore->Close_Pipe( m_stdErr );
		m_stdErr = -1;
	}

	// Positve value is byte count
	else if ( bytes > 0 )
	{
		const char	*bptr = buf;

		while( m_stdErrBuf->Buffer( &bptr, &bytes ) > 0 ) {
			// Do nothing for now
		}
	}

	// Negative is an error; check for EWOULDBLOCK
	else if (  ( errno != EWOULDBLOCK ) && ( errno != EAGAIN )  )
	{
		dprintf( D_ALWAYS,
				 "Cron: read STDERR failed for '%s' %d: '%s'\n",
				 GetName(), errno, strerror( errno ) );
		return -1;
	}


	// Flush the buffers
	m_stdErrBuf->Flush();
	return 0;
}

// Create the job's file descriptors
int
CronJobBase::OpenFds ( void )
{
	int	tmpfds[2];

	// stdin goes to the bit bucket
	m_childFds[0] = -1;

	// Pipe to stdout
	if ( !daemonCore->Create_Pipe( tmpfds,	// pipe ends
								   true,	// read end registerable
								   false,	// write end not registerable
								   true		// read end nonblocking
								   ) ) {
		dprintf( D_ALWAYS, "Cron: Can't create pipe, errno %d : %s\n",
				 errno, strerror( errno ) );
		CleanAll( );
		return -1;
	}
	m_stdOut = tmpfds[0];
	m_childFds[1] = tmpfds[1];
	daemonCore->Register_Pipe( m_stdOut,
							   "Standard Out",
							   (PipeHandlercpp) & CronJobBase::StdoutHandler,
							   "Standard Out Handler",
							   this );

	// Pipe to stderr
	if ( !daemonCore->Create_Pipe( tmpfds,	// pipe ends	
								   true,	// read end registerable
								   false,	// write end not registerable
								   true		// read end nonblocking
				    ) ) {
		dprintf( D_ALWAYS, "Cron: Can't create STDERR pipe, errno %d : %s\n",
				 errno, strerror( errno ) );
		CleanAll( );
		return -1;
	}
	m_stdErr = tmpfds[0];
	m_childFds[2] = tmpfds[1];
	daemonCore->Register_Pipe( m_stdErr,
							   "Standard Error",
							   (PipeHandlercpp) & CronJobBase::StderrHandler,
							   "Standard Error Handler",
							   this );

	// Done; all ok
	return 0;
}

// Clean up all file descriptors & FILE pointers
void
CronJobBase::CleanAll ( void )
{
	CleanFd( &m_stdOut );
	CleanFd( &m_stdErr );
	CleanFd( &m_childFds[0] );
	CleanFd( &m_childFds[1] );
	CleanFd( &m_childFds[2] );
}

// Clean up a FILE *
void
CronJobBase::CleanFile ( FILE **file )
{
	if ( NULL != *file ) {
		fclose( *file );
		*file = NULL;
	}
}

// Clean up a file descriptro
void
CronJobBase::CleanFd ( int *fd )
{
	if ( *fd >= 0 ) {
		daemonCore->Close_Pipe( *fd );
		*fd = -1;
	}
}

// Kill a job
int
CronJobBase::KillJob( bool force )
{
	// Change our mode
	m_mode = CRON_KILLED;

	// Idle?
	if ( ( CRON_IDLE == m_state ) || ( CRON_DEAD == m_state ) ) {
		return 0;
	}

	// Not running?
	if ( m_pid <= 0 ) {
		dprintf( D_ALWAYS, "Cron: '%s': Trying to kill illegal PID %d\n",
				 GetName(), m_pid );
		return -1;
	}

	// Kill the process *hard*?
	if ( ( force ) || ( CRON_TERMSENT == m_state )  ) {
		dprintf( D_JOB, "Cron: Killing job '%s' with SIGKILL, pid = %d\n", 
				 GetName(), m_pid );
		if ( daemonCore->Send_Signal( m_pid, SIGKILL ) == 0 ) {
			dprintf( D_ALWAYS,
					 "Cron: job '%s': Failed to send SIGKILL to %d\n",
					 GetName(), m_pid );
		}
		m_state = CRON_KILLSENT;
		KillTimer( TIMER_NEVER );	// Cancel the timer
		return 0;
	} else if ( CRON_RUNNING == m_state ) {
		dprintf( D_JOB, "Cron: Killing job '%s' with SIGTERM, pid = %d\n", 
				 GetName(), m_pid );
		if ( daemonCore->Send_Signal( m_pid, SIGTERM ) == 0 ) {
			dprintf( D_ALWAYS,
					 "Cron: job '%s': Failed to send SIGTERM to %d\n",
					 GetName(), m_pid );
		}
		m_state = CRON_TERMSENT;
		KillTimer( 1 );				// Schedule hard kill in 1 sec
		return 1;
	} else {
		return -1;					// Nothing else to do!
	}
}

// Set the job timer
int
CronJobBase::SetTimer( unsigned first, unsigned period_arg )
{
	// Reset the timer
	if ( m_runTimer >= 0 )
	{
		daemonCore->Reset_Timer( m_runTimer, first, period_arg );
		if( period_arg == TIMER_NEVER ) {
			dprintf( D_FULLDEBUG,
					 "Cron: timer ID %d reset to first: %u, period: NEVER\n",
					 m_runTimer, first );
		} else {
			dprintf( D_FULLDEBUG,
					 "Cron: timer ID %d reset to first: %u, period: %u\n",
					 m_runTimer, first, m_period );
		}
	}

	// Create a periodic timer
	else
	{
		// Debug
		dprintf( D_FULLDEBUG, 
				 "Cron: Creating timer for job '%s'\n", GetName() );
		TimerHandlercpp handler =
			(  ( CRON_WAIT_FOR_EXIT == m_mode ) ? 
			   (TimerHandlercpp)& CronJobBase::StartJob :
			   (TimerHandlercpp)& CronJobBase::RunJob );
		m_runTimer = daemonCore->Register_Timer(
			first,
			period_arg,
			handler,
			"RunJob",
			this );
		if ( m_runTimer < 0 ) {
			dprintf( D_ALWAYS, "Cron: Failed to create timer\n" );
			return -1;
		}
		if( period_arg == TIMER_NEVER ) {
			dprintf( D_FULLDEBUG, "Cron: new timer ID %d set to first: %u, "
					 "period: NEVER\n", m_runTimer, first );
		} else {
			dprintf( D_FULLDEBUG, "Cron: new timer ID %d set to first: %u, "
					 "period: %u\n", m_runTimer, first, m_period );
		}
	} 

	return 0;
}

// Start the kill timer
int
CronJobBase::KillTimer( unsigned seconds )
{
	// Cancel request?
	if ( TIMER_NEVER == seconds ) {
		dprintf( D_FULLDEBUG, "Cron: Canceling kill timer for '%s'\n",
				 GetName() );
		if ( m_killTimer >= 0 ) {
			return daemonCore->Reset_Timer( 
				m_killTimer, TIMER_NEVER, TIMER_NEVER );
		}
		return 0;
	}

	// Reset the timer
	if ( m_killTimer >= 0 )
	{
		daemonCore->Reset_Timer( m_killTimer, seconds, 0 );
		dprintf( D_FULLDEBUG, "Cron: Kill timer ID %d reset to %us\n", 
				 m_killTimer, seconds );
	}

	// Create the Kill timer
	else
	{
		// Debug
		dprintf( D_FULLDEBUG, "Cron: Creating kill timer for '%s'\n", 
				 GetName() );
		m_killTimer = daemonCore->Register_Timer(
			seconds,
			0,
			(TimerHandlercpp)& CronJobBase::KillHandler,
			"KillJob",
			this );
		if ( m_killTimer < 0 ) {
			dprintf( D_ALWAYS, "Cron: Failed to create kill timer\n" );
			return -1;
		}
		dprintf( D_FULLDEBUG, "Cron: new kill timer ID = %d set to %us\n",
				 m_killTimer, seconds );
	}

	return 0;
}

// Convert state value into string (for printing)
const char *
CronJobBase::StateString( CronJobState state_arg )
{
	switch( state_arg )
	{
	case CRON_IDLE:
		return "Idle";
	case CRON_RUNNING:
		return "Running";
	case CRON_TERMSENT:
		return "TermSent";
	case CRON_KILLSENT:
		return "KillSent";
	case CRON_DEAD:
		return "Dead";
	default:
		return "Unknown";
	}
}

// Same, but uses the job's current state
const char *
CronJobBase::StateString( void )
{
	return StateString( m_state );
}
