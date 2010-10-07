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
#include "condor_config.h"
#include "simplelist.h"
#include "condor_cron.h"
#include "condor_cronmgr.h"
#include "condor_string.h"


// Basic constructor
CronMgrBase::CronMgrBase( const char *name )
{
	dprintf( D_FULLDEBUG, "CronMgr: Constructing '%s'\n", name );

	// Make sure that SetName doesn't try to free Name or ParamBase...
	Name = NULL;
	ParamBase = NULL;
	configValProg = NULL;

	// Set 'em
	SetName( name, name, "_cron" );
}

// Basic destructor
CronMgrBase::~CronMgrBase( )
{
	// Kill all running jobs
	Cron.DeleteAll( );

	// Free up name, etc. buffers
	if ( NULL != Name ) {
		free( Name );
	}
	if ( NULL != ParamBase ) {
		free( ParamBase );
	}

	// Log our death
	dprintf( D_FULLDEBUG, "CronMgr: bye\n" );
}

// Handle initialization
int
CronMgrBase::Initialize( void )
{
	return DoConfig( true );
}

// Set new name..
int
CronMgrBase::SetName( const char *newName, 
					  const char *newParamBase,
					  const char *newParamExt )
{
	int		retval = 0;

	// Debug...
	dprintf( D_FULLDEBUG, "CronMgr: Setting name to '%s'\n", newName );
	if ( NULL != Name ) {
		free( (char *) Name );
	}

	// Copy it out..
	Name = strdup( newName );
	if ( NULL == Name ) {
		retval = -1;
	}

	// Set the parameter base name
	if ( NULL != newParamBase ) {
		retval = SetParamBase( newParamBase, newParamExt );
	}

	// Done
	return retval;
}

// Set new name..
int CronMgrBase::SetParamBase( const char *newParamBase,
							   const char *newParamExt )
{
	dprintf( D_FULLDEBUG, "CronMgr: Setting parameter base to '%s'\n",
			 newParamBase );

	// Free the old one..
	if ( NULL != ParamBase ) {
		free( ParamBase );
	}

	// Default?
	if ( NULL == newParamBase ) {
		newParamBase = "CRON";
	}
	if ( NULL == newParamExt ) {
		newParamExt = "";
	}

	// Calc length & allocate
	int		len = strlen( newParamBase ) + strlen( newParamExt ) + 1;
	char *tmp = (char * ) malloc( len );
	if ( NULL == tmp ) {
		return -1;
	}

	// Copy it out..
	strcpy( tmp, newParamBase );
	strcat( tmp, newParamExt );
	ParamBase = tmp;

	return 0;
}

// Kill all running jobs
int
CronMgrBase::KillAll( bool force)
{
	// Log our death
	dprintf( D_FULLDEBUG, "CronMgr: Killing all jobs\n" );

	// Kill all running jobs
	return Cron.KillAll( force );
}

// Check: Are we ready to shutdown?
bool
CronMgrBase::IsAllIdle( void )
{
	int		AliveJobs = Cron.NumAliveJobs( );

	dprintf( D_FULLDEBUG, "CronMgr: %d jobs alive\n", AliveJobs );
	return AliveJobs ? false : true;
}

// Handle Reconfig
int
CronMgrBase::Reconfig( void )
{
	return DoConfig( false );
}

// Handle configuration
int
CronMgrBase::DoConfig( bool initial )
{
	char *paramBuf;

	// Is the config val program specified?
	if( configValProg ) {
		free( configValProg );
	}
	configValProg = GetParam( "CONFIG_VAL" );

	// Clear all marks
	Cron.ClearAllMarks( );

	// Look for _JOBLIST
	paramBuf = GetParam( "JOBLIST" );
	if ( paramBuf != NULL ) {
		ParseJobList( paramBuf );
		free( paramBuf );
	}

	// Delete all jobs that didn't get marked
	Cron.DeleteUnmarked( );

	// And, initialize all jobs (they ignore it if already initialized)
	Cron.InitializeAll( );

	// Find our environment variable, if it exits..
	dprintf( D_FULLDEBUG, "CronMgr: Doing config (%s)\n",
			 initial ? "initial" : "reconfig" );

	// Reconfigure all running jobs
	if ( ! initial ) {
		Cron.Reconfig( );
	}

	// Done
	return 0;
}

// Read a parameter
char *
CronMgrBase::GetParam( const char *paramName, 
					   const char *paramName2 )
{

	// Defaults...
	if ( NULL == paramName2 ) {
		paramName2 = "";
	}

	// Build the name of the parameter to read
	int len = ( strlen( ParamBase ) + 
				strlen( paramName ) +
				1 +
				strlen( paramName2 ) +
				1 );
	char *nameBuf = (char *) malloc( len );
	if ( NULL == nameBuf ) {
		return NULL;
	}
	strcpy( nameBuf, ParamBase );
	strcat( nameBuf, "_" );
	strcat( nameBuf, paramName );
	strcat( nameBuf, paramName2 );

	// Now, go read the actual parameter
	char *paramBuf = param( nameBuf );
	free( nameBuf );

	// Done
	return paramBuf;
}

// Parse the "Job List"
int
CronMgrBase::ParseJobList( const char *jobListString )
{
	// Debug
	dprintf( D_JOB, "CronMgr: Job string is '%s'\n", jobListString );

	// Break it into a string list
	StringList	jobList( jobListString );
	jobList.rewind( );

	// Parse out the job names
	const char *jobName;
	while( ( jobName = jobList.next()) != NULL ) {
		dprintf( D_JOB, "CronMgr: Job name is '%s'\n", jobName );

		// Parse out the prefix
		char *paramPrefix_cstr     = GetParam( jobName, "_PREFIX" );
		char *paramExecutable_cstr = GetParam( jobName, "_EXECUTABLE" );
		char *paramPeriod_cstr     = GetParam( jobName, "_PERIOD" );
		char *paramMode_cstr       = GetParam( jobName, "_MODE" );
		char *paramReconfig_cstr   = GetParam( jobName, "_RECONFIG" );
		char *paramKill_cstr       = GetParam( jobName, "_KILL" );
		char *paramOptions_cstr    = GetParam( jobName, "_OPTIONS" );
		char *paramArgs_cstr       = GetParam( jobName, "_ARGS" );
		char *paramEnv_cstr        = GetParam( jobName, "_ENV" );
		char *paramCwd_cstr        = GetParam( jobName, "_CWD" );

		MyString paramPrefix = paramPrefix_cstr; free(paramPrefix_cstr);
		MyString paramExecutable = paramExecutable_cstr; free(paramExecutable_cstr);
		MyString paramPeriod = paramPeriod_cstr; free(paramPeriod_cstr);
		MyString paramMode = paramMode_cstr; free(paramMode_cstr);
		MyString paramReconfig = paramReconfig_cstr; free(paramReconfig_cstr);
		MyString paramKill = paramKill_cstr; free(paramKill_cstr);
		MyString paramOptions = paramOptions_cstr; free(paramOptions_cstr);
		MyString paramArgs = paramArgs_cstr; free(paramArgs_cstr);
		MyString paramEnv = paramEnv_cstr; free(paramEnv_cstr);
		MyString paramCwd = paramCwd_cstr; free(paramCwd_cstr);

		bool jobOk = true;

		// Some quick sanity checks
		if ( paramExecutable.IsEmpty() ) {
			dprintf( D_ALWAYS, 
					 "CronMgr: No path found for job '%s'; skipping\n",
					 jobName );
			jobOk = false;
		}

		// Pull out the period
		unsigned	jobPeriod = 0;
		if ( paramPeriod.IsEmpty() ) {
			dprintf( D_ALWAYS,
					 "CronMgr: No job period found for job '%s': skipping\n",
					 jobName );
			jobOk = false;
		} else {
			char	modifier = 'S';
			int		num = sscanf( paramPeriod.Value(), "%d%c",
								  &jobPeriod, &modifier );
			if ( num < 1 ) {
				dprintf( D_ALWAYS,
						 "CronMgr: Invalid job period found "
						 "for job '%s' (%s): skipping\n",
						 jobName, paramPeriod.Value() );
				jobOk = false;
			} else {
				// Check the modifier
				modifier = toupper( modifier );
				if ( ( 'S' == modifier ) ) {	// Seconds
					// Do nothing
				} else if ( 'M' == modifier ) {
					jobPeriod *= 60;
				} else if ( 'H' == modifier ) {
					jobPeriod *= ( 60 * 60 );
				} else {
					dprintf( D_ALWAYS,
							 "CronMgr: Invalid period modifier "
							 "'%c' for job %s (%s)\n",
							 modifier, jobName, paramPeriod.Value() );
					jobOk = false;
				}
			}
		}

		// Options
		CronJobMode	jobMode = CRON_PERIODIC;
		bool		jobReconfig = false;
		bool		jobKillMode = false;

		// Parse the job mode
		if ( ! paramMode.IsEmpty() ) {
			if ( ! strcasecmp( paramMode.Value(), "Periodic" ) ) {
				jobMode =  CRON_PERIODIC;
			} else if ( ! strcasecmp( paramMode.Value(), "WaitForExit" ) ) {
				jobMode = CRON_WAIT_FOR_EXIT;
			} else if ( ! strcasecmp( paramMode.Value(), "OneShot" ) ) {
				jobMode = CRON_ONESHOT;
			} else {
				dprintf( D_ALWAYS,
						 "CronMgr: Unknown job mode for '%s'\n",
						 jobName );
			}
		}
		if ( ! paramReconfig.IsEmpty() ) {
			if ( ! strcasecmp( paramReconfig.Value(), "True" ) ) {
				jobReconfig = true;
			} else {
				jobReconfig = false;
			}
		}
		if ( ! paramKill.IsEmpty() ) {
			if ( ! strcasecmp( paramKill.Value(), "True" ) ) {
				jobKillMode = true;
			} else {
				jobKillMode = false;
			}
		}

		// Parse the option string
		if ( ! paramOptions.IsEmpty() ) {
			StringList	list( paramOptions.Value(), " :," );
			list.rewind( );

			const char *option;
			while( ( option = list.next()) != NULL ) {

				// And, parse it
				if ( !strcasecmp( option, "kill" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': Kill option ok\n",
							 jobName );
					jobKillMode = true;
				} else if ( !strcasecmp( option, "nokill" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': NoKill option ok\n",
							 jobName );
					jobKillMode = false;
				} else if ( !strcasecmp( option, "reconfig" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': Reconfig option ok\n",
							 jobName );
					jobReconfig = true;
				} else if ( !strcasecmp( option, "noreconfig" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': NoReconfig option ok\n",
							 jobName );
					jobReconfig = false;
				} else if ( !strcasecmp( option, "WaitForExit" ) ) {
					dprintf( D_FULLDEBUG,
							 "CronMgr: '%s': WaitForExit option ok\n",
							 jobName );
					jobMode = CRON_WAIT_FOR_EXIT;
				} else {
					dprintf( D_ALWAYS,
							 "CronMgr: Job '%s': "
							 "Ignoring unknown option '%s'\n",
							 jobName, option );
				}
			}
		}

		// Are there arguments for it?
		ArgList args;
		MyString args_errors;

		// Force the first arg to be the "Job Name"..
		args.AppendArg(jobName);

		if( !args.AppendArgsV1RawOrV2Quoted( paramArgs.Value(),
											 &args_errors ) ) {
			dprintf( D_ALWAYS,
					 "CronMgr: Job '%s': "
					 "Failed to parse arguments: '%s'\n",
					 jobName, args_errors.Value());
			jobOk = false;
		}

		// Parse the environment.
		Env envobj;
		MyString env_error_msg;

		if( !envobj.MergeFromV1RawOrV2Quoted( paramEnv.Value(),
											  &env_error_msg ) ) {
			dprintf( D_ALWAYS,
					 "CronMgr: Job '%s': "
					 "Failed to parse environment: '%s'\n",
					 jobName, env_error_msg.Value());
			jobOk = false;
		}


		// Create the job & add it to the list (if it's new)
		CronJobBase *job = NULL;
		if ( jobOk ) {
			bool add_it = false;
			job = Cron.FindJob( jobName );
			if ( NULL == job ) {
				job = NewJob( jobName );
				add_it = true;

				// Ok?
				if ( NULL == job ) {
					dprintf( D_ALWAYS,
							 "Cron: Failed to allocate job object for '%s'\n",
							 jobName );
				}
			}

			// Put the job in the list
			if ( NULL != job ) {
				if ( add_it ) {
					if ( Cron.AddJob( jobName, job ) < 0 ) {
						dprintf( D_ALWAYS,
								 "CronMgr: Error creating job '%s'\n", 
								 jobName );
						delete job;
						job = NULL;
					}
				} else {
					dprintf( D_FULLDEBUG,
							 "CronMgr: Not adding duplicate job '%s' (OK)\n",
							 jobName );
				}
			}
		}

		// Now fill in the job details
		if ( NULL == job ) {
			dprintf( D_ALWAYS,
					 "Cron: Can't create job for '%s'\n",
					 jobName );
		} else {
			job->SetKill( jobKillMode );
			job->SetReconfig( jobReconfig );

			// And, set it's characteristics
			job->SetPath( paramExecutable.Value() );
			job->SetPrefix( paramPrefix.Value() );
			job->SetArgs( args );
			job->SetCwd( paramCwd.Value() );
			job->SetPeriod( jobMode, jobPeriod );
			job->SetConfigVal( configValProg );

			char **env_array = envobj.getStringArray();
			job->SetEnv( env_array );
			deleteStringArray(env_array);

			// Mark the job so that it doesn't get deleted (below)
			job->Mark( );
		}

		// Debug info
		dprintf( D_FULLDEBUG,
				 "CronMgr: Done processing job '%s'\n", jobName );

		// Job initialization is done by Cron::InitializeAll()
	}

	// All ok
	return 0;
}
