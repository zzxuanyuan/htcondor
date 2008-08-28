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

#include <termios.h>
#include <uuid/uuid.h>

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_string.h"
#include "basename.h"
#include "internet.h"
#include "../condor_minica/minica_common.h"
//#include "minica_common.h"
#include "condor_distribution.h"
#include "dc_minica.h"

/**
 * Given a prompt, ask for a password twice.  If they match, it's all
 * good; if not, abort.
 **/
char *
my_getpass_prompt(char * prompt)
{
    char *pass = NULL;
    char *pass_confirm = NULL;
    int i = 0;
    int size, size_c;

    /* Get password try 1 */
    // fprintf( stderr, "%s\n", prompt );
    pass = my_getpass( prompt );
    if( strlen( pass ) == 0 ) {
        dprintf( D_SECURITY, "Error getting password\n" );
        exit( 1 );
    }

    /* Get password try 2 */
    fprintf( stderr, "Confirm: " );
    pass_confirm = my_getpass( prompt );
    if( strlen( pass_confirm ) == 0 ) {
        dprintf(D_SECURITY, "Error getting password\n" );
        exit( 1 );
    }

    /* Compare the two passwords. */
    if( strcmp(pass, pass_confirm) == 0 ) {
        // passwords match.

        /* Zeroize. */
        size_c = strlen( pass_confirm );
        for( i = 0; i < size_c ; i++ ) {
            pass_confirm[i] = 0;
        }
        free( pass_confirm );
        
        return pass;
    } else {
        dprintf(D_ALWAYS, "Passwords don't match: aborting.\n" );

        /* Zeroize. */
        size_c = strlen( pass_confirm );
        for( i = 0; i < size_c; i++ ) {
            pass_confirm[i] = 0;
        }
        free( pass_confirm );
        size = strlen ( pass );
        for ( i = 0; i < size; i++ ) {
            pass[i] = 0;
        }
        free( pass );
        
        return NULL;
    }
}

/**
 * Print usage information and exit.
 */
void
usage( char * my_name )
{
    fprintf( stderr, "Usage: %s [options] [-c [username] | -h hostname ] -f filename\n", my_name );
    fprintf( stderr, "  Valid options:\n" );
    fprintf( stderr, "  -s\t\tGet the openssl.cnf file from the server.\n" );
    fprintf( stderr, "  -a <sin.ful.str.ing:port>\tSpecify the minica address.\n" );
    fprintf( stderr, "  -m filename\tSpecify the file containing the minica address.\n" );
    fprintf( stderr, "  -c\t\tRequest client certificate for the current user@UID_DOMAIN.\n" );
    fprintf( stderr, "  -C username\tRequest client certificate for the specified user.\n" );
    fprintf( stderr, "  -h hostname\tRequest host certificate for the specified host.\n" );
    fprintf( stderr, "  Only administrative users can request host certificates or client\n" );
    fprintf( stderr, "  certificates for other users.\n" );
    fprintf( stderr, "  -f filename\tBase filename for storing key, request, and certificate\n" );
    fprintf( stderr, "             \t(.key, .req, and .crt, respectively, will be added.)\n" );
    fprintf( stderr, "  -p\t\tPassive mode: key generation is done on server side.\n" );
    fprintf( stderr, "  -u\t\tAdd UUID to CN.\n" );
    exit( 1 );
}

/*
 * Main routine performs following steps:
 * 1) Handle command line arguments & paramaterize.
 * 2) Get password from user for client keys.
 * 3) Fork to generate private key.
 * 4) Fork to generate certificate signing request.
 * 5) Connect to CA service.
 * 6) Engage in protocol, sending request, obtain result.
 */
int
main( int argc, char *argv[] )
{
	myDistro->Init(argc, argv);
    config(); // external

	CondorError errstack;
    char **ptr;
    char *my_name = NULL;
    request_type req = NONE;
    char *client_name = NULL;
    char *host_name = NULL;
    struct passwd *pwent = NULL;
	char *base_file_name = NULL;
	char *server_ss = NULL;
	bool passive_client;
	bool use_server_openssl_cnf;
	bool add_uuid;
	char *tmp;
	int err;

    my_name = strrchr( argv[0], DIR_DELIM_CHAR );
	if(!my_name) {
		my_name = argv[0];
	} else {
		my_name++;
	}
    
    pwent = getpwuid( getuid( ) );
    if( pwent == NULL ) {
        fprintf(stderr, "Error getting user name.\n");
        exit( 1 );
    }

    for ( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
        if( ptr[0][0] == '-' ) {
            switch( ptr[0][1] ) {
            case 'a': // Specify addr on command line as a sinful string.
                ++ptr;
                if( !(--argc) || !(*ptr) ) {
                    fprintf(stderr, "-a requires another argument" );
                    exit( 1 );
                }
                if( is_valid_sinful( *ptr ) ) {
					server_ss = strdup( *ptr );
                } else {
                    fprintf(stderr, "Invalid sinful string.\n" );
                    usage( my_name );
                }
                break;
            case 'c':
                if( req != NONE ) {
                    fprintf(stderr, "Too many arguments.\n" );
                    usage( my_name );
                }
                req = CLIENT;
                break;
            case 'C': // Indicate that the request is for a client
                ++ptr;
                if( req != NONE ) {
                    fprintf(stderr, "Too many arguments.\n" );
                    usage( my_name );
                }
                req = CLIENT;
                if( !(--argc) || !(*(ptr)) ) {
                    fprintf(stderr, "%s: -C requires another argument\n",
                                 my_name );
                    usage( my_name );
                    // No more arguments, assume that the user is requesting
                    // a certificate for herself.  So, client_name stays NULL.
                } else {
                    client_name = strdup(*ptr);
                    fprintf(stderr, "%s: set client name to %s\n",
                             my_name, client_name );
                }
                break;
			case 'd':
				Termlog = 1;
				dprintf_config("TOOL");
				break;
            case 'f': // Base filename for key and cert files.
                ++ptr;
                if( !(--argc) || !(*ptr) ) {
                    fprintf(stderr, "-f requires another argument" );
                    usage( my_name );
                }
				base_file_name = strdup( *ptr );
                break;
            case 'h': // Request is for a host cert.
                ++ptr;
                if( req != NONE ) {
                    fprintf(stderr, "Too many arguments.\n");
                    usage( my_name );
                }
                req = HOST;
                if( !(--argc) || !(*(ptr)) ) {
                    fprintf(stderr, "%s: -h requires another argument\n",
                             my_name );
                    usage( my_name );
                }
                host_name = strdup(*ptr);
                fprintf(stderr, "%s: set host name to %s\n",
                         my_name, host_name );
                break;
            case 'm': // Specify file containing sinful addr.
                ++ptr;
                if( !(--argc) || !(*ptr) ) {
                    EXCEPT( "-m requires another argument" );
                }
				tmp = NULL;
				
                if( (tmp = file_to_str(*ptr, &err)) && is_valid_sinful( tmp ) ) {
					server_ss = tmp;
					tmp = NULL;
				} else {
					if(tmp) free(tmp);
					fprintf(stderr, "Invalid sinful string file.\n" );
					usage( my_name );
				}
                break;
            case 'p': // client is "passive" - the key is generated on the server side.
                passive_client = true;
                break;
            case 's':
                use_server_openssl_cnf = true;
                break;
            case 'u':
                add_uuid = true;
                break;
            default:
                usage( my_name );
            }
        } else {
            fprintf(stderr, "Error parsing command line arguments.\n" );
            usage( my_name );
        }
    }

    DCMinica dc_minica( server_ss );

    if( base_file_name == false ) {
		// Make the default be $HOME/.globus/usercert.pem
		// $HOME/.globus/usercred.p12
		// $HOME/.globus/userreq.pem
		char *home = getenv("HOME");
		if(!home) {
			fprintf(stderr, "Error getting $HOME from env.\n" );
			fprintf(stderr, "Specify -f basename.\n");
			usage( my_name );
		}
		MyString key = home;
		MyString rq = home;
		MyString cert = home;
		key = key + "/.globus/usercred.p12";
		rq = rq + "/.globus/userreq.pem";
		cert = cert + "/.globus/usercert.pem";
		dc_minica.setKeyFilename(key);
		dc_minica.setReqFilename(rq);
		dc_minica.setCertFilename(cert);
	} else {
		MyString key = base_file_name;
		MyString rq = base_file_name;
		MyString cert = base_file_name;
		key = key + ".key";
		rq = rq + ".req";
		cert = cert + ".crt";
		dc_minica.setKeyFilename(key);
		dc_minica.setReqFilename(rq);
		dc_minica.setCertFilename(cert);
	}

    /** Summarize options and actions, build common name **/
       
    if( req == NONE ) {
        req = CLIENT;
    }

    // fprintf(stderr, "Command line arguments parsed.  State summary:\n" );
    MyString cn;
    switch( req ) {
    case CLIENT:
        if( client_name == NULL ) {
             client_name = pwent->pw_name;
			tmp = param("UID_DOMAIN");
			cn = client_name;
			if(tmp) {
				cn = cn + "@" + tmp;
				free(tmp);
			}
        } else {
			cn = client_name;
        }
		dc_minica.setCommonName(cn);
        break;
    case HOST:
        if( host_name == NULL ) {
            fprintf(stderr, "Sanity!: no host!\n" );
            exit( 1 );
		}
		cn = host_name;
		dc_minica.setCommonName(cn);
        break;
    default:

        // This really should never happen.
        fprintf(stderr, "Sanity!: no request type!\n" );
        exit( 1 );
    }

    if( add_uuid ) {
		cn = dc_minica.getCommonName();
		char ustr[40];
		memset(ustr,0,40);
        uuid_t u;
        if(!ustr) {
            fprintf(stderr, "Can't get memory.\n" );
            exit( 1 );
        }
        uuid_generate( u );
        uuid_unparse( u, ustr );
		cn = cn + ustr;
        dc_minica.setCommonName(cn);
    }
    
    tmp = param( "MCA_PATH_TO_OPENSSL" );
    if( tmp == NULL ) {
        fprintf(stderr, "reconfig: Can't get path to openssl binary.\n" );
        exit( 1 );
    }
	dc_minica.setPathToOpenssl(tmp);
	free(tmp); 

    if( ! use_server_openssl_cnf ) { // if true, we don't do this here
        tmp = param( "MCA_PATH_TO_OPENSSL_CNF" );
        if( tmp == NULL ) {
            fprintf(stderr, "Can't get path to openssl configuration file.\n" );
            exit( 1 );
        }
		dc_minica.setPathToOpensslCnf(tmp);
		free(tmp);
    }

	dc_minica.setUseServerOpensslCnf(use_server_openssl_cnf);
	dc_minica.setPassiveClient(passive_client);

	char *pass = NULL;
    if( req == CLIENT ) {
        pass = my_getpass_prompt(
            "Type password used to encrypt the private key: ");
        if( pass == NULL || ( (strlen( pass ) == 1) && (pass[0] == '\n')) ) {
            fprintf(stderr, "Password is null.\n" );
            exit( 1 );
        }
        if( strlen( pass ) < 4 ) {
            fprintf(stderr, "Password too short.\n" );
            exit( 1 );
        }
    } // else pass is null
	dc_minica.setPassword(pass);

    fprintf(stderr, "Finding MiniCA server.\n" );
	if( ! dc_minica.locate() ) {
		fprintf(stderr, "Can't locate: %s\n", dc_minica.error());
		return 1;
	}

    fprintf(stderr, "Starting command.\n" );
	if(dc_minica.signRequest( errstack )) {
		printf("Request complete.\n");
	} else {
		fprintf(stderr, "Unable to obtain certificate: %s.\n", 
				errstack.getFullText(true));
		return 1;
	}
	return 0;
}

