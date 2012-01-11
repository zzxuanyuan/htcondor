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
#include "condor_config.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_ver_info.h"
#include "dc_schedd.h"
#include "dc_transferd.h"
#include "condor_distribution.h"
#include "basename.h"
#include "internet.h"
#include "MyString.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_ftp.h"
#include "tool_core.h"

MyString global_constraint;
bool had_error = false;
DCSchedd* schedd = NULL;
bool All = false;

SandboxTransferMethod st_method = STM_USE_SCHEDD_ONLY;

void usage(int iExitCode=1);
void procArg(const char*);
void addConstraint(const char *);
void handleAll();

void usage(int iExitCode)
{
	fprintf( stderr, "Usage: %s [options] [constraints]\n", toolname );
	fprintf( stderr, " where [options] is zero or more of:\n" );
	fprintf( stderr, "  -help               Display this message and exit\n" );
	fprintf( stderr, "  -version            Display version information and exit\n" );

// i'm not sure we want -debug documented.  if we change our minds, we
// should just uncomment the next line
//	fprintf( stderr, "  -debug              Display debugging information while running\n" );

	fprintf( stderr, "  -name schedd_name   Connect to the given schedd\n" );
	fprintf( stderr, "  -pool hostname      Use the given central manager to find daemons\n" );
	fprintf( stderr, "  -address <ip:port>     Connect directly to the given \"sinful string\"\n" );
	fprintf( stderr, "  -stm <method>\t\thow to move a sandbox out of Condor\n" );
	fprintf( stderr, "               \t\tAvailable methods:\n\n" );
	fprintf( stderr, "               \t\t\tstm_use_schedd_only\n" );
	fprintf( stderr, "               \t\t\tstm_use_transferd\n\n" );
	fprintf( stderr, " and where [constraints] is one or more of:\n" );
	fprintf( stderr, "  cluster.proc        transfer data for the given job\n");
	fprintf( stderr, "  cluster             transfer data for the given cluster of jobs\n");
	fprintf( stderr, "  user                transfer data for all jobs owned by user\n" );
	fprintf( stderr, "  -constraint expr    transfer data for all jobs matching the boolean expression\n" );
	fprintf( stderr, "  -all                transfer data for all jobs "
			 "(cannot be used with other constraints)\n" );
	exit( iExitCode );
}


void
procArg(const char* arg)
{
	int		c, p;								// cluster/proc #
	char*	tmp;

	MyString constraint;

	if(isdigit(*arg))
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
			constraint.sprintf( "%s==%d", ATTR_CLUSTER_ID, c );
			addConstraint(constraint.Value());
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf( stderr, "Invalid proc # from %s.\n", arg);
				had_error = 1;
				return;
			}
			if(*tmp == '\0')
			// process a proc
			{
				constraint.sprintf( "(%s==%d && %s==%d)", 
					ATTR_CLUSTER_ID, c,
					ATTR_PROC_ID, p);
				addConstraint(constraint.Value());
				return;
			}
		}
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
		return;
	}
	else if(isalpha(*arg))
	// process by user name
	{
		constraint.sprintf( "%s == \"%s\"", ATTR_OWNER, arg );
		addConstraint(constraint.Value());
	} else {
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped\n", arg );
	}
}


void
addConstraint( const char *constraint )
{
	if ( global_constraint.Length() > 0 ) {
		global_constraint += " || (";
	} else {
		global_constraint += "(";
	}
	global_constraint += constraint;
	global_constraint += ")";
}


void
handleAll()
{
	char constraint[128];
	sprintf( constraint, "%s >= 0", ATTR_CLUSTER_ID );

	addConstraint(constraint);
}




int
main(int argc, char *argv[])
{
	char	*arg;
	char	**args = (char **)malloc(sizeof(char *)*(argc - 1)); // args 
	int		nArgs = 0;				// number of args 
	int	 i, result;
	char* pool = NULL;
	char* scheddName = NULL;
	char* scheddAddr = NULL;
	MyString method;
	char *tmp;
	param_functions *p_funcs = NULL;

	myDistro->Init( argc, argv );
	config();

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	// dig around in the config file looking for what the config file says
	// about getting files from Condor. This defaults with the global variable
	// initialization.
	tmp = param( "SANDBOX_TRANSFER_METHOD" );
	if ( tmp != NULL ) {
		method = tmp;
		free( tmp );
		string_to_stm( method, st_method );
	}

	set_usage(usage);
	if( ! arg[1] )
		tool_usage(1);

	// parse the arguments.
	for(i=1; i < argc; i++) {
		if(match_prefix(argv[i], "-debug"))
			continue;
		if(match_prefix(argv[i], "-pool")
			|| match_prefix(argv[i], "-name")
			|| match_prefix(argv[i], "-addr"))
		{
			i++;
			continue;
		}
		if(match_prefix(argv[i], "-constraint"))
		{
			args[nArgs] = argv[i];
				nArgs++;
				i++;
				if( ! argv[i] ) {
					fprintf( stderr, 
							 "%s: -constraint requires another argument\n", 
							 toolname);
					tool_exit(1);
				}				
				args[nArgs] = argv[i];
				nArgs++;
		}
		else if(match_prefix(argv[i], "-all"))
			All = true;
		else if(match_prefix(argv[i], "-stm"))
		{
			i++;
			if( ! argv[i] ) {
				fprintf( stderr, "%s: -stm requires another argument\n", 
						 toolname);
				tool_exit(1);
			}				
			method = argv[i];
			string_to_stm(method, st_method);
		}
		else if(match_prefix(argv[i], "-"))
		{
			fprintf( stderr, "Unrecognized option: %s\n", arg ); 
			usage();
		}
		else
		{
			if(All)
				usage();

			args[nArgs] = argv[i];
			++nArgs;
		}
	}

	if(addr_arg)
	{
		if(scheddAddr) free(scheddAddr);
		scheddAddr = strdup(addr_arg);
		if( ! scheddAddr ) {
			fprintf( stderr, "Out of Memory!\n" );
			tool_exit(1);
		}
	}

	if(name_arg)
	{
		if ( scheddName ) free(scheddName);
		scheddName = strdup(name_arg);
		if( ! scheddName ) {
			fprintf( stderr, "Out of Memory!\n" );
			tool_exit(1);
		}
	}

	if(pool_arg)
	{
		if(pool) free(pool);
		pool = strdup(pool_arg);
		if(!pool)
		{
			fprintf( stderr, "Out of Memory!\n" );
			tool_exit(1);
		}
	}

	// Check to make sure we have a valid sandbox transfer mechanism.
	if (st_method == STM_UNKNOWN) {
		fprintf( stderr,
			"%s: Unknown sandbox transfer method: %s\n", toolname,
			method.Value());
		usage();
		exit(1);
	}

	if( ! (All || nArgs) ) {
			// We got no indication of what to act on


		fprintf( stderr, "You did not specify any jobs\n" ); 
		usage();
	}

		// We're done parsing args, now make sure we know how to
		// contact the schedd. 
	if( ! scheddAddr ) {
			// This will always do the right thing, even if either or
			// both of scheddName or pool are NULL.
		schedd = new DCSchedd( scheddName, pool );
	} else {
		schedd = new DCSchedd( scheddAddr );
	}
	if( ! schedd->locate() ) {
		fprintf( stderr, "%s: %s\n", toolname, schedd->error() ); 
		exit( 1 );
	}

		// Process the args.
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

		// Sanity check: make certain we now have a constraint
	if ( global_constraint.Length() <= 0 ) {			
		fprintf( stderr, "Unable to create a job constraint!\n");
		exit(1);
	}

	fprintf(stdout,"Fetching data files...\n");

	switch(st_method) {
		case STM_USE_SCHEDD_ONLY:
			{ // start block

			// Get the sandbox directly from the schedd.
			// And now, do the work.
			CondorError errstack;
			result = schedd->receiveJobSandbox(global_constraint.Value(),
				&errstack);
			if ( !result ) {
				fprintf( stderr, "\n%s\n", errstack.getFullText(true) );
				fprintf( stderr, "ERROR: Failed to spool job files.\n" );
				exit(1);
			}
		
			// All done
			return 0;

			} //end block
			break;

		case STM_USE_TRANSFERD:
			{ // start block

			// NEW METHOD where we ask the schedd for a transferd, then get the
			// files from the transferd

			CondorError errstack;
			ClassAd respad;
			int invalid;
			MyString reason;
			MyString td_sinful;
			MyString td_cap;

			result = schedd->requestSandboxLocation(FTPD_DOWNLOAD, 
				global_constraint, FTP_CFTP, &respad, &errstack);
			if ( !result ) {
				fprintf( stderr, "\n%s\n", errstack.getFullText(true) );
				fprintf( stderr, "ERROR: Failed to spool job files.\n" );
				exit(1);
			}

			respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
			if (invalid == TRUE) {
				fprintf( stderr, "ERROR: Failed to spool job files.\n" );
				respad.LookupString(ATTR_TREQ_INVALID_REASON, reason);
				fprintf( stderr, "%s\n", reason.Value());
				exit(EXIT_FAILURE);
			}

			respad.LookupString(ATTR_TREQ_TD_SINFUL, td_sinful);
			respad.LookupString(ATTR_TREQ_CAPABILITY, td_cap);

			dprintf(D_ALWAYS, 
				"td: %s, cap: %s\n", td_sinful.Value(), td_cap.Value());

			DCTransferD dctd(td_sinful.Value());

			result = dctd.download_job_files(&respad, &errstack);
			if ( !result ) {
				fprintf( stderr, "\n%s\n", errstack.getFullText(true) );
				fprintf( stderr, "ERROR: Failed to spool job files.\n" );
				exit(1);
			}

			} // end block
			break;

		default:
			EXCEPT("PROGRAMMER ERROR: st_method must be known.");
			break;
	}

	// All done
	return 0;
}