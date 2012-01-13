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
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_io.h"
#include "get_daemon_name.h"
#include "internet.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "condor_version.h"
#include "condor_ver_info.h"
#include "string_list.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "dc_collector.h"
#include "condor_distribution.h"
#include "CondorError.h"
#include "str_isxxx.h"
#include "enum_utils.h"
#include "tool_core.h"

char 	*actionReason = NULL;
char    *holdReasonSubCode = NULL;
JobAction mode;
bool All = false;
bool ConstraintArg = false;
bool UserJobIdArg = false;
bool had_error = false;

DCSchedd* schedd = NULL;

StringList* job_ids = NULL;

	// Prototypes of local interest
void addConstraint(const char *);
void procArg(const char*);
void usage(int iExitCode=1);
void handleAll();
void handleConstraints( void );
ClassAd* doWorkByList( StringList* ids, CondorError * errstack );
void printNewMessages( ClassAd* result_ad, StringList* ids );
bool mayUserForceRm( );

bool has_constraint;

MyString global_constraint;
StringList global_id_list;

const char* 
actionWord( JobAction action, bool past )
{
	switch( action ) {
	case JA_RELEASE_JOBS:
		return past ? "released" : "release";
		break;

	case JA_HOLD_JOBS:
		return past ? "held" : "hold";
		break;
	
	case JA_SUSPEND_JOBS:
		return past ? "suspended" : "suspend";
		break;
		
	case JA_CONTINUE_JOBS:
		return past ? "continued" : "continue";
		break;

	case JA_REMOVE_JOBS:
	case JA_REMOVE_X_JOBS:
		return past ? "removed" : "remove";
		break;

	case JA_VACATE_JOBS:
		return past ? "vacated" : "vacate";
		break;

	case JA_VACATE_FAST_JOBS:
		return past ? "fast-vacated" : "fast-vacate";
		break;

	default:
		fprintf( stderr, "ERROR: Unknown action: %d\n", action );
		exit( 1 );
		break;
	}
	return NULL;
}


void
usage(int iExitCode)
{
	char word[32];
	sprintf( word, "%s", getJobActionString(mode) );
	fprintf( stderr, "Usage: %s [options] [constraints]\n", toolname );
	fprintf( stderr, " where [options] is zero or more of:\n" );
	fprintf( stderr, "  -help               Display this message and exit\n" );
	fprintf( stderr, "  -version            Display version information and exit\n" );

// i'm not sure we want -debug documented.  if we change our minds, we
// should just uncomment the next line
//	fprintf( stderr, "  -debug              Display debugging information while running\n" );

	fprintf( stderr, "  -name schedd_name   Connect to the given schedd\n" );
	fprintf( stderr, "  -pool hostname      Use the given central manager to find daemons\n" );
	fprintf( stderr, "  -addr <ip:port>     Connect directly to the given \"sinful string\"\n" );
	if( mode == JA_REMOVE_JOBS || mode == JA_REMOVE_X_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given RemoveReason\n");
	} else if( mode == JA_RELEASE_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given ReleaseReason\n");
	} else if( mode == JA_HOLD_JOBS ) {
		fprintf( stderr, "  -reason reason      Use the given HoldReason\n");
		fprintf( stderr, "  -subcode number     Set HoldReasonSubCode\n");
	}

	if( mode == JA_REMOVE_JOBS || mode == JA_REMOVE_X_JOBS ) {
		fprintf( stderr,
				     "  -forcex             Force the immediate local removal of jobs in the X state\n"
		         "                      (only affects jobs already being removed)\n" );
	}
	if( mode == JA_VACATE_JOBS || mode == JA_VACATE_FAST_JOBS ) {
		fprintf( stderr,
				     "  -fast               Use a fast vacate (hardkill)\n" );
	}
	fprintf( stderr, " and where [constraints] is one of:\n" );
	fprintf( stderr, "  cluster.proc        %s the given job\n", word );
	fprintf( stderr, "  cluster             %s the given cluster of jobs\n", word );
	fprintf( stderr, "  user                %s all jobs owned by user\n", word );
	fprintf( stderr, "  -constraint expr    %s all jobs matching the boolean expression\n", word );
	fprintf( stderr, "  -all                %s all jobs "
			 "(cannot be used with other constraints)\n", word );
	tool_exit( iExitCode );
}


int
main( int argc, char *argv[] )
{
	char	**args = (char **)malloc(sizeof(char *)*(argc - 1)); // args 
	int					nArgs = 0;				// number of args 
	int					i;
	const char*	cmd_str;
	DCCollector* pool = NULL;
	char* scheddName = NULL;
	param_functions *p_funcs = NULL;

		// Initialize our global variables
	has_constraint = false;

	myDistro->Init( argc, argv );
	set_usage(&usage);
	tool_parse_command_line(argc, argv);

	cmd_str = strchr( toolname, '_');

	// we match modes based on characters after the '_'. This means
	// 'condor_hold.exe' or 'condor_hold_wrapped' are all legal argv[0]'s
	// for condor_hold.

	if (cmd_str && strncasecmp( cmd_str, "_hold", strlen("_hold") ) == MATCH) { 

		mode = JA_HOLD_JOBS;

	} else if ( cmd_str && 
			strncasecmp( cmd_str, "_release", strlen("_release") ) == MATCH ) {

		mode = JA_RELEASE_JOBS;

	} else if ( cmd_str && 
			strncasecmp( cmd_str, "_suspend", strlen("_suspend") ) == MATCH ) {

		mode = JA_SUSPEND_JOBS;

	} else if ( cmd_str && 
			strncasecmp( cmd_str, "_continue", strlen("_continue") ) == MATCH ) {

		mode = JA_CONTINUE_JOBS;

	}else if ( cmd_str && 
			strncasecmp( cmd_str, "_rm", strlen("_rm") ) == MATCH ) {

		mode = JA_REMOVE_JOBS;

	} else if( cmd_str && ! strncasecmp(cmd_str, "_vacate_job",
									strlen("_vacate_job")) ) {  

		mode = JA_VACATE_JOBS;

	} else {
		// don't know what mode we're using, so bail.
		fprintf( stderr, "Unrecognized command name, \"%s\"\n", toolname ); 
		usage();
	}

	config();


	if( argc < 2 ) {
			// We got no indication of what to act on
		fprintf( stderr, "You did not specify any jobs\n" ); 
		usage();
	}

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for(i = 1; i < argc; i++)
	{
		if(match_prefix(argv[i], "-debug"))
			continue;
		if(match_prefix(argv[i], "-pool")
			|| match_prefix(argv[i], "-name")
			|| match_prefix(argv[i], "-addr"))
		{
			i++;
			continue;
		}

		if(match_prefix(argv[i], "-constraint")) {
			args[nArgs] = argv[i];
			nArgs++;
			i++;
			if( ! argv[i] ) {
				option_needs_arg("-constraint");
			}				
			args[nArgs] = argv[i];
			nArgs++;
			ConstraintArg = true;
		}
		else if(match_prefix(argv[i], "-all"))
			All = true;
		else if(match_prefix(argv[i], "-reason")) {
			i++;
			if( ! argv[i] ) {
				option_needs_arg("-reason");
			}
			actionReason = strdup(argv[i]);
			if( ! actionReason ) {
				fprintf( stderr, "Out of memory!\n" );
				tool_exit(1);
			}
		} else if (match_prefix(argv[i], "-subcode")) {
			i++;
			if( ! argv[i] ) {
				option_needs_arg("-subcode");
			}		
			char *end = NULL;
			long code = strtol(argv[i],&end,10);
			if( code == LONG_MIN || !end || *end || end==argv[i] ) {
				fprintf( stderr, "Invalid -subcode %s!\n", argv[i] );
				tool_exit(1);
			}
			holdReasonSubCode = strdup(argv[i]);
			ASSERT( holdReasonSubCode );
		} else if (match_prefix(argv[i], "-forcex")) {
				if( mode == JA_REMOVE_JOBS ) {
					mode = JA_REMOVE_X_JOBS;
				} else {
                    fprintf( stderr, 
                             "-forcex is only valid with condor_rm\n" );
					usage();
				}
		} else if (match_prefix(argv[i], "-fast")) {
			if( mode == JA_VACATE_JOBS ) {
				mode = JA_VACATE_FAST_JOBS;
			} else {
				fprintf( stderr, 
					"-fast is only valid with condor_vacate_job\n" );
					usage();
			}
		} else  if(match_prefix(argv[i], "-")) {
			fprintf( stderr, "Unrecognized option: %s\n", argv[i] ); 
			usage();
		} else {
			if( All ) {
					// If -all is set, there should be no other
					// constraint arguments.
				usage();
			}
			args[nArgs] = argv[i];
			nArgs++;
			UserJobIdArg = true;
		}
	}

	if(name_arg)
	{
		if(!(scheddName = get_daemon_name(name_arg)))
		{
			fprintf( stderr, "%s: unknown host %s\n", 
				toolname, get_host_part(name_arg) );
			tool_exit(1);
		}
	}

	if(pool_arg)
	{
		if( pool ) {
			delete pool;
		}
		pool = new DCCollector( pool_arg );
		if( ! pool->addr() ) {
			fprintf( stderr, "%s: %s\n", toolname, pool->error() );
			exit(1);
		}
	}

	if( ! (All || nArgs) ) {
			// We got no indication of what to act on
		fprintf( stderr, "You did not specify any jobs\n" ); 
		usage();
	}

	if ( ConstraintArg && UserJobIdArg ) {
		fprintf( stderr, "You can't use both -constraint and usernames or job ids\n" );
		usage();
	}

		// Pick the default reason if the user didn't specify one
	if( actionReason == NULL ) {
		switch( mode ) {
		case JA_RELEASE_JOBS:
			actionReason = strdup("via condor_release");
			break;
		case JA_REMOVE_X_JOBS:
			actionReason = strdup("via condor_rm -forcex");
			break;
		case JA_REMOVE_JOBS:
			actionReason = strdup("via condor_rm");
			break;
		case JA_HOLD_JOBS:
			actionReason = strdup("via condor_hold");
			break;
		case JA_SUSPEND_JOBS:
			actionReason = strdup("via condor_suspend");
			break;
		case JA_CONTINUE_JOBS:
			actionReason = strdup("via condor_continue");
			break;
		default:
			actionReason = NULL;
		}
	}

		// We're done parsing args, now make sure we know how to
		// contact the schedd. 
	if( ! addr_arg ) {
			// This will always do the right thing, even if either or
			// both of scheddName or pool are NULL.
		schedd = new DCSchedd( scheddName, pool ? pool->addr() : NULL );
	} else {
		schedd = new DCSchedd( addr_arg );
	}
	if( ! schedd->locate() ) {
		fprintf( stderr, "%s: %s\n", toolname, schedd->error() ); 
		exit( 1 );
	}

		// Special case for condor_rm -forcex: a configuration
		// setting can disable this functionality.  The real
		// validation is done in the schedd, but we can catch
		// the most common cases here and give a useful error
		// message.
	if(mode == JA_REMOVE_X_JOBS) {
		if( mayUserForceRm() == false) {
			fprintf( stderr, "Remove aborted. condor_rm -forcex has been disabled by the administrator.\n" );
			exit( 1 );
		}
	}

		// Process the args so we do the work.
	if( All ) {
		handleAll();
	} else {
		for(i = 0; i < nArgs; i++) {
			if( match_prefix( args[i], "-constraint" ) ) {
				i++;
				addConstraint( args[i] );
			} else {
				procArg(args[i]);
			}
		}
	}

		// Deal with all the -constraint constraints
	handleConstraints();

		// Finally, do the actual work for all our args which weren't
		// constraints...
	if( job_ids ) {
		CondorError errstack;
		ClassAd* result_ad = doWorkByList( job_ids, &errstack );
		if (had_error) {
			fprintf( stderr, "%s\n", errstack.getFullText(true) );
		}
		printNewMessages( result_ad, job_ids );
		delete( result_ad );
	}

		// If releasing jobs, and no errors happened, do a 
		// reschedule command now.
	if ( mode == JA_RELEASE_JOBS && had_error == false ) {
		Daemon  my_schedd(DT_SCHEDD, NULL, NULL);
		CondorError errstack;
		if (!my_schedd.sendCommand(RESCHEDULE, Stream::safe_sock, 0, &errstack)) {
			fprintf( stderr, "%s\n", errstack.getFullText(true) );
		}
	}

	return had_error;
}



// For now, just return true if the constraint worked on at least
// one job, false if not.  Someday, we can fix up the tool to take
// advantage of all the slick info the schedd gives us back about this
// request.  
bool
doWorkByConstraint( const char* constraint, CondorError * errstack )
{
	ClassAd* ad = 0;
	int total_jobs = -1;
	bool rval = true;
	switch( mode ) {
	case JA_RELEASE_JOBS:
		ad = schedd->releaseJobs( constraint, actionReason, errstack );
		break;
	case JA_REMOVE_X_JOBS:
		ad = schedd->removeXJobs( constraint, actionReason, errstack );
		break;
	case JA_VACATE_JOBS:
		ad = schedd->vacateJobs( constraint, VACATE_GRACEFUL, errstack );
		break;
	case JA_VACATE_FAST_JOBS:
		ad = schedd->vacateJobs( constraint, VACATE_FAST, errstack );
		break;
	case JA_REMOVE_JOBS:
		ad = schedd->removeJobs( constraint, actionReason, errstack );
		break;
	case JA_HOLD_JOBS:
		ad = schedd->holdJobs( constraint, actionReason, holdReasonSubCode, errstack );
		break;
	case JA_SUSPEND_JOBS:
		ad = schedd->suspendJobs( constraint, actionReason, errstack );
		break;
	case JA_CONTINUE_JOBS:
		ad = schedd->continueJobs( constraint, actionReason, errstack );
		break;
	default:
		EXCEPT( "impossible: unknown mode in doWorkByConstraint" );
	}
	if( ! ad ) {
		had_error = true;
		rval = false;
	} else {
		int result = FALSE;
		if( !ad->LookupInteger(ATTR_ACTION_RESULT, result) ) {
			had_error = true;
			rval = false;
		}
		else if( !result ) {
			// There were no ads acted upon, but that doesn't
			// mean there was an error.  It's possible the schedd
			// had no jobs
		        if( !ad->LookupInteger(ATTR_TOTAL_JOB_ADS, total_jobs) || total_jobs > 0 ) {
				had_error = true;
			} else {
				// There were no jobs in the queue, so add a
				// more meaningful error message
				errstack->push("condor_rm", 0, "There are no jobs in the queue");
			}
			rval = false;
		}
	}
	return rval;
}


ClassAd*
doWorkByList( StringList* ids, CondorError *errstack )
{
	ClassAd* rval = 0;
	switch( mode ) {
	case JA_RELEASE_JOBS:
		rval = schedd->releaseJobs( ids, actionReason, errstack );
		break;
	case JA_REMOVE_X_JOBS:
		rval = schedd->removeXJobs( ids, actionReason, errstack );
		break;
	case JA_VACATE_JOBS:
		rval = schedd->vacateJobs( ids, VACATE_GRACEFUL, errstack );
		break;
	case JA_VACATE_FAST_JOBS:
		rval = schedd->vacateJobs( ids, VACATE_FAST, errstack );
		break;
	case JA_REMOVE_JOBS:
		rval = schedd->removeJobs( ids, actionReason, errstack );
		break;
	case JA_HOLD_JOBS:
		rval = schedd->holdJobs( ids, actionReason, holdReasonSubCode, errstack );
		break;
	case JA_SUSPEND_JOBS:
		rval = schedd->suspendJobs( ids, actionReason, errstack );
		break;
	case JA_CONTINUE_JOBS:
		rval = schedd->continueJobs( ids, actionReason, errstack );
		break;
	default:
		EXCEPT( "impossible: unknown mode in doWorkByList" );
	}
	if( ! rval ) {
		had_error = true;
	} else {
		int result = FALSE;
		if( !rval->LookupInteger(ATTR_ACTION_RESULT, result) || !result ) {
			had_error = true;
		}
	}
	return rval;
}


void
procArg(const char* arg)
{
	int		c, p;								// cluster/proc #
	char*	tmp;

	MyString constraint;

	if( str_isint(arg) || str_isreal(arg,true) )
	// process by cluster/proc #
	{
		c = strtol(arg, &tmp, 10);
		if(c <= 0)
		{
			fprintf(stderr, "Invalid cluster # from %s.\n", arg);
			had_error = true;
			return;
		}
		if(*tmp == '\0')
		// delete the cluster
		{
			CondorError errstack;
			constraint.sprintf( "%s == %d", ATTR_CLUSTER_ID, c );
			if( doWorkByConstraint(constraint.Value(), &errstack) ) {
				fprintf( stdout, 
						 "Cluster %d %s.\n", c,
						 (mode == JA_REMOVE_JOBS) ?
						 "has been marked for removal" :
						 (mode == JA_REMOVE_X_JOBS) ?
						 "has been removed locally (remote state unknown)" :
						 actionWord(mode,true) );
			} else {
				fprintf( stderr, "%s\n", errstack.getFullText(true) );
				if (had_error)
				{
					fprintf( stderr, 
						 "Couldn't find/%s all jobs in cluster %d.\n",
						 actionWord(mode,false), c );
				}
			}
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf( stderr, "Invalid proc # from %s.\n", arg);
				had_error = true;
				return;
			}
			if(*tmp == '\0')
			// process a proc
			{
				if( ! job_ids ) {
					job_ids = new StringList();
				}
				job_ids->append( arg );
				return;
			}
		}
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
		return;
	}
	// process by user name
	else {
		CondorError errstack;
		constraint.sprintf("%s == \"%s\"", ATTR_OWNER, arg );
		if( doWorkByConstraint(constraint.Value(), &errstack) ) {
			fprintf( stdout, "User %s's job(s) %s.\n", arg,
					 (mode == JA_REMOVE_JOBS) ?
					 "have been marked for removal" :
					 (mode == JA_REMOVE_X_JOBS) ?
					 "have been removed locally (remote state unknown)" :
					 actionWord(mode,true) );
		} else {
			fprintf( stderr, "%s\n", errstack.getFullText(true) );
			if (had_error)
			{
				fprintf( stderr, 
					 "Couldn't find/%s all of user %s's job(s).\n",
					 actionWord(mode,false), arg );
			}
		}
	}
}


void
addConstraint( const char *constraint )
{
	static bool has_clause = false;
	if( has_clause ) {
		global_constraint += " && (";
	} else {
		global_constraint += "(";
	}
	global_constraint += constraint;
	global_constraint += ")";

	has_clause = true;
	has_constraint = true;
}

bool
mayUserForceRm( )
{
	const char * PARAM_ALLOW_FORCE_RM = "ALLOW_FORCE_RM";
	char* tmp = param( PARAM_ALLOW_FORCE_RM );
	if( tmp == NULL) {
		// Not present.  Assume TRUE (old behavior).
		return true;
	}

	ClassAd tmpAd;
	const char * TESTNAME = "test";
	if(tmpAd.AssignExpr(TESTNAME, tmp) == FALSE) {
		// Error parsing, most likely.  Warn and assume TRUE.
		fprintf(stderr, "The configuration setting %s may be invalid.  Treating as TRUE.\n", PARAM_ALLOW_FORCE_RM);
		free(tmp);
		return true;
	}

	free(tmp);

	int is_okay = 0;
	if(tmpAd.EvalBool(TESTNAME, 0, is_okay)) {
		return is_okay;
	} else {
		// Something went wrong.  May be undefined because
		// we need a job classad.  Assume TRUE.
		return true;
	}
}


void
handleAll()
{
	char constraint[128];
	sprintf( constraint, "%s >= 0", ATTR_CLUSTER_ID );

	CondorError errstack;
	if( doWorkByConstraint(constraint, &errstack) ) {
		fprintf( stdout, "All jobs %s.\n",
				 (mode == JA_REMOVE_JOBS) ?
				 "marked for removal" :
				 (mode == JA_REMOVE_X_JOBS) ?
				 "removed locally (remote state unknown)" :
				 actionWord(mode,true) );
	} else {
		fprintf( stderr, "%s\n", errstack.getFullText(true) );
		if (had_error)
		{
			fprintf( stderr, "Could not %s all jobs.\n",
				 actionWord(mode,false) );
		}
	}
}


void
handleConstraints( void )
{
	if( ! has_constraint ) {
		return;
	}
	const char* tmp = global_constraint.Value();

	CondorError errstack;
	if( doWorkByConstraint(tmp, &errstack) ) {
		fprintf( stdout, "Jobs matching constraint %s %s\n", tmp,
				 (mode == JA_REMOVE_JOBS) ?
				 "have been marked for removal" :
				 (mode == JA_REMOVE_X_JOBS) ?
				 "have been removed locally (remote state unknown)" :
				 actionWord(mode,true) );

	} else {
		fprintf( stderr, "%s\n", errstack.getFullText(true) );
		if (had_error)
		{
			fprintf( stderr, 
				 "Couldn't find/%s all jobs matching constraint %s\n",
				 actionWord(mode,false), tmp );
		}
	}
}


void
printNewMessages( ClassAd* result_ad, StringList* ids )
{
	char* tmp;
	char* msg;
	PROC_ID job_id;
	bool rval;

	JobActionResults results;
	results.readResults( result_ad );

	ids->rewind();
	while( (tmp = ids->next()) ) {
		job_id = getProcByString( tmp );
		rval = results.getResultString( job_id, &msg );
		if( rval ) {
			fprintf( stdout, "%s\n", msg );
		} else {
			fprintf( stderr, "%s\n", msg );
		}
		free( msg );
	}
}
