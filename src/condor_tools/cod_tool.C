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

void version();

// Global variables
int cmd = 0;
char* addr = NULL;
char* name = NULL;
char* pool = NULL;
char* target = NULL;
char* my_name = NULL;
char* claim_id = NULL;
bool needs_id = true;
VacateType vacate_type = VACATE_GRACEFUL;

void
usage( char *str )
{
		// TODO!!!
	fprintf(stderr, "usage: \n" );
	exit( 1 );
}


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
		case 'p':
			tmp++;
			if( tmp && *tmp ) {
				pool = get_full_hostname( (const char *)(*tmp) );
				if( !pool ) {
					fprintf( stderr, "%s: unknown host %s\n", my_name,
							 *tmp ); 
					exit( 1 );	
				}
			} else {
				fprintf( stderr,
						 "ERROR: -pool requires another argument\n" );
				usage( my_name );
			}
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
				fprintf( stderr, 
						 "ERROR: -addr requires another argument\n" ); 
				usage( my_name );
			}
			addr = strdup( *tmp ); 
			break;
		case 'i':
			tmp++;
			if( ! (tmp && *tmp) ) {
				fprintf( stderr, 
						 "ERROR: -id requires another argument\n" ); 
				usage( my_name );
			} else {
				claim_id = strdup( *tmp );
			}
			break;
		case 'n':
				// We got a "-name", make sure we've got 
				// something else after it
			tmp++;
			if( ! (tmp && *tmp) ) {
				fprintf( stderr, 
						 "ERROR: -name requires another argument\n" );
				usage( my_name );
			}
			name = get_daemon_name( *tmp );
			if( ! name ) {
                fprintf( stderr, "%s: unknown host %s\n", my_name, 
                         get_host_part(*tmp) );
				exit( 1 );
			}
			break;
		default:
			fprintf( stderr, "ERROR: invalid argument \"%s\"\n",
					 *tmp );
			usage( my_name );
		}
	}

		// now that we're done parsing, do some checking
	if( needs_id && ! claim_id ) {
		fprintf( stderr, 
				 "You need to specify the ClaimID with -id for %s\n",
				 my_name );
		exit( 1 );
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
}


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
		fprintf( stderr, "Failed to send %s to startd %s\n%s\n",
				 getCommandString(cmd), startd.addr(), startd.error() ); 
		return 1;
	}
	printf( "Sent %s to startd at %s\n", getCommandString(cmd),
			startd.addr() ); 
	return 0;
}

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}

