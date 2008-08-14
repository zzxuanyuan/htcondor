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
#include "../condor_daemon_client/daemon.h"
#include "../condor_minica/minica_common.h"
//#include "minica_common.h"


typedef struct request_properties
{
    char *key_filename;
    char *request_filename;
    char *cert_filename;
    char *common_name;
    int passive_client;
    int use_server_openssl_cnf;
    int add_uuid;
} REQUEST_PROPERTIES;

char * _mca_path_to_openssl = NULL;
char * _mca_path_to_openssl_cnf = NULL;

char * MiniCAHost = NULL;

/**
 * This routine is used in the minica_common routines so that it can
 * be defined differently between the client and server mains that
 * report the same debugging information in different ways.  The
 * client just prints to standard error, but the server wants to
 * dprintf to the log file.
 */
void debug_print ( char * fmt, ... )
{
    char buf[ MCA_ERROR_BUFSIZE ]; 
    va_list args;
    va_start( args, fmt );
    vsnprintf( buf, MCA_ERROR_BUFSIZE, fmt, args );
    fprintf( stderr, "%s", buf );
}

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
        debug_print( "Error getting password\n" );
        exit( 1 );
    }

    /* Get password try 2 */
    fprintf( stderr, "Confirm: " );
    pass_confirm = my_getpass( prompt );
    if( strlen( pass_confirm ) == 0 ) {
        debug_print( "Error getting password\n" );
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
        debug_print( "Passwords don't match: aborting.\n" );

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
    fprintf( stderr, "  -c\t\tRequest client certificate for the current user.\n" );
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

/**
 * Obtain information about globals from condor config file.
 */
void
reconfig(int have_cnf)
{
    char * tmp = NULL;
    debug_print( "Reconfig called.\n" );
    tmp = param( "MCA_PATH_TO_OPENSSL" );
    if( tmp == NULL ) {
        debug_print( "reconfig: Can't get path to openssl binary.\n" );
        exit( 1 );
    }
    if( tmp ) {
        if( _mca_path_to_openssl != NULL ) {
            free( _mca_path_to_openssl );
        }
        _mca_path_to_openssl = tmp;
    }
    if( ! have_cnf ) { // if true, we don't do this and mca_path_to_openssl is affected.
        tmp = param( "MCA_PATH_TO_OPENSSL_CNF" );
        if( tmp == NULL ) {
            debug_print( "reconfig: Can't get path to openssl configuration file.\n" );
            exit( 1 );
        }
        if( tmp ) {
            if( _mca_path_to_openssl_cnf != NULL ) {
                free( _mca_path_to_openssl_cnf );
            }
            _mca_path_to_openssl_cnf = tmp;
        }
    }
    debug_print( "Set _mca_path_to_openssl to '%s'.\n", _mca_path_to_openssl );
}

/**
 * Parse the command line.
 */
request_type
parse_command_line( int argc,
                    char *argv[],
                    REQUEST_PROPERTIES *r )
{
    char **ptr;
    int got_filename = 0;
    int filename_len = 0;
    FILE *addr_fp;
    char *my_name = NULL;
    request_type req = NONE;
    char buf[101];
    char *client_name = NULL;
    char *host_name;
    struct passwd *pwent = NULL;

    my_name = strdup( basename( argv[0] ) );
    
    r->common_name = (char *)malloc( MCA_MAX_COMMONNAME_LEN );
    if( r->common_name == NULL ) {
        perror( "parse_command_line" );
        debug_print( "Can't malloc for common_name.\n" );
        exit( 1 );
    }

    pwent = getpwuid( getuid( ) );
    if( pwent == NULL ) {
        debug_print( "Error getting user name.\n" );
        exit( 1 );
    }

    for ( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
        if( ptr[0][0] == '-' ) {
            switch( ptr[0][1] ) {
            case 'f': // Base filename for key and cert files.
                ++ptr;
                if( !(--argc) || !(*ptr) ) {
                    debug_print( "-f requires another argument" );
                    usage( my_name );
                }
                filename_len = strlen( *ptr ) + 5;
                r->key_filename = (char *)malloc( filename_len );
                r->request_filename = (char *)malloc( filename_len );
                r->cert_filename = (char *)malloc( filename_len );
                if( (r->key_filename == NULL)
                    || (r->request_filename == NULL)
                    || (r->cert_filename == NULL) ) {
                    perror( "parse_command_line" ); 
                    debug_print( "Can't malloc for filename" );
                    exit( 1 );
                }
                snprintf( r->key_filename, filename_len, "%s.key", *ptr );
                snprintf( r->request_filename, filename_len, "%s.req", *ptr );
                snprintf( r->cert_filename, filename_len, "%s.crt", *ptr );
                got_filename = true;
                break;
            case 'a': // Specify addr on command line as a sinful string.
                ++ptr;
                if( !(--argc) || !(*ptr) ) {
                    debug_print( "-a requires another argument" );
                    exit( 1 );
                }
                if( is_valid_sinful( *ptr ) ) {
                    MiniCAHost = strnewp( *ptr );
                } else {
                    debug_print( "Invalid sinful string.\n" );
                    usage( my_name );
                }
                break;
            case 'm': // Specify file containing sinful addr.
                ++ptr;
                if( !(--argc) || !(*ptr) ) {
                    EXCEPT( "-m requires another argument" );
                }
                if( (addr_fp = safe_fopen_wrapper( *ptr, "r" )) ) {
                    fgets( buf, 100, addr_fp );
                    chomp( buf );
                    fclose( addr_fp );
                    if( is_valid_sinful( buf ) ) {
                        MiniCAHost = strnewp( buf );
                    } else {
                        debug_print( "Invalid sinful string file.\n" );
                        usage( my_name );
                    }
                    debug_print( "Got daemon address: %s.\n", MiniCAHost );
                } else {
                    debug_print( "Invalid sinful string file.\n" );
                    usage( my_name );
                }
                break;
            case 'c':
                if( req != NONE ) {
                    debug_print( "Too many arguments.\n" );
                    usage( my_name );
                }
                req = CLIENT;
                break;
            case 'C': // Indicate that the request is for a client
                ++ptr;
                if( req != NONE ) {
                    debug_print( "Too many arguments.\n" );
                    usage( my_name );
                }
                req = CLIENT;
                if( !(--argc) || !(*(ptr)) ) {
                    debug_print( "%s: -C requires another argument\n",
                                 my_name );
                    usage( my_name );
                    // No more arguments, assume that the user is requesting
                    // a certificate for herself.  So, client_name stays NULL.
                } else {
                    client_name = *ptr;
                    debug_print( "%s: set client name to %s\n",
                             my_name, client_name );
                }
                break;
            case 'h': // Request is for a host cert.
                ++ptr;
                if( req != NONE ) {
                    debug_print( "Too many arguments.\n");
                    usage( my_name );
                }
                req = HOST;
                if( !(--argc) || !(*(ptr)) ) {
                    debug_print( "%s: -h requires another argument\n",
                             my_name );
                    usage( my_name );
                }
                host_name = *ptr;
                debug_print( "%s: set host name to %s\n",
                         my_name, host_name );
                break;
            case 'p': // client is "passive" - the key is generated on the server side.
                r->passive_client = true;
                break;
            case 's':
                r->use_server_openssl_cnf = true;
                break;
            case 'u':
                r->add_uuid = true;
                break;
            default:
                usage( my_name );
            }
        } else {
            debug_print( "Error parsing command line arguments.\n" );
            usage( my_name );
        }
    }
    if( got_filename == false ) {
        debug_print(
                 "Couldn't get filename!  Make sure you specify one with the -f option.\n" );
        usage( my_name );
    }

    /** Summarize options and actions, build common name **/
       
    if( req == NONE ) {
        req = CLIENT;
    }

    // debug_print( "Command line arguments parsed.  State summary:\n" );
    
    switch( req ) {
    case CLIENT:
        //debug_print( "  Client certificate requested.\n" );
        if( client_name == NULL ) {
            //debug_print( "    Assuming that the client is the current user.\n" );
            client_name = pwent->pw_name;
        } else {
            //debug_print( "    The client certificate will be requested for '%s'.\n",
            //         client_name );
        }
        snprintf( r->common_name, MCA_MAX_COMMONNAME_LEN, "%s", client_name );
        break;
    case HOST:
        //debug_print( "  Host certificate requested.\n" );
        if( host_name == NULL ) {
            //debug_print( "Sanity!: no host!\n" );
            exit( 1 );
        } else {
            //debug_print( "    The host certificate will be requested for '%s'.\n",
            //host_name );
        }
        snprintf( r->common_name, MCA_MAX_COMMONNAME_LEN, "%s", host_name );
        break;
    default:

        // This really should never happen.
        debug_print( "Sanity!: no request type!\n" );
        exit( 1 );
    }

    if( r->add_uuid ) {
        char *tmp = r->common_name;
        r->common_name = (char *)malloc( MCA_MAX_COMMONNAME_LEN );
        char *ustr = (char *) malloc( 40 );
        uuid_t u;
        if( NULL == r->common_name ) {
            debug_print( "Can't get memory.\n" );
            exit( 1 );
        }
        uuid_generate( u );
        uuid_unparse( u, ustr );
        snprintf( r->common_name, MCA_MAX_COMMONNAME_LEN, "%s %s", tmp, ustr );
        free( ustr );
        free( tmp );
    }
    
    debug_print( "Common name for requested certificate: '%s'\n", r->common_name );
    free( my_name );
    return req;
}

char *
get_openssl_cnf( ReliSock s )
{
    char *openssl_cnf = NULL;
    char *path_to_openssl_cnf;
    char path[] = "/tmp/openssl.cnf.XXXXXX";
    int clen, plen, cnf_fd;
    
    s.decode( );
    if( ! (s.code( openssl_cnf )) ) {
        debug_print( "error getting openssl.cnf file from server.\n" );
        exit( 1 );
    }
    if( ! (s.end_of_message( )) ) {
        debug_print( "error communicating with server.\n" );
        exit( 1 );
    }
    if( (cnf_fd = mkstemp( path )) == -1 ) {
        debug_print( "error making temp file for openssl.cnf.\n" );
        exit( 1 );
    }
    clen = strlen( openssl_cnf );
    if( write( cnf_fd, openssl_cnf, clen ) != clen ) {
        debug_print( "Error writing openssl.cnf.\n" );
        exit( 1 );
    }
    if( close( cnf_fd ) != 0 ) {
        debug_print( "Error closing temp file.\n" );
        exit( 1 );
    }
    plen = strlen( path );
    path_to_openssl_cnf = (char *) malloc( plen + 1 );
    snprintf( path_to_openssl_cnf, plen+1, "%s", path );
    return path_to_openssl_cnf;
}

void
maybe_transfer_openssl_cnf( ReliSock s, int xfer )
{
    s.encode( );
    if( ! ( (s.code( xfer ))
            && (s.end_of_message( )) ) ) {
        debug_print( "Error sending config file status.\n" );
        exit( 1 );
    }
    if( xfer ) {
        debug_print( "Getting openssl.cnf.\n" );
        _mca_path_to_openssl_cnf = get_openssl_cnf( s );
        debug_print( "Got path to openssl.cnf: '%s'\n", _mca_path_to_openssl_cnf );
    }
}


/**
 * When the client is passive, i.e. doesn't generate their own key or
 * certificate request, this gets the server side to do that.
 */
void
handle_passive_client( ReliSock s, char *pass, char *common_name,
                       char **key_block, char **cert_request )
{
    int response_code;
    char *error_message = NULL;

    //debug_print( "Passive client, initiating contact for key and request.\n" );
    s.encode( ); /* 5 */
    if( ! ( (s.code( pass ))
            && (s.code( common_name ))
            && (s.end_of_message( )) ) ) {
        debug_print( "Error sending data to server.\n" );
        exit( 1 );
    }
    //debug_print( "Receiving key and csr from server.\n" );
    s.decode( ); /* 6 */
    if( ! ( (s.code( response_code ))
            && (s.code( error_message ))
            && (s.code( *key_block ))
            && (s.code( *cert_request ))
            && (s.end_of_message( )) ) ) {
        debug_print( "Error getting key from server.\n" );
        exit( 1 );
    }
    //debug_print( "Sent: \n%s\n%s\n", pass, common_name );
    //debug_print( "Got: \n%s\n%s\n%s\n", error_message, *key_block, *cert_request );
    if( response_code != MCA_RESPONSE_ALL_OK ) {
        debug_print( "Server generated error: %s\n", error_message );
        exit( 1 );
    }
    //debug_print( "Key: %s\n", *key_block );
    //debug_print( "Certificate Request: %s\n", *cert_request );
}

/**
 * When the client does generate the key and signing request.  Active
 * => not passive.
 */
void
handle_active_client( char *pass, char *common_name,
                      char **key_block, char **cert_request )
{
    int error_code;
    
    /** Fork to generate rsa key **/
    debug_print( "Generating key...\n" );
    //debug_print( "_mca_path_to_openssl is '%s'.\n", _mca_path_to_openssl );

    *key_block = repeat_gen_rsa_key( pass, MCA_MAX_KEYGEN_ATTEMPTS,
                                    MCA_KEYGEN_WAIT_TIME_SECS, &error_code );
    
    if( error_code > MCA_ERROR_NOERROR ) {
        debug_print( "Error generating key: %s\n",
                     get_mca_error_message( error_code ) );
    }
    if( *key_block == NULL ) {
        debug_print( "Key generation failed.  Can't continue.\n" );
        exit( 1 );
    }

    // debug_print( "getting signing request.\n" );
    /** Fork to generate signing request. **/
    *cert_request = gen_signing_request( *key_block, common_name, pass,
                                         &error_code );
    
    if( error_code > MCA_ERROR_NOERROR ) {
        debug_print( "Error generating certificate request: %s\n",
                     get_mca_error_message( error_code ) );
    }// else {
    //    debug_print( "Got signing request OK.\n" );
    //}
    if( cert_request == NULL ) {
        debug_print( "Certificate request generation failed. Can't continue.\n" );
        exit( 1 );
    }
}

/**
 * Get the key and certificate signing request, either passively or not.
 */
void
get_key_csr( ReliSock s, int passive_client,
             char *pass, char *common_name,
             char **key_block, char **cert_request)
{
    int minica_server_status = 0;
    char *minica_server_response = NULL;
    // debug_print( "_mca_path_to_openssl is '%s'.\n", _mca_path_to_openssl );
    
    s.encode( ); /* 3 */
    if( ! ( (s.code( passive_client ))
            && (s.end_of_message( )) ) ) {
        debug_print( "Error sending data to server.\n" );
        exit( 1 );
    }

    s.decode( ); /* 4 */
    if( ! ( (s.code( minica_server_status ))
            && (s.code( minica_server_response ))
            && (s.end_of_message( )) ) ) {
        debug_print( "Error receiving response from server (4).\n" );
        exit( 1 );
    }
    switch( minica_server_status ) {
    case MCA_PROTOCOL_ABORT:
        debug_print( "Server sent protocol abort response.\n" );
        debug_print( "Server sent: '%s'\n", minica_server_response );
        exit( 1 );
        break;
    case MCA_PROTOCOL_PROCEED:
        debug_print( "Server sent: '%s'\n", minica_server_response );
        break;
    default:
        debug_print( "Server sent unknown protocol message.\n" );
        exit( 1 );
    }
    free( minica_server_response );
    minica_server_response = NULL;

    if( passive_client ) {
        handle_passive_client(s, pass, common_name,
                              *&key_block, *&cert_request );
    } else {
        handle_active_client( pass, common_name,
                              *&key_block, *&cert_request );
    }
}

/**
 * Get the signed certificate given the request.
 */
void
get_cert( ReliSock s, char *cert_request, char **signed_cert )
{
    int response_code = 0;
    char *error_message = NULL;
    
    //debug_print( "Sending cert to server.\n" );
    s.encode( ); /* 7 */
    s.code( cert_request );
    s.end_of_message();
    
    s.decode( ); /* 8 */
    if( ! ( ( s.code( response_code )) &&
            ( s.code( error_message )) &&
            ( s.code( *signed_cert )) &&
            ( s.end_of_message()))) {
        debug_print( "Got error receiving data from minica server.\n" );
    }
    if( response_code != MCA_RESPONSE_ALL_OK ) {
        debug_print( "Server generated error (8): %s\n", error_message );
    }

    if( (*signed_cert == NULL) || (strlen( *signed_cert ) == 0) ) {
        debug_print( "No certificate was returned.\n" );
        exit( 1 );
    }
}

/**
 * The protocol preface involves a version comparision.
 */
int
check_version_compatibility( ReliSock s )
{
    char * minica_client_version = NULL;
    char * minica_server_version = NULL;
    char * minica_server_response = NULL;
    int minica_server_status = 0;

    /* Protocol init. */
    s.encode( ); /* Numbers refer to protocol stages.  1 */
    minica_client_version = strdup( MCA_PROTOCOL_VERSION );
	if(!minica_client_version) {
		perror("strdup");
		exit( 1 );
	}
    if( ! ( (s.code( minica_client_version ))
            && (s.end_of_message( )) ) ) {
        debug_print( "Error sending version to server.\n" );
        exit( 1 );
    }

    s.decode( ); /* 2 */
    if( ! ( (s.code( minica_server_version ))
            && (s.code( minica_server_status ))
            && (s.code( minica_server_response ))
            && (s.end_of_message( )) ) ) {
        debug_print( "Error receiving version from server.\n" );
        exit( 1 );
    }
    
    switch( minica_server_status ) {
    case MCA_PROTOCOL_ABORT:
        debug_print( "Server sent protocol abort response.\n" );
        debug_print( "Client version: '%s'\nServer version: '%s'\n",
                 minica_client_version, minica_server_version );
        debug_print( "Server sent: '%s'\n", minica_server_response );
        exit( 1 );
        break;
    case MCA_PROTOCOL_PROCEED:
        debug_print( "Server sent: '%s'\n", minica_server_response );
        break;
    default:
        debug_print( "Server sent unknown protocol message.\n" );
        exit( 1 );
    }

    if( minica_client_version ) {
        delete [] minica_client_version;
        minica_client_version = NULL;
    }
    free( minica_server_version );
    minica_server_version = NULL;
    free( minica_server_response );
    minica_server_response = NULL;
    return TRUE;
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
    request_type  req_type = NONE;
    char          *key_block = NULL;
    char          *cert_request = NULL;
    char          *signed_cert = NULL;
    char          *pass = NULL;
//    char          *openssl_cnf = NULL;
//    char          *key_filename = NULL;
//    char          *request_filename = NULL;
//    char          *cert_filename = NULL;
//    int          passive_client = 0;
//    char *common_name = NULL;
    REQUEST_PROPERTIES req;
    // set defaults:
    req.key_filename = NULL;
    req.request_filename = NULL;
    req.cert_filename = NULL;
    req.common_name = NULL;
    req.passive_client = false;
    req.use_server_openssl_cnf = false;
    req.add_uuid = false;

    Daemon ca_daemon( DT_ANY ); // (MINICASERVER);//<128.105.175.112:54139>");
    ReliSock g,s;

    config(); // external

    req_type = parse_command_line( argc, argv, &req );
    /*&key_filename,
                                   &request_filename, &cert_filename,
                                   &passive_client, &common_name );*/
    reconfig( req.use_server_openssl_cnf ); // local

    if( req_type == CLIENT ) {
        pass = my_getpass_prompt(
            "Type password used to encrypt the private key: ");
        if( pass == NULL || ( (strlen( pass ) == 1) && (pass[0] == '\n')) ) {
            debug_print( "Password is null.\n" );
            exit( 1 );
        }
        if( strlen( pass ) < 4 ) {
            debug_print( "Password too short.\n" );
            exit( 1 );
        }
    } // else pass is null

    debug_print( "Connecting to MiniCA server.\n" );

    if(!s.connect( MiniCAHost )) {
        debug_print( "Failed to connect.\n");
        exit(1);
    }
    
    debug_print( "Starting command.\n" );
    ca_daemon.startCommand( DC_SIGN_CERT_REQUEST,(Sock *)&s );

    debug_print( "Checking version compatability.\n" );
    check_version_compatibility( s );

    debug_print( "Sending config file request status.\n" );
    maybe_transfer_openssl_cnf( s, req.use_server_openssl_cnf );

    get_key_csr( s, req.passive_client, pass, req.common_name, &key_block, &cert_request );

    //debug_print( "%s", cert_request );
    
    if( str_to_file( key_block, req.key_filename ) ) {
        debug_print(
            "Key generated OK, but can't save to file '%s'.  Aborting.\n",
            req.key_filename );
        exit( 1 );
    }
    if( str_to_file( cert_request, req.request_filename ) ) {
        debug_print(
            "Request generated OK, but can't save to file '%s'. Aborting.\n",
            req.request_filename );
        exit( 1 );
    }

    get_cert( s, cert_request, &signed_cert );
    
    if( str_to_file( signed_cert, req.cert_filename ) ) {
        debug_print( "Error writing signed certificate to file.\n" );
        exit( 1 );
    }
    debug_print( "Obtained certificate.\n" );

    if( pass != NULL ) {
        free( pass );
    }
    free( req.common_name );
    free( req.key_filename );
    free( req.request_filename );
    free( req.cert_filename );
    free( signed_cert );
    free( key_block );
    free( cert_request );
    exit( 0 );
}

