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

#ifndef _CONDOR_CRONJOB_H
#define _CONDOR_CRONJOB_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "linebuffer.h"
#include "Queue.h"
#include "env.h"

// Cron's StdOut Line Buffer
class CronJobOut : public LineBuffer
{
  public:
	CronJobOut( class CronJobBase *job );
	virtual ~CronJobOut( void ) {};
	virtual int Output( const char *buf, int len );
	int GetQueueSize( void );
	char *GetLineFromQueue( void );
	int FlushQueue( void );
  private:
	Queue<char *>		lineq;
	class CronJobBase	*job;
};

// Cron's StdErr Line Buffer
class CronJobErr : public LineBuffer
{
  public:
	CronJobErr( class CronJobBase *job );
	virtual ~CronJobErr( void ) {};
	virtual int Output( const char *buf, int len );
  private:
	class CronJobBase	*job;
};

// Job's state
typedef enum {
	CRON_NOINIT,			// Not initialized yet
	CRON_IDLE, 				// Job is idle / not running
	CRON_RUNNING,			// Job is running
	CRON_TERMSENT,			// SIGTERM sent to job, waiting for SIGCHLD
	CRON_KILLSENT,			// SIGKILL sent to job
	CRON_DEAD				// Job is dead
} CronJobState;

// Job's "run" (when to restart) mode
typedef enum 
{ 
	CRON_WAIT_FOR_EXIT,		// Timing from job's exit
	CRON_PERIODIC, 			// Run it periodically
	CRON_ONESHOT,			// "One-shot" timer
	CRON_KILLED,			// Job has been killed & don't restart it
	CRON_ILLEGAL			// Illegal mode
} CronJobMode;

// Notification events..
typedef enum {
	CONDOR_CRON_JOB_START,
	CONDOR_CRON_JOB_DIED,
} CondorCronEvent;

// Cronjob event
class CronJobBase;
typedef int     (Service::*CronEventHandler)(CronJobBase*,CondorCronEvent);

// Enable pipe I/O debugging
#define CRONJOB_PIPEIO_DEBUG	0

// Define a Condor 'Cron' job
class CronJobBase : public Service
{
  public:
	CronJobBase( const char *mgrName, const char *jobName );
	virtual ~CronJobBase( );

	// Finish initialization
	virtual int Initialize( void );

	// Manipulate the job
	const char *GetName( void ) { return m_name.Value(); };
	const char *GetPrefix( void ) { return m_prefix.Value(); };
	const char *GetPath( void ) { return m_path.Value(); };
	//const char *GetArgs( void ) { return args.Value(); };
	const char *GetCwd( void ) { return m_cwd.Value(); };
	unsigned GetPeriod( void ) { return m_period; };

	// State information
	CronJobState GetState( void ) { return m_state; };
	bool IsRunning( void ) 
		{ return ( CRON_RUNNING == m_state ? true : false ); };
	bool IsIdle( void )
		{ return ( CRON_IDLE == m_state ? true : false ); };
	bool IsDead( void ) 
		{ return ( CRON_DEAD == m_state ? true : false ); };
	bool IsAlive( void ) 
		{ return ( (CRON_IDLE == m_state)||(CRON_DEAD == m_state)
				   ? false : true ); };

	int Reconfig( void );

	int SetConfigVal( const char *path );
	int SetName( const char *name );
	int SetPrefix( const char *prefix );
	int SetPath( const char *path );	
	int SetArgs( ArgList const &new_args );
	int SetEnv( char const * const *env );
	int AddEnv( char const * const *env );
	int SetCwd( const char *cwd );
	int SetPeriod( CronJobMode mode, unsigned period );
	int SetKill( bool );
	int SetReconfig( bool );
	int AddPath( const char *path );	
	int AddArgs( ArgList const &new_args );

	virtual int KillJob( bool );

	// Marking operations
	void Mark( void ) { m_marked = true; };
	void ClearMark( void ) { m_marked = false; };
	bool IsMarked( void ) { return m_marked; };

	int ProcessOutputQueue( void );
	virtual int ProcessOutput( const char *line ) = 0;

	int SetEventHandler( CronEventHandler handler, Service *s );

  protected:
	MyString		 m_configValProg;	// Path to _config_val

  private:
	MyString		 m_name;			// Logical name of the job
	MyString		 m_prefix;			// Publishing prefix
	MyString		 m_path;			// Path to the executable
	ArgList          m_args;			// Arguments to pass it
	Env              m_env;				// Environment variables
	MyString		 m_cwd;				// Process's initial CWD
	unsigned		 m_period;			// The configured period
	int				 m_runTimer;		// It's DaemonCore "run" timerID
	CronJobMode		 m_mode;			// Is is a periodic
	CronJobState	 m_state;			// Is is currently running?
	int				 m_pid;				// The process's PID
	int				 m_stdOut;			// Process's stdout file descriptor
	int				 m_stdErr;			// Process's stderr file descriptor
	int				 m_childFds[3];		// Child process FDs
	int				 m_reaperId;		// ID Of the child reaper
	CronJobOut		*m_stdOutBuf;		// Buffer for stdout
	CronJobErr		*m_stdErrBuf;		// Buffer for stderr
	bool			 m_marked;			// Is this one marked?
	int				 m_killTimer;		// Make sure it dies
	int				 m_numOutputs;		// # output blocks have we processed?

	// Event handler stuff
	CronEventHandler m_eventHandler;	// Handle cron events
	Service			*m_eventService;	// Associated service

	// Options
	bool			 m_optKill;			// Kill the job if before next run?
	bool			 m_optReconfig;		// Send the job a HUP for reconfig
	bool			 m_optIdle;			// Only run when idle

	// Private methods; these can be replaced
	virtual int Schedule( void );
	virtual int RunJob( void );
	virtual int StartJob( void );
	virtual void KillHandler( void );
	virtual int StdoutHandler( int pipe );
	virtual int StderrHandler( int pipe );
	virtual int Reaper( int exitPid, int exitStatus );
	virtual int RunProcess( void );

	// No reason to replace these
	int OpenFds( void );
	int TodoRead( int, int );
	void CleanAll( void );
	void CleanFile( FILE **file );
	void CleanFd( int *fd );
	const char *StateString( void );
	const char *StateString( CronJobState state );

	// Timer maintainence
	int SetTimer( unsigned first, unsigned seconds );
	int KillTimer( unsigned seconds );

	// Debugging
# if CRONJOB_PIPEIO_DEBUG
	char	*TodoBuffer;
	int		 TodoBufSize;
	int		 TodoBufWrap;
	int		 TodoBufOffset;
	int		 TodoWriteNum;
	public:
	void	 TodoWrite( void );
# endif
};

#endif /* _CONDOR_CRONJOB_H */
