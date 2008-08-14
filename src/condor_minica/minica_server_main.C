/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department,
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

/* minica_server_main.C
 *
 * $Id$
 *
 * This file contains the bulk of the code used by the minica server
 * to handle signing requests.  This is a DaemonCore server, which
 * makes use of the authenticated connections provided by the CEDAR
 * library.
 *
 * Some of the functionality of the server is contained in the file
 * minica_common.C; that which is shared between the client and the
 * server, for example, the code which generates the private key
 * (in case a client which does not have openssl is allowed to have
 * a key created for it by the server).
 *
 * In addition, many of the constants used in this file are declared
 * in minica_common.h, including those used in the protocol shared
 * between client and server.
 *
 * This code relies on the availability of an openssl binary
 * executable somewhere on the server; the full path to that
 * executable is passed through the condor configuration file.  To
 * compile, it relies on the existence of the PCRE (Perl Compatible
 * Regular Expression) library, which is used to parse the
 * configuration file.
 *
 */

#include "condor_common.h"
#include "file_lock.h"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_string.h"
#include "minica_common.h"
#include "../condor_c++_util/Regex.h"
#include "ui_callback.h"

#define GET_SETTING( v, n )      if( v ) free( v ); v = param( n );

/* Lines in the policy file must not be longer than this.  If they
 * are, then the policy will be read incorrectly, after being broken
 * into chunks this size.
 */
#define MCA_MAX_LINE_LENGTH 200

/* These globals are passed through the condor config file -
 * see the function main_config() */
/* Path to the openssl executable. */
char * _mca_path_to_openssl = NULL;

/* Path to the openssl.cnf configuration file.  This is the same file
 * that gets sent back to the client if they request it. */
char * _mca_path_to_openssl_cnf = NULL;

/* A lock file - must be on local filesystem.  The lock file is used
 * to keep this program from stomping on itself.  See
 * sign_cert_request().
 */
char * _mca_ca_op_lock_file = NULL; 

/* The following globals and corresponding config file entries
 * are set once (at startup) and never changed.  This protects them...
 */
int _mca_ca_key_is_set = 0;
 
/* Path to the ca key file.  Note that although this gets set in the
 * config file, it doesn't get re-read... or does it?  TODO 
 */
char * _mca_ca_key_file = NULL;

/* The password for that file, provided at startup */
char * _mca_ca_password = NULL;

/* An OpenSSL style specification of how the server administrator will
 * provide the password for that file.  See openssl(1), section "Pass
 * Phrase Arguments", for more info.  Null, or unspecified in the
 * config file, means from standard in.  Right?  TODO
 */
char * _mca_ca_key_passin = NULL;

/* Policy is stored in a list strucuture. */
typedef struct policy_entry {
    char * policy_line;
    struct policy_entry * next;
} Policy;

// Global variables.
// Sinful string of daemon.
char      *MiniCAHost = NULL;
// used by Daemon Core
char      *mySubSystem = "MINICA";

// Pointer to singing policy structure
Policy    *policy = NULL;

/* This is the policy file */
char * _mca_signing_policy_file = NULL;

/* The server and the client report errors differently, but both need
 * to print errors...  So each has a different implementation of this
 * function.
 */
void
debug_print ( char * fmt, ... )
{
    char buf[ MCA_ERROR_BUFSIZE ]; 
    va_list args;
    va_start( args, fmt );
    vsnprintf( buf, MCA_ERROR_BUFSIZE, fmt, args );
    dprintf( D_ALWAYS, "%s", buf );
}

/* This is the heart of the policy checking mechanism.  Returns true
 * if the string matches the pattern.  Lines in the file provide
 * the pattern, strings are assembled from user data.
 * TODO: test
 */
int
check_regex_match( char * pat, char * str )
{
    bool rc = FALSE;
    Regex re = Regex( );
    const char *err;
    int err_offset = 0;
	rc = re.compile( pat, &err, &err_offset );
	if( rc == false ) {
        debug_print( "Error compiling pcre in check_regex_match." );
        debug_print( "Error in '%s' at %d: '%s'.", pat, err_offset, err );
		return 0; // TODO: is this the right thing to do?
    }
	rc = re.match(str);
    if( rc == FALSE ) {
        debug_print( "Fail: %s %s\n", pat, str );
        return 0;
    } else {
        debug_print( "Match: %s %s\n", pat, str );
        return 1;
    }
}

/*
 * Utility function for listing the signing policy.
 */
void
list_signing_policy( Policy * pol )
{
    Policy * cur = pol;
    while ( 1 ) {
        if( cur == NULL ) {
            return;
        }
        debug_print( "Policy element: %s\n", cur->policy_line );
        cur = cur->next;
    }
}

/*
 * Given a policy and a user attempting to obtain a certificate for
 * with a particular subject line, return true if this is allowed
 * under the policy.
 */
int
check_signing_policy( Policy * pol, char * user, char * subject )
{
    char buf[ MCA_MAX_SMALL_FILE_SIZE ];

    // The user and subject information is concatenated with a ':'.
    if( index( user, ':' ) != NULL ) {
        debug_print( "User authentication information contains illegal ':'.\n" );
        debug_print( "The user is: '%s'\n", user );
        return 0;
    }
    if( index( subject, ':' ) != NULL ) {
        debug_print( "Certificate request subject contains illegal ':'\n" );
        debug_print( "The subject is: '%s'\n", subject );
    }

    sprintf( buf, "%s:%s", user, subject );
    Policy * cur = pol;
    while( cur != NULL ) {
        if(check_regex_match( cur->policy_line, buf ) ) {
            // debug_print( "Got match: %s %s\n", user, subject );
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}

/*
 * Signing policy list destruction.
 */
void
free_signing_policy( Policy * pol )
{
    Policy * cur = pol;
    Policy * next;
    while( 1 ) {
        if( cur == NULL ) {
            return;
        }
        next = cur->next;
        free( cur->policy_line );
        free( cur );
        cur = next;
    }
}   

/*
 * Read the signing policy from the file.
 *
 * The returned structure is a list of pairs: the first
 * pair is a regex for users, and the second is a regex for
 * subjects.
 *
 * TODO: provide external utility for validating policy.  
 */
Policy *
read_signing_policy( )
{
    FILE       * fp;
    int          done;
    int          len;
    char       * line_ptr;
    char       * line;
    Policy     * policy_head = NULL;
    Policy     * policy_cur;
    Policy     * policy_prev = NULL;

    if( (fp = safe_fopen_wrapper( _mca_signing_policy_file, "r" )) ) {
        done = 0;
        while( 1 ) {
            line_ptr = line = NULL;

            // malloc space for line
            line_ptr = (char *) malloc( MCA_MAX_LINE_LENGTH+1 );
            if( line_ptr == 0 ) {
                debug_print( "Error in malloc.\n" );
                exit( 1 );
            }
            
            // get line
            fgets( line_ptr, MCA_MAX_LINE_LENGTH, fp );

            // check if zero length - EOF: return head
            len = strlen( line_ptr );
            if( len <= 0 ) {
                // fprintf( stderr, "No match found.\n" );
                return policy_head;
            }

            // strip off comments
            line = strstr( line_ptr, "#" );
            if( line != NULL ) {
                line[0] = '\0';
            }

            // change newline on end to end of line
            line_ptr[len-1] = '\0';
            
            // ignore whitespace at the beginning of the line
            line = line_ptr;
            while( line[0] == ' ' || line[0] == '\t' ) {
                line++;
            }

            // skip blank lines
            if( strlen( line_ptr ) > 0 ) {

                policy_cur = (Policy *)malloc( sizeof( Policy ) );
                if( policy_cur == NULL ) {
                    debug_print( "Error mallocing policy.\n" );
                    exit( 1 );
                }

                // If the policy list is empty, make this the first entry.
                if( policy_head == NULL ) {
                    policy_head = policy_cur;
                }

                // Fill the structure.
                policy_cur->policy_line = line_ptr;
                policy_cur->next = NULL;
                if( policy_prev != NULL ) {
                    policy_prev->next = policy_cur;
                }
                policy_prev = policy_cur;
            }
        }
    }
    return NULL;
}

/*
 * Given a certificate request, extract the subject line.
 *
 * TODO: efficiency could be improved if this were done without
 * forking.
 */
char *
get_subject_from_x509_req( char * csr )
{
    int     pid, status;

    char    *argv[] = { _mca_path_to_openssl, "req", "-noout", "-text",
                        NULL
    };
    int     pi_resp[2];
    int     pi_req[2];
    char    *csr_text = (char *)malloc( MCA_MAX_SMALL_FILE_SIZE );
    ssize_t size = 0, csr_size = 0;
    char    *subject = NULL;
    char    *subject_end = NULL;
    char    *subject_rv = NULL;

    if(csr_text == NULL) {
        debug_print( "Error in malloc.\n" );
        debug_print( "In get_subject_from_x509_req" );
        free( csr_text );
        return NULL;
    }

    if( _mca_path_to_openssl == NULL ) {
        debug_print( "Error: _mca_path_to_openssl not defined.\n" );
        return NULL; 
    }

    if( pipe( pi_resp ) || pipe( pi_req ) ) {
        debug_print( "Error creating pipe.\n" );
        debug_print( "In get_subject_from_x509_req" );
        free( csr_text );
        return NULL;
    }

    print_argv_to_fork( argv );
    pid = fork( );
    if( pid == -1 ) {
        debug_print( "Error forking in get_subject_from_x509_req.\n" );
        free( csr_text );
        return NULL;
    }
    if( pid == 0 ) { // child
        // replace standard out with pipe.

        // child writes, so close reader end in this process.
        close( pi_resp[0] );
        // make stdout be our pipe
        dup2( pi_resp[1], 1 );
        // tidily close our pipe for the execed program.
        close( pi_resp[1] );

        // replace standard in with pipe.

        // child reads, so close writer end in this process.
        close( pi_req[1] );
        // make standard in be our pipe
        dup2( pi_req[0], 0 );
        // close our pipe for the execed program
        close( pi_req[0] );
        
        execve( _mca_path_to_openssl, argv, NULL );
        debug_print( "Error execing!\n" );
        free( csr_text );
        return NULL;
    }

    // Parent

    // We're the writer, so close the reader end.
    close( pi_req[0] );
    csr_size = strlen( csr );
    size = write( pi_req[1], csr, csr_size );
    if( size < csr_size ) {
        debug_print( "Couldn't write to child.\n" );
        debug_print( "In get_subject_from_x509_req" );
        free( csr_text );
        return NULL;
    }
    close( pi_req[1] );

    // We're the reader, close the writer end of the pipe.
    close( pi_resp[1] );

    // This needs to be modified so we repeat to get the whole thing.
    csr_size = read( pi_resp[0], csr_text, MCA_MAX_SMALL_FILE_SIZE );
    if( csr_size < 1 ) {
        debug_print( "Didn't read anything.\n" );
        debug_print( "In get_subject_from_x509_req" );
        free( csr_text );
        return NULL;
    }
    close( pi_resp[0] );

    subject = strstr( csr_text, "Subject: " );
    if( subject == NULL ) {
        debug_print( "Error: didn't find 'Subject: ' in csr: \n%s\n",
                 csr );
        free( csr_text );
        return NULL;
    }
    subject_end = strstr( subject, "\n" );
    subject_rv = (char *)malloc( subject_end-subject+2-9 );
    if( subject_rv == NULL ) {
        debug_print( "Couldn't malloc for subject return value.\n" );
        debug_print( "In get_subject_from_x509_req" );
        free( csr_text );
        return NULL;
    }       
    status = snprintf( subject_rv, subject_end - subject+1-9, "%s", subject+9 );
    if( status < 1 ) {
        debug_print( "In get_subject_from_x509_req" );
        debug_print( "Couldn't copy string\n" );
        free( csr_text );
        free( subject_rv );
        return NULL;
    }
    free( csr_text );
    do {
        if( waitpid(pid, &status, 0) == -1 ) {
            if( errno != EINTR ) {
                debug_print( "In get_subject_from_x509_req" );
                debug_print( "Problem, can't wait right.\n" );
                free( subject_rv );
                return NULL;;
            }
        } else {
            return( subject_rv );
        }
    } while( 1 );
}

/*
 * This is the routine that actually signs the request.
 *
 * TODO: this needs to be modified to allow unique_subject = no in the
 * openssl.cnf file.  There are needs for this, but right now we don't
 * allow it.  Why not?  Because we want to enforce this for the users
 * but not for hosts.  Moreover, right now, we get this info by
 * reading standard error, and enforce it for both hosts and users.
 * When we change unique_subject to no, openssl won't complain for us,
 * so we need to either allow users to do this themselves as well.
 * Hmm.  However, at present, it's disallowed.
 *
 * References: look for unique_subject in openssl changes file, also
 * man page for openssl ca.
 */
char *
sign_req( char *csr, int *err_code )
{
    int        pid, status;
    char       csr_file[] = "/tmp/srv_csrfileXXXXXX";
    char       ca_pass_pipe[20]; // String for passing password filedescriptor:
    // Will be something like fd:%d.
    char       *argv[] = { _mca_path_to_openssl, "ca", "-config",
                           _mca_path_to_openssl_cnf, "-batch", "-passin",
                           ca_pass_pipe,
                           "-infiles", csr_file,
                           NULL
    };
    int        cert_size, csr_size, size;
    int        err_size;
    char       *cert = NULL;
    char       *err_text = NULL;
    int        pi_resp[2], pe_resp[2], ca_resp[2];
    int        tmpfile = 0; 
    int        offset;

    *err_code = MCA_ERROR_NOERROR;

    cert = (char *)malloc( MCA_MAX_SMALL_FILE_SIZE );
    err_text = (char *)malloc( MCA_MAX_SMALL_FILE_SIZE );
    if( cert == NULL || err_text == NULL ) {
        debug_print( "Error in malloc.\n" );
        debug_print( "In sign_req" );
        *err_code = MCA_ERROR_MALLOC;
        goto return_error_nofree;
    }

    tmpfile = mkstemp( csr_file );
    if( tmpfile == -1 ) {
        debug_print( "In sign_req" );
        debug_print( "Couldn't create temp file.\n" );
        *err_code = MCA_ERROR_TEMP_FILE;
        goto return_error_free;
    }

    csr_size = strlen( csr );
    size = write( tmpfile, csr, csr_size );
    if( size < csr_size ) {
        debug_print( "In sign_req" );
        debug_print( "Error writing csr file.\n" );
        *err_code = MCA_ERROR_WRITE_FILE;
        goto return_error_unlink_free;
    }
    close( tmpfile );

    if( pipe( pi_resp ) || pipe( pe_resp ) || pipe( ca_resp ) ) {
        debug_print( "In sign_req" );
        debug_print( "Error creating pipe.\n" );
        *err_code = MCA_ERROR_PIPE;
        goto return_error_unlink_free;
    }

    sprintf( ca_pass_pipe, "fd:%d", ca_resp[0] );

    print_argv_to_fork( argv );
    pid = fork();

    if(pid == -1) {
        debug_print( "In sign_req" );
        debug_print( "Error forking.\n" );
        *err_code = MCA_ERROR_FORK;
        goto return_error_unlink_free;
    }
    if(pid == 0) { // child
        // replace standard out with pipe.
        close( pi_resp[0] ); // child writes, so close reader end in this process.
        dup2( pi_resp[1], 1 ); // make stdout be our pipe
        close( pi_resp[1] ); // tidily close our pipe for the execed program.

        //fclose( stderr );
        close( pe_resp[0] );
        dup2( pe_resp[1], 2 );
        close( pe_resp[1] );

        close( ca_resp[1] );
        
        execve( _mca_path_to_openssl, argv, NULL );
        debug_print( "Error execing!\n" );
        exit( 127 );
    }

    // We're the reader, close the writer end of the pipe.
    close( pi_resp[1] );
    close( pe_resp[1] );
    close( ca_resp[0] );

    // don't try too hard
	// debug_print("Here's where we're writing the password: '%s'\n", _mca_ca_password);
    offset = write( ca_resp[1], _mca_ca_password, strlen( _mca_ca_password ) );
    if( offset != (int) strlen( _mca_ca_password ) ) {
        debug_print( "Error writing ca password.\n" );
        exit( 1 );
    }
	offset = write( ca_resp[1], "\n", 1 );
	if( offset != 1 ) {
		debug_print( "Error writing ca password newline.\n" );
		exit( 1 );
	}
    
    err_size = 0;
    offset = 0;
    while( ( offset = read( pe_resp[0], err_text+err_size,
                            MCA_MAX_SMALL_FILE_SIZE ) ) > 0 ) {
        err_size += offset;
    }
    err_text[err_size] = '\0';

    cert_size = 0;
    offset = 0;
    while((offset = read( pi_resp[0], cert+cert_size,
                          MCA_MAX_SMALL_FILE_SIZE )) > 0) {
        cert_size += offset;
    }
    cert[cert_size] = '\0';

    // debug_print( "Read %d.\n", cert_size );

    if( offset < 0 ) {
        debug_print( "Error reading.\n" );
        *err_code = MCA_ERROR_PIPE_READ;
        /* Still send cert back - don't free.
          goto return_error_unlink_free;
        */
    }

    close( pi_resp[0] );
    close( pe_resp[0] );

	//debug_print("All done, just waiting.\n");
    if( waitpid(pid, &status, 0) == -1 ) {
        debug_print( "In sign_req" );
        debug_print( "Problem, can't wait right.\n" );
        *err_code = MCA_ERROR_WAIT;
    }
    if( WIFEXITED( status ) ) {
        debug_print( "openssl exited with code %d.\n",
                 WEXITSTATUS( status ) );
        if( WEXITSTATUS( status ) != 0 ) {
            debug_print( "Error:\n%s", err_text );
            if( NULL != strstr( err_text, "TXT_DB error number 2" )) {
                debug_print( "Certificate request conflicts with existing cert (%s).\n", cert );
                *err_code = MCA_ERROR_OPENSSL_INDEX_CLASH;
            }
        }
    }
    if( unlink( csr_file ) ) {
        debug_print( "In sign_req" );
        debug_print( "Couldn't unlink file!\n" );
    }
    return( cert );

  return_error_unlink_free:
    if( unlink( csr_file ) ) {
        debug_print( "In sign_req" );
        debug_print( "Couldn't unlink file!\n" );
    }
  return_error_free:
    free( cert );
  return_error_nofree:
    return NULL;
}

/**
 * The protocol preamble involves an agreement on protocol version.
 */
int
check_version_compatibility( Stream *sock )
{
    char *minica_server_version = NULL;
    char *minica_client_version = NULL;
    int response_code = 0;
    char *error_message;
    
    sock->decode( ); /* Protocol stage 1 */
    if( ! (sock->code( minica_client_version )) ) {
        debug_print( "check_version_compatability() received bad input.\n" );
        return FALSE;
    }
    if( ! (sock->end_of_message( )) ) {
        debug_print( "check_version_compatability() received bad input.\n" );
        free( minica_client_version );
        return FALSE;
    }
    
    sock->encode( ); /* 2 */
    minica_server_version = strdup( MCA_PROTOCOL_VERSION );
    if( minica_server_version == NULL ) {
		perror("strdup");
        free( minica_client_version );
        return FALSE;
    }
    if( ! (sock->code( minica_server_version )) ) {
        debug_print( "check_version_compatability() error sending version.\n" );
        free( minica_server_version );
        free( minica_client_version );
        return FALSE;
    }
    
    /* A more detailed version check might be appropriate. */
    if( ! strcmp( minica_server_version, minica_client_version ) ) {
        response_code = MCA_PROTOCOL_PROCEED;
        error_message = strdup( "Proceed version." );
        if( error_message == NULL ) {
			perror("strdup");
            free( minica_client_version );
            free( minica_server_version );
            return FALSE;
        }
        if( ! ( (sock->code( response_code ))
                && (sock->code( error_message ))
                &&( sock->end_of_message( )) ) ) {
            debug_print( "check_version_compatability() error in protocol init.\n" );
            debug_print( "Client: '%s'\nServer: '%s'\n",
                     minica_client_version, minica_server_version );
            free( error_message );
            free( minica_client_version );
            free( minica_server_version );
            return FALSE;
        }
        free( error_message );
    } else {
        response_code = MCA_PROTOCOL_ABORT;
        /* No error checking since we'll do the same thing anyway. */
        sock->code( response_code );
        error_message = strdup("Versions don't match.");
        sock->code( error_message );
        sock->end_of_message( );
        debug_print( "check_version_compatability() aborting due to version mismatch.\n" );
        free( error_message );
        free( minica_client_version );
        free( minica_server_version );
        return FALSE;
    }
    free( minica_client_version );
    free( minica_server_version );
    return TRUE;
}

/**
 * Passive clients require the server to perform key and request generation.
 */
int
handle_passive_client( Stream *sock )
{
    char    *pass = NULL;
    char    *common_name = NULL;
    int     passive_client = 0; /* really a bool */
    int     response_code = 0;
    char    *csr = NULL;
    char    *key_block = NULL;
    int     error_code = 0;
    char    *error_message = NULL;

    debug_print( "Entering handle_passive_client.\n" );

    sock->decode( ); /* 3 */
    if( ! ( (sock->code( passive_client ))
            && (sock->end_of_message( )) ) ) {
        debug_print( "handle_passive_client() received bad input.\n" );
        return FALSE;
    }

    /* Here's where you can reject all passive clients. */
    sock->encode( ); /* 4 */
    if( ! passive_client ) {
        response_code = MCA_PROTOCOL_PROCEED;
        error_message = strdup( "Proceed active client." );
        debug_print( "Telling active client to proceed.\n" );
        if( ! ( (sock->code( response_code ))
                && (sock->code( error_message ))
                && (sock->end_of_message( )) ) ) {
            debug_print( "handle_passive_client() error sending passive accept.\n" );
            free( error_message );
            return FALSE;
        }
        free( error_message );
    } else {
        if( (! MCA_ACCEPT_PASSIVE_CLIENTS) && passive_client ) {
            response_code = MCA_PROTOCOL_ABORT;
            error_message = strdup( "No passive connections allowed." );
            // This time we're not so careful because we're exiting soon anyway.
            sock->code( response_code );
            sock->code( error_message );
            sock->end_of_message( );
            free( error_message );
            debug_print( "handle_passive_client() won't accept passive clients.\n" );
            return FALSE;
        }
        if( passive_client && MCA_ACCEPT_PASSIVE_CLIENTS ) {
            response_code = MCA_PROTOCOL_PROCEED;
            error_message = strdup( "Proceed passive client." );
            if( ! ( (sock->code( response_code ))
                    && (sock->code( error_message ))
                    && (sock->end_of_message( )) ) ) {
                debug_print( "handle_passive_client() error sending passive accept.\n" );
                free( error_message );
                return FALSE;
            }
            free( error_message );
        }

        // Now it's up to us to generate the cert for the client.
        sock->decode( ); /* 5 */
        if( ! ( (sock->code( pass ))
                && (sock->code( common_name ))
                && (sock->end_of_message( )) ) ) {
            debug_print( "handle_passive_client() received bad input (5).\n" );
            if( pass != NULL ) {
                free( pass );
                pass = NULL;
            }
            if( common_name != NULL ) {
                free( common_name );
                common_name = NULL;
            }
            return FALSE;
        }

        // set response_code, key block, csr.
        debug_print( "Generating client key.\n" );
        response_code = MCA_RESPONSE_ALL_OK; // A-OK
        key_block = repeat_gen_rsa_key( pass, MCA_MAX_KEYGEN_ATTEMPTS,
                                        MCA_KEYGEN_WAIT_TIME_SECS,
                                        &error_code );
        if( error_code > MCA_ERROR_NOERROR ) {
            debug_print( "Error generating key: %s\n",
                     get_mca_error_message( error_code ) );
        }
        if( key_block == NULL ) {
            response_code = MCA_RESPONSE_KEY_FAILED;
        } else {
            debug_print( "Generating client crt.\n" );
            csr = gen_signing_request( key_block,
                                       common_name, pass, &error_code );
            if( error_code > MCA_ERROR_NOERROR ) {
                debug_print(
                         "Error generating certificate request: %s\n",
                         get_mca_error_message( error_code ) );
            }
            if( csr == NULL ) {
                response_code = MCA_RESPONSE_CSR_FAILED;
            }
        }
        switch ( response_code ) {
        case MCA_RESPONSE_ALL_OK:
            error_message = strdup( "All OK." );
            break;
        case MCA_RESPONSE_KEY_FAILED:
            error_message = strdup( "Key generation failed." );
            break;
        case MCA_RESPONSE_CSR_FAILED:
            error_message = strdup(
                "Error generating certificate signing request." );
            break;
        default:
            debug_print( "Sanity check.  Unknown response code." );
            error_message = NULL;
        }
        if( error_message == NULL ) {
            free( key_block );
            free( csr );
            free( pass );
            free( common_name );
            return FALSE;
        }
        //debug_print( "Sending key and csr to client:\n%s\n%s\n%s.\n", error_message, key_block, csr );
        sock->encode( ); /* 6 */
        if( ! ( (sock->code( response_code ))
                && (sock->code( error_message ))
                && (sock->code( key_block ))
                && (sock->code( csr ))
                && (sock->end_of_message( )) ) ) {
            debug_print( "handle_passive_client() error sending.\n" );
        }
        if( response_code != MCA_RESPONSE_ALL_OK ) {
            debug_print(
                     "Passive key/request generation failed.  Aborting.\n" );
            free( error_message );
            free( key_block );
            free( csr );
            free( pass );
            free( common_name );
            return FALSE;
        }
        free( error_message );
        free( key_block );
        free( csr );
        free( pass );
        free( common_name );
        return TRUE;
    } /* end if passive_client */
    return TRUE;
}

/**
 * The last phase of the protocol is to actually sign the certificate request.
 */
int
sign_cert( Stream *sock, char *user )
{
    char    *cert = NULL;
    char    *subject = NULL;
    char    *csr = NULL;
    int     response_code = 0;
    int     error_code = 0;
    char    *error_message = NULL;
    
    sock->decode(); /* 7 */
    debug_print( "Receiving csr from client.\n" );
    if( ! ( (sock->code( csr )) &&
            (sock->end_of_message( )) ) ) {
        debug_print( "sign_cert() received bad input (7).\n" );
        return FALSE;
    }
    if( csr == NULL ) {
        debug_print(
                 "sign_cert() called with error (NULL) input.\n" );
        return FALSE;
    }

    subject = get_subject_from_x509_req( csr );
    if( subject == NULL ) {
        debug_print( "sign_cert() can't get subject for cert '%s'\n", csr );
        response_code = MCA_RESPONSE_CERT_FAILED;
    } else {
        debug_print( "Received signing request with subject: '%s'\n", subject );
        
        if( ! check_signing_policy( policy, user, subject ) ) {
            debug_print( "Policy check failed.\n" );
            debug_print(
                     "Not signing for %s with subject %s.\n", user, subject );
            response_code = MCA_RESPONSE_CERT_POLICY_FAILED;
        } else {
            cert = sign_req( csr, &error_code );
            if( error_code > MCA_ERROR_NOERROR ) {
                debug_print( "Error signing certificate request: %s\n",
                         get_mca_error_message( error_code ) );
            }
            if( (cert == NULL) || (strlen( cert ) == 0) ) {
                response_code = MCA_RESPONSE_CERT_FAILED;
                if( cert != NULL ) {
                    free( cert );
                    cert = NULL;
                }
            } else {
                response_code = MCA_RESPONSE_ALL_OK;
            }
        }
    }
    if( cert == NULL ) {
        cert = (char *)malloc( 2 );
        if( cert == NULL ) { // Can't malloc to send null string back to client, abort
            free( csr );
            free( subject );
            return FALSE;
        }
        cert[0] = '\0';
    }
    switch ( response_code ) {
    case MCA_RESPONSE_ALL_OK:
        error_message = strdup( "All OK." );
        break;
    case MCA_RESPONSE_CERT_FAILED:
        error_message = strdup( "Certificate signing failed." );
        break;
    case MCA_RESPONSE_CERT_POLICY_FAILED:
        error_message = strdup( "Policy check failed, certificate not signed." );
        break;
    default:
        debug_print( "Sanity check.. Unknown response code." );
        error_message = NULL;
    }
    if( error_message == NULL ) {
        free( csr );
        free( cert );
        free( subject );
        return FALSE;
    }
    sock->encode( ); /* 8 */
    if( ! ( (sock->code( response_code )) &&
            (sock->code( error_message )) &&
            (sock->code( cert )) &&
            (sock->end_of_message( )))) {
        debug_print( "sign_cert() error sending.\n" );
    }

    free( error_message );
    free( subject );
    free( csr );
    //free( user );
    if( cert == NULL || strlen( cert ) == 0 ) {
        if( cert != NULL ) {
            free( cert );
        }
        return FALSE;
    } else {
        free( cert );
        return TRUE;
    }
}

/**
 * Wait for a lock on the lock file, handling errors.
 */
int
get_lock( int *lock_fd ) {
    /* I couldn't make heads or tails out of the condor portable flock
     * mystery.  This seems to work on linux.
     */
    debug_print(
             "Attempting to get lock on lock file '%s'...\n",
             _mca_ca_op_lock_file );
    *lock_fd = safe_open_wrapper( _mca_ca_op_lock_file, 
                        O_CREAT|O_WRONLY|O_TRUNC,
                        S_IRUSR|S_IWUSR );
    if( *lock_fd == -1 ) {
        debug_print( "In get_lock" );
        debug_print( "Error creating lock file.\n" );
        return FALSE;
    }
    debug_print( "Opened lock file...\n" );
    
    if( flock( *lock_fd, LOCK_EX ) == -1 ) {
        debug_print( "In get_lock" );
        debug_print( "Error creating lock on lock file.\n" );
        if( close( *lock_fd ) == -1 ) {
            debug_print( "In gen signing request" );
            debug_print( "Fatal: error closing lock file.\n" );
            return FALSE;
        }
        return FALSE;
    }
    debug_print( "Got lock on lock file %d.\n", *lock_fd );
    return TRUE;
}

/**
 * Release the lock.
 */
int
release_lock( int lock_fd )
{
    //debug_print( "Releasing lock.\n" );
    if( flock( lock_fd, LOCK_UN ) == -1 ) {
        // We can't sign anything else, so we exit.
        debug_print( "In release_lock" );
        debug_print( "Error removing lock from lock file.\n" );
        return FALSE;
    }
    if( close( lock_fd ) == -1 ) {
        debug_print( "In release_lock" );
        debug_print( "Error closing lock file.\n" );
        return FALSE;
    }
    //debug_print( "Released lock.\n" );
    return TRUE;
}

/*
 * Read an openssl.cnf file from disk, and send it to the client.
 */
int
send_openssl_cnf( Stream *sock )
{
    char *openssl_cnf;
    int error = 0;

    debug_print(" Entering send_openssl_cnf.\n" );
    
    openssl_cnf = file_to_str( _mca_path_to_openssl_cnf, &error );
    if( openssl_cnf == NULL ) {
        debug_print( "send_openssl_cnf error: '%s'\n", get_mca_error_message( error ) );
        return FALSE;
    }
    sock->encode( );
    if( ! (sock->code( openssl_cnf )) ) {
        debug_print( "send_openssl_cnf() error sending file.\n" );
        free( openssl_cnf );
        return FALSE;
    }
    if( ! (sock->end_of_message( )) ) {
        debug_print( "send_openssl_cnf() error communicating.\n" );
        free( openssl_cnf );
        return FALSE;
    }
    free( openssl_cnf );
    debug_print("Sent openssl.cnf to client successfully.\n" );
    return TRUE;
}

/*
 * If the client wants the openssl.cnf file, it will tell us.
 * Otherwise skip sending it.
 */
int
maybe_send_openssl_cnf( Stream *sock )
{
    int xfer = FALSE;
    debug_print( "Transfering config file status.\n" );
    sock->decode( );
    if( ! ( (sock->code( xfer ))
            && (sock->end_of_message( )) ) ) {
        debug_print( "Error getting config file status.\n" );
        return FALSE;
    }
    if( xfer ) {
        return send_openssl_cnf( sock );
    }
    return TRUE;
}

/*
 * This is the main subroutine called by daemon core in response to a
 * client request.
 *
 * Check that the request is allowable under our policy, and then sign
 * it and return it to the client.
 */
int
sign_cert_request(Service *, int, Stream *sock)
{
    char    *user;
    int     pid;
    int     lock_fd = -1;

    ReliSock * socket = (ReliSock*)sock;
    
	// TODO: This should probably be fixed!  ASK ZACH!!!
    // Authenticate
//    if( !socket->isAuthenticated( ) ) {
//        char * p = SecMan::getSecSetting( "SEC_%s_AUTHENTICATION_METHODS",
//                                          "READ" );
        MyString methods;
//        if (p) {
//            methods = p;
//            free (p);
//        } else {
            methods = SecMan::getDefaultAuthenticationMethods();
//        }
        CondorError errstack;
		if( ! socket->authenticate(methods.Value(), &errstack, 60) ) {
            dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
            // goto EXIT;
        }
		//  }   
	if(!(socket->getFullyQualifiedUser())) {
        debug_print( "sign_cert_request() Error: couldn't get user.\n" );
        return FALSE;
    }
    user = strdup( socket->getFullyQualifiedUser() );
	if(user == NULL) {
		perror("strdup");
		return FALSE;
	}
    
    debug_print( "sign_cert_request: Connection initiated by: '%s'\n", user );
    
    /* Let the daemon_core get back to its event loop */
    pid = fork();
    if( pid == -1 ) { // can't fork.  
        debug_print( "In sign_cert_request" );
        debug_print( "Error forking.\n" );
        free( user );
        return FALSE;
    }
    if( pid != 0) {
        debug_print( "parent returning to main event loop.\n" );
        return TRUE;
    }

    if( ! get_lock( &lock_fd ) ) {
        free( user );
        exit( 1 );
    }
    // debug_print( "Got lock on lock file %d..\n", lock_fd );

    if( check_version_compatibility( sock )
        && maybe_send_openssl_cnf( sock )
        && handle_passive_client( sock )
        && sign_cert( sock, user ) ) {
        release_lock( lock_fd );
        free( user );
        debug_print( "completed with success, exiting child.\n" );
        exit( 0 );
    }
    release_lock( lock_fd );
    free( user );
    debug_print( "completed with failure, exiting child.\n" );
    exit( 1 ); //DC_Exit( 1 );
}

//-------------------------------------------------------------
void
usage( char * MyName )
{
    debug_print( "Usage: %s [option]\n", MyName );
    debug_print( "  where [option] is one of:\n" );
    debug_print( "    -m filename  (sinful address of daemon is written to filename)\n" );
    
    DC_Exit( 1 );
}

//-------------------------------------------------------------


/* TODO: This might cause problems if a protocol were GSI2K and
   we tested for GSI.  We should tokenize on commas or something...
*/
int
param_contains( char *name, char *needle )
{
    char *tmp = NULL;
    tmp = param( name );
    int rv = 0;
    if( strstr( tmp, needle ) ) {
        rv = 1;
    } else {
        debug_print( "'%s' does not contain '%s'; it is '%s'.\n",
                     name, needle, tmp );
    }
    free( tmp );
    return rv;
}

int
param_is( char *name, char *needle )
{
    char *tmp = NULL;
    tmp = param( name );
    int rv = 0;
    if( ! strncmp( needle, tmp, strlen( needle ) ) ) {
        rv = 1;
    } else {
        debug_print( "'%s' is not '%s'; it is '%s'.\n",
                     name, needle, tmp );
    }
    free( tmp );
    return rv;
}

int 
main_config( bool is_full )
{
    //char *tmp = NULL;
    int safe = 0;
    
	debug_print( "main_config() called\n" );
    
    GET_SETTING( _mca_signing_policy_file, "MCA_SIGNING_POLICY_FILE" );
    GET_SETTING( _mca_path_to_openssl_cnf, "MCA_PATH_TO_OPENSSL_CNF" );
    GET_SETTING( _mca_path_to_openssl, "MCA_PATH_TO_OPENSSL" );
    GET_SETTING( _mca_ca_op_lock_file, "MCA_CA_OP_LOCK_FILE" );
    /* Only set these ones the first time the config file is read. */
    if( ! _mca_ca_key_is_set ) {
        GET_SETTING( _mca_ca_key_file, "MCA_CA_KEY_FILE" );
        GET_SETTING( _mca_ca_key_passin, "MCA_CA_KEY_PASSIN" );
    }
    _mca_ca_key_is_set = 1;

    if( ! ( _mca_signing_policy_file &&
            _mca_path_to_openssl_cnf &&
            _mca_path_to_openssl &&
            _mca_ca_key_file &&
            _mca_ca_op_lock_file ) ) {
        debug_print( "configuration file must contain:\n" );
        if( ! _mca_signing_policy_file ) {
            debug_print( "MCA_SIGNING_POLICY_FILE - the location of the signing policy\n" );
        }
        if( ! _mca_path_to_openssl_cnf ) {
            debug_print(
                     "MCA_PATH_TO_OPENSSL_CNF - the location of the openssl config file\n" );
        }
        if( ! _mca_path_to_openssl ) {
            debug_print(
                     "MCA_PATH_TO_OPENSSL - the location of the openssl executable\n" );
        }
        if( ! _mca_ca_key_file ) {
            debug_print(
                     "MCA_CA_KEY_FILE - the path to the encrypted ca key file\n" );
        }
        if( ! _mca_ca_op_lock_file ) {
            debug_print(
                     "MCA_CA_OP_LOCK_FILE - the lock file (anywhere protected; not nfs)\n" );
        }
    
        DC_Exit( 1 );
        return FALSE;
    }

    
    /* Our policy: require a, e, and i.  authentication methods must
     * include gsi or kerberos.  encryption must have 3DES or
     * BLOWFISH.
     * TODO: Zach suggests that this might be done through the sock api, but
     * I'm not sure what "isAuthenticated" means... 
     */
    
/* Shoot yourself!

    if( param_is( "SEC_DEFAULT_AUTHENTICATION", "REQUIRED" ) &&
        param_is( "SEC_DEFAULT_ENCRYPTION", "REQUIRED" ) &&
        param_is( "SEC_DEFAULT_INTEGRITY", "REQUIRED" ) &&
        ( ( ! param_contains( "SEC_DEFAULT_AUTHENTICATION_METHODS", "FS" ) ) &&
          ( ! param_contains( "SEC_DEFAULT_AUTHENTICATION_METHODS", "ANONYMOUS" ) ) &&
          ( ! param_contains( "SEC_DEFAULT_AUTHENTICATION_METHODS", "CLAIMTOBE" ) ) )
        &&
        ( param_contains( "SEC_DEFAULT_CRYPTO_METHODS", "3DES" ) |
          param_contains( "SEC_DEFAULT_CRYPTO_METHODS", "BLOWFISH" ) ) ) {
    safe = 1;
    }

    if( safe == 0 ) {
        debug_print( "Unsafe config settings.\n" );
       if( !param_is( "MCA_ALLOW_INSECURE_CA", "YES" ) ) {
           DC_Exit( 1 );
       } else {
           debug_print( "WARNING: Continuing.  MCA_ALLOW_INSECURE_CA=YES\n" );
       }
    }
*/
    
    if( policy ) {
        free_signing_policy( policy );
        policy = NULL;
    }
    policy = read_signing_policy( );
    if( policy == NULL ) {
        debug_print( "read_signing_policy returns null.  Exiting.\n" );
        DC_Exit( 1 );
        return FALSE;
    }
    debug_print( "Configuration completed successfully.\n" );
    debug_print( "Configuration summary:\n" );
    debug_print( "MCA_SIGNING_POLICY_FILE: %s\n", _mca_signing_policy_file );
    debug_print( "MCA_PATH_TO_OPENSSL: %s\n", _mca_path_to_openssl );
    debug_print( "MCA_PATH_TO_OPENSSL_CNF: %s\n", _mca_path_to_openssl_cnf );
    debug_print( "MCA_CA_OP_LOCK_FILE: %s\n", _mca_ca_op_lock_file );
    debug_print( "MCA_CA_KEY_FILE: %s\n", _mca_ca_key_file );
    if( _mca_ca_key_passin )
        debug_print( "MCA_CA_KEY_PASSIN: %s\n", _mca_ca_key_passin );
    else
        debug_print( "MCA_CA_KEY_PASSIN not defined: will prompt for password.\n" );

    debug_print( "Policy:\n" );
    list_signing_policy( policy );
	return TRUE;
}

//-------------------------------------------------------------

int
get_validated_ca_password( )
{
    BIO *key=NULL;
    EVP_PKEY *pkey=NULL;
    PW_CB_DATA cb_data;
    char *key_str = NULL;
    char *passin = (char *)malloc( APP_PASS_LEN );
    if( passin ) {
        memset( passin, 0, 1 );
    } else {
        debug_print( "get_validated_ca_password: Couldn't get memory.\n" );
        exit( 1 );
    }
    if( _mca_ca_key_passin ) {
        strncpy( passin, _mca_ca_key_passin, APP_PASS_LEN );
    }

    app_passwd(NULL, passin, NULL, &key_str, NULL);

    cb_data.password = key_str;
    cb_data.prompt_info = _mca_ca_key_file;

    key = BIO_new( BIO_s_file() );
    if( key == NULL ) {
        debug_print( "Couldn't even begin to read key!\n" );
        exit( 1 );
    }
    ERR_print_errors_fp( stderr );
    if( BIO_read_filename( key, _mca_ca_key_file ) <= 0) {
        ERR_print_errors_fp( stderr );
        debug_print( "Couldn't read key from file '%s'.\n", _mca_ca_key_file );
        exit( 1 );
    }
    ERR_print_errors_fp( stderr );
    
    pkey = PEM_read_PrivateKey( safe_fopen_wrapper(_mca_ca_key_file, "r"), 
								NULL,
                                (pem_password_cb *)password_callback, 
								&cb_data );
    /*pkey = PEM_read_bio_PrivateKey( key, NULL,
      NULL, &cb_data);*/
    free( passin );
    if( pkey == NULL ) {
        debug_print( "Couldn't open private key file.\n" );
        ERR_print_errors_fp( stderr );
            return 0;
    } else {
        _mca_ca_password = strdup( key_str );
        free( key_str );
        return 1;
    }
}

//-------------------------------------------------------------

int
main_init(int argc, char *argv[])
{
    char **    ptr;
    char       buf[ MCA_MAX_SMALL_FILE_SIZE ];

    ERR_load_crypto_strings();
    CRYPTO_malloc_init();
    OpenSSL_add_all_algorithms();
    
	// This should be made to use getopt?
	// This should contact the collector so that it works like a proper condor daemon.
    for(ptr = argv + 1; *ptr; ptr++) {
        if(ptr[0][0] != '-') {
            usage( argv[0] );
        }
        switch( ptr[0][1] ) {
        case 'm':
            ++ptr;
            if( !(ptr && *ptr) ) {
                debug_print( "-m requires another argument\n");
                usage( argv[0] );
                EXCEPT( "-m requires another argument" );
                
            }
            //buf = daemonCore->InfoCommandSinfulString();
            sprintf(buf,"%s\n", daemonCore->InfoCommandSinfulString());
            if(str_to_file( buf, *ptr ) == -1) {
                debug_print( "In main_init" );
                debug_print( "Error opening file for writing: ");
                usage( argv[0] );
            }
            break;
        default:
            debug_print( "Error: Unknown option %s\n", *ptr );
            usage( argv[0] );
        }
    }

    main_config( FALSE );

    if( get_validated_ca_password( ) == 0 ) {
        debug_print( "Error: server password bad.\n" );
        exit( 1 );
    }
    debug_print( "Got CA Password OK.\n" );

    daemonCore->Register_Command( DC_SIGN_CERT_REQUEST, "SIGN_CERT_REQUEST",
                                  (CommandHandler)&sign_cert_request,
                                  "sign_cert_request", NULL, READ,
                                  D_FULLDEBUG );

	return TRUE;
}

//-------------------------------------------------------------

int
main_shutdown_fast()
{
	dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
	DC_Exit( 0 );
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

int
main_shutdown_graceful()
{
	dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
	DC_Exit( 0 );
	return TRUE;	// to satisfy c++
}

//-------------------------------------------------------------

void
main_pre_dc_init( int argc, char* argv[] )
{
		// dprintf isn't safe yet...
}


void
main_pre_command_sock_init( )
{
}

