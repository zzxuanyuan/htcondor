/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
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

 

/*********************************************************************
  COD command-line tool
*********************************************************************/

#include "condor_common.h"
#include "condor_distribution.h"
#include "command_strings.h"
#include "enum_utils.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_version.h"
#include "get_full_hostname.h"
#include "get_daemon_addr.h"
#include "daemon.h"
#include "dc_startd.h"
#include "sig_install.h"


// Global variables
int cmd = 0;
char* addr = NULL;
char* name = NULL;
char* pool = NULL;
char* target = NULL;
char* my_name = NULL;
char* claim_id = NULL;
char* classad_path = NULL;
FILE* CA_PATH = NULL;
int cluster_id = -1;
int proc_id = -1;
bool needs_id = true;
VacateType vacate_type = VACATE_GRACEFUL;


// protoypes of interest
void usage( char* );
void version( void );
void invalid( char* opt );
void ambiguous( char* opt );
void another( char* opt );
void parseCOpt( char* opt, char* arg );
void parsePOpt( char* opt, char* arg );
void parseArgv( int argc, char* argv[] );
int getCommandFromArgv( int argc, char* argv[] );


/*********************************************************************
   main()
*********************************************************************/

int
main( int argc, char *argv[] )
{

#ifndef WIN32
	// Ignore SIGPIPE so if we cannot connect to a daemon we do not
	// blowup with a sig 13.
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	myDistro->Init( argc, argv );

	config();

	cmd = getCommandFromArgv( argc, argv );
	
	parseArgv( argc, argv );

	DCStartd startd( target, pool );

	if( needs_id ) {
		assert( claim_id );
		startd.setClaimId( claim_id );
	}

	if( ! startd.locate() ) {
		fprintf( stderr, "ERROR: %s\n", startd.error() );
		exit( 1 );
	}

	bool rval;
	ClassAd reply;

		// TODO!!! for the commands that need this, we better put
		// something in it. :)
	ClassAd ad;

	switch( cmd ) {
	case CA_REQUEST_CLAIM:
		rval = startd.requestClaim( CLAIM_COD, &ad, &reply );
		break;
	case CA_ACTIVATE_CLAIM:
		rval = startd.activateClaim( &ad, &reply );
		break;
	case CA_SUSPEND_CLAIM:
		rval = startd.suspendClaim( &reply );
		break;
	case CA_RESUME_CLAIM:
		rval = startd.resumeClaim( &reply );
		break;
	case CA_DEACTIVATE_CLAIM:
		rval = startd.deactivateClaim( vacate_type, &reply );
		break;
	case CA_RELEASE_CLAIM:
		rval = startd.releaseClaim( vacate_type, &reply );
		break;
	}

	if( ! rval ) {
		fprintf( stderr, "Attempt to send %s to startd %s failed\n%s\n",
				 getCommandString(cmd), startd.addr(), startd.error() ); 
		return 1;
	}

	if( CA_PATH ) {
		reply.fPrint( CA_PATH );
		fclose( CA_PATH );
		printf( "Sent %s to startd at %s\n", getCommandString(cmd),
				startd.addr() ); 
		printf( "Result ClassAd written to %s\n", classad_path );
	} else {
		if( cmd == CA_REQUEST_CLAIM ) {
			fprintf( stderr, "Sent %s to startd at %s\n", 
					 getCommandString(cmd), startd.addr() ); 
			fprintf( stderr, "WARNING: You did not specify "
					 "-classad_path, printing to STDOUT\n" );
			reply.fPrint( stdout );
		}
	}
	return 0;
}



/*********************************************************************
   Helper functions used by main()
*********************************************************************/

// TODO



/*********************************************************************
   Helper functions to parse the command-line 
*********************************************************************/

int
getCommandFromArgv( int argc, char* argv[] )
{
	char* cmd_str = NULL;
	int size;

	my_name = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !my_name ) {
		my_name = argv[0];
	} else {
		my_name++;
	}

		// See if there's an '-' in our name, if not, append argv[1]. 
	cmd_str = strchr( my_name, '_');
	if( !cmd_str ) {

			// If there's no argv[1], print usage.
		if( ! argv[1] ) { usage( my_name ); }

			// If argv[1] begins with '-', print usage, don't append.
		if( argv[1][0] == '-' ) { 
				// The one exception is if we got a "cod -v", we
				// should print the version, not give an error.
			if( argv[1][1] == 'v' ) {
				version();
			} else {
				usage( my_name );
			}
		}
		size = strlen( argv[1] );
		my_name = (char*)malloc( size + 5 );
		sprintf( my_name, "cod_%s", argv[1] );
		cmd_str = my_name+3;
		argv++; argc--;
	}
		// Figure out what kind of tool we are.
	if( !strcmp( cmd_str, "_request" ) ) {
			// this is the only one that doesn't require a claim id 
		needs_id = false;
		return CA_REQUEST_CLAIM;
	} else if( !strcmp( cmd_str, "_release" ) ) {
		return CA_RELEASE_CLAIM;
	} else if( !strcmp( cmd_str, "_activate" ) ) {
		return CA_ACTIVATE_CLAIM;
	} else if( !strcmp( cmd_str, "_deactivate" ) ) {
		return CA_DEACTIVATE_CLAIM;
	} else if( !strcmp( cmd_str, "_suspend" ) ) {
		return CA_SUSPEND_CLAIM;
	} else if( !strcmp( cmd_str, "_resume" ) ) {
		return CA_RESUME_CLAIM;
	} else {
		fprintf( stderr, "ERROR: unknown command %s\n", my_name );
		usage( "cod" );
	}
	return -1;
}


void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}

void
invalid( char* opt )
{
	fprintf( stderr, "%s: '%s' is invalid\n", my_name, opt );
	usage( my_name );
}


void
ambiguous( char* opt )
{
	fprintf( stderr, "%s: '%s' is ambiguous\n", my_name, opt ); 
	usage( my_name );
}


void
another( char* opt )
{
	fprintf( stderr, "%s: '%s' requires another argument\n", my_name,
			 opt ); 
	usage( my_name );
}


void
parseCOpt( char* opt, char* arg )
{
 		// first, try to understand what the option is they passed,
		// given what kind of cod command we want to send.  -cluster
		// is only valid for activate, so if we're not doing that, we
		// can assume they want -classadpath

	char _cpath[] = "-classad_path";
	char _clust[] = "-cluster_id";
	char* target = NULL;

		// we already checked these, make sure we're not crazy
	assert( opt[0] == '-' && opt[1] == 'c' );  

	if( cmd == CA_ACTIVATE_CLAIM ) {
		if( ! (opt[2] && opt[3]) ) {
			ambiguous( opt );
		}
		if( opt[2] != 'l' ) {
			invalid( opt );
		}
		switch( opt[3] ) {
		case 'a':
			if( strncmp(_cpath, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			target = _cpath;
			break;

		case 'u':
			if( strncmp(_clust, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			target = _clust;
			break;

		default:
			invalid( opt );
			break;
		}
	} else { 
		if( strncmp(_cpath, opt, strlen(opt)) ) {
			invalid( opt );
		}
		target = _cpath;
	}

		// now, make sure we got the arg
	if( ! arg ) {
		another( target );
	}
	if( target == _clust ) {
			// we can check like that, since we're setting target to
			// point to it, so we don't have to do a strcmp().
		cluster_id = atoi( arg );
	} else {
		classad_path = strdup( arg );
	}
}


void
parsePOpt( char* opt, char* arg )
{
 		// first, try to understand what the option is they passed,
		// given what kind of cod command we want to send.  -proc
		// is only valid for activate, so if we're not doing that, we
		// can assume they want -pool

	char _pool[] = "-pool";
	char _proc[] = "-proc_id";
	char* target = NULL;

		// we already checked these, make sure we're not crazy
	assert( opt[0] == '-' && opt[1] == 'p' );  

	if( cmd == CA_ACTIVATE_CLAIM ) {
		if( ! opt[2] ) {
			ambiguous( opt );
		}
		switch( opt[2] ) {
		case 'o':
			if( strncmp(_pool, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			target = _pool;
			break;

		case 'r':
			if( strncmp(_proc, opt, strlen(opt)) ) {
				invalid( opt );
			} 
			target = _proc;
			break;

		default:
			invalid( opt );
			break;
		}
	} else { 
		if( strncmp(_pool, opt, strlen(opt)) ) {
			invalid( opt );
		} 
		target = _pool;
	}

		// now, make sure we got the arg
	if( ! arg ) {
		another( target );
	}

	if( target == _pool ) {
		pool = get_full_hostname( (const char *)(arg) );
		if( !pool ) {
			fprintf( stderr, "%s: unknown host %s\n", my_name, arg );
			exit( 1 );
		}
	} else {
		proc_id = atoi( arg );
	}
}


void
parseArgv( int argc, char* argv[] )
{
	char** tmp = argv;

	for( tmp++; *tmp; tmp++ ) {
		if( (*tmp)[0] != '-' ) {
				// If it doesn't start with '-', skip it
			continue;
		}
		switch( (*tmp)[1] ) {
		case 'v':
			version();
			break;
		case 'h':
			usage( my_name );
			break;
		case 'd':
			Termlog = 1;
			dprintf_config ("TOOL", 2);
			break;
		case 'f':
			vacate_type = VACATE_FAST;
			break;
		case 'a':
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-address" );
			}
			addr = strdup( *tmp ); 
			break;
		case 'i':
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-id" );
			}
			claim_id = strdup( *tmp );
			break;
		case 'n':
				// We got a "-name", make sure we've got 
				// something else after it
			tmp++;
			if( ! (tmp && *tmp) ) {
				another( "-name" );
			}
			name = get_daemon_name( *tmp );
			if( ! name ) {
                fprintf( stderr, "%s: unknown host %s\n", my_name, 
                         get_host_part(*tmp) );
				exit( 1 );
			}
			break;

				// P and C are complicated, since they are ambiguous
				// in the case of activate, but not others.  so, they
				// have their own methods to make it easier to
				// understand what the hell's going on. :)
		case 'p':
			parsePOpt( tmp[0], tmp[1] );
			tmp++;
			break;
		case 'c':
			parseCOpt( tmp[0], tmp[1] );
			tmp++;
			break;

		default:
			invalid( *tmp );
		}
	}

		// Now that we're done parsing, make sure it makes sense 
	if( needs_id && ! claim_id ) {
		fprintf( stderr, 
				 "ERROR: You must specify the ClaimID with -id for %s\n",
				 my_name );
		usage( my_name );
	}

	if( addr && name ) {
		fprintf( stderr, 
				 "ERROR: You cannot specify both -name and -address\n" );
		usage( my_name );
	}

	if( addr ) {
		target = addr;
	} else if( name ) {
		target = name;
	} else { 
			// local startd
		target = NULL;
	}
	if( classad_path ) { 
		CA_PATH = fopen( classad_path, "w" );
		if( !CA_PATH ) {
			fprintf( stderr, 
					 "ERROR: failed to open '%s': errno %d (%s)\n",
					 classad_path, errno, strerror(errno) );
			exit( 1 );
		}
	}
}


void
usage( char *str )
{
		// TODO!!!
	fprintf(stderr, "Usage: \n" );
	exit( 1 );
}

