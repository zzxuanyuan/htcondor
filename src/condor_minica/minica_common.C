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

// #include <termios.h>
#include <openssl/ui.h>

#include "condor_common.h"
// #include "condor_string.h"

#include "minica_common.h"
#include "file_lock.h"

/* Config file parameter: this is where openssl is located. */
extern char * _mca_path_to_openssl;

/* Even the client needs this configuration file.  Perhaps we should
 * create it from the binary if it does not exist.
 */
extern char * _mca_path_to_openssl_cnf;

/**
 * Get password from terminal and return it.  Typically, when a
 * password is obtained for encryption, this routine is called twice,
 * to prevent errors from typos.
 */
char *
my_getpass ( char *prompt )
{
    UI *srv_ui = NULL;
    int rv = 0;
    char *buf = NULL;
    const char *pass = NULL;
    char *ret_str = NULL;

    buf = (char *)malloc( MCA_PASSWORD_MAX_SIZE+1 );
    bzero( buf, MCA_PASSWORD_MAX_SIZE+1 );

    srv_ui = UI_new( );
    rv = UI_add_input_string( srv_ui, prompt, 0, buf, 0,
                              MCA_PASSWORD_MAX_SIZE );
    rv = UI_process( srv_ui );
    pass = UI_get0_result( srv_ui, 0 );
    ret_str = strdup( pass );
    //printf("See: '%s'\n", b);
    UI_free( srv_ui );
    return( ret_str );
}

/**
 * Safe memset for passwords.  See Viega & Messier, Secure Programming Cookbook.
 */
volatile void *
spc_memset( volatile void *dst, int c, size_t len )
{
    volatile char *buf;
    for( buf = (volatile char *)dst; len; buf[--len] = c );
    return dst;
}

/**
 * Useful for debugging, this just prints the command that will be
 * issued.
 */
void
print_argv_to_fork( char * argv[] ) {
    int i = 0;
    int linelen = 0;
    char *line;

    while(argv[i] != NULL) {
        linelen += strlen(argv[i])+1;
        i++;
    }

    line = (char *)malloc( linelen );
    if( line == NULL ) {
        debug_print( "Error in malloc in print_argv_to_fork.\n" );
        return;
    }
    i = 0;
    linelen = 0;
    while(argv[i] != NULL) {
        sprintf( line + linelen, "%s ", argv[i] );
        linelen += strlen(argv[i])+1;
        i++;
    }
    
    debug_print( "%s\n", line );
}

/**
 * Write the contents of the string to the file.
 *
 * @returns 0 on success, -1 on error.
 **/
int
str_to_file( char * str, char * fn )
{
    FILE *fp;
    
    if( (fp = safe_fopen_wrapper( fn, "w" )) ) {
        fputs( str, fp );
        fclose( fp );
        return 0;
    } else {
        debug_print( "str_to_file: Error opening file '%s' for writing.\n", 
					 fn );
        return -1;
    }
}

/**
 * Return the contents of the (small) file.
 */
char *
file_to_str( char * filename, int *err_code )
{
    FILE *fp;
    int ch, ptr = 0;
    char buf[ MCA_MAX_SMALL_FILE_SIZE ];
    char *rv = NULL;

    *err_code = MCA_ERROR_NOERROR;
    
    if( (fp = safe_fopen_wrapper( filename, "r" )) ) {
        while( ( ch = fgetc( fp ) ) != EOF ) {
            if( ptr > MCA_MAX_SMALL_FILE_SIZE ) {
                debug_print( "Attack in progress: %s is > %d chars.\n",
                             filename, MCA_MAX_SMALL_FILE_SIZE );
                fclose( fp );
                *err_code = MCA_ERROR_BUFFER_OVERFLOW;
                return NULL;
            }
            buf[ptr++] = ch;
        }
        buf[ptr++] = 0;
        fclose( fp );
        rv = strdup( buf );
        if( rv == NULL ) {
            debug_print( "file_to_str: Can't allocate memory.\n" );
            *err_code = MCA_ERROR_MALLOC;
            return NULL;
        }
        return rv;
    } else {
        debug_print( "file_to_str: Can't read %s.\n", filename );
        *err_code = MCA_ERROR_OPEN_FILE;
        return NULL;
    }
}

/**
 * This routine is necessary because the different calling programs
 * (client and server) handle error responses differently.  Best to
 * just send a message to the caller and let them figure out what to
 * do with it.
 **/
char *
get_mca_error_message( int err_code )
{
    switch( err_code ) {
    case MCA_ERROR_NOERROR:
        return strdup( MCA_ERROR_NOERROR_STRING );
        break;
    case MCA_ERROR_MALLOC:
        return strdup( MCA_ERROR_MALLOC_STRING );
        break;
    case MCA_ERROR_PIPE:
        return strdup( MCA_ERROR_PIPE_STRING );
        break;
    case MCA_ERROR_FORK:
        return strdup( MCA_ERROR_FORK_STRING );
        break;
    case MCA_ERROR_PIPE_READ:
        return strdup( MCA_ERROR_PIPE_READ_STRING );
        break;
    case MCA_ERROR_WAIT:
        return strdup( MCA_ERROR_WAIT_STRING );
        break;
    case MCA_ERROR_BUFFER_OVERFLOW:
        return strdup( MCA_ERROR_BUFFER_OVERFLOW_STRING );
        break;
    case MCA_ERROR_OPEN_FILE:
        return strdup( MCA_ERROR_OPEN_FILE_STRING );
        break;
    case MCA_ERROR_TEMP_FILE:
        return strdup( MCA_ERROR_TEMP_FILE_STRING );
        break;
    case MCA_ERROR_NO_RESPONSE:
        return strdup( MCA_ERROR_NO_RESPONSE_STRING );
        break;
    case MCA_ERROR_LOCK_FILE:
        return strdup( MCA_ERROR_LOCK_FILE_STRING );
        break;
    case MCA_ERROR_WRITE_FILE:
        return strdup( MCA_ERROR_WRITE_FILE_STRING );
        break;
    case MCA_ERROR_CLOSE_FILE:
        return strdup( MCA_ERROR_CLOSE_FILE_STRING );
        break;
    case MCA_ERROR_OPENSSL_INDEX_CLASH:
        return strdup( MCA_ERROR_OPENSSL_INDEX_CLASH_STRING );
        break;
    case MCA_ERROR_POLICY_FAILURE:
        return strdup( MCA_ERROR_POLICY_FAILURE_STRING );
        break;
    case MCA_ERROR_NULL_GLOBAL:
        return strdup( MCA_ERROR_NULL_GLOBAL_STRING );
        break;
    default:
        return strdup( "Unknown error!" );
    }
}

/**
 * Given a password, fork openssl to generate an RSA key, and return
 * the (possibly encrypted result) as a blob of text.
 **/
char *
gen_rsa_key( char *pass, int *err_code )
{
    int pid, status;

    char fdstr[16];
    int pi[2], po[2];
    int rv_size;

    char *rv = (char *)malloc( MCA_MAX_KEY_SIZE );

    char *user_argv[] = { _mca_path_to_openssl, "genrsa", "-des3",
                     "-passout", "",
                     "1024", NULL };

    char *host_argv[] = { _mca_path_to_openssl, "genrsa", "1024", NULL };

    char **argv;

    if( _mca_path_to_openssl == NULL ) {
        debug_print( "Error: _mca_path_to_openssl not defined.\n" );
        *err_code = MCA_ERROR_NULL_GLOBAL;
        return NULL;
    }

    *err_code = MCA_ERROR_NOERROR;

    if( rv == 0 ) {
        debug_print( "gen_rsa_key: Error in malloc.\n" );
        *err_code = MCA_ERROR_MALLOC;
        return NULL;
    }
    
    if( pipe( po ) ) { // Pipe for getting the key.
        debug_print( "gen_rsa_key: Error creating pipe.\n" );
        *err_code = MCA_ERROR_PIPE;
        goto error;
    }

    if( pass == NULL ) {
        argv = host_argv;
    } else {
        // debug_print( "Pass is not null here.\n" );
        argv = user_argv;
        if( pipe( pi ) ) { // Pipe for sending the password.
            debug_print( "gen_rsa_key: Error creating pipe.\n" );
            *err_code = MCA_ERROR_PIPE;
            goto error;
        }
        
        snprintf(fdstr, 16, "fd:%d", pi[0]);
        argv[4] = fdstr;
    }

    pid = fork();
    if(pid == -1) {
        debug_print( "gen_rsa_key: Error forking.\n");
        *err_code = MCA_ERROR_FORK;
        goto error;
    }
    if(pid == 0) { // child
        // see minica_server_main.C for full comments
        close( po[0] );
        dup2( po[1], 1 );
        close( po[1] );
        
        if( pass != NULL ) {
            close( pi[1] );
        }
        close( 2 );
        execve( _mca_path_to_openssl, argv, NULL );
        debug_print( "Exec failed?\n" );
        exit(127);
    }
    
    // parent
    if( pass != NULL ) {
        close( pi[0] );
        write( pi[1], pass, strlen( pass ) );
        close( pi[1] );
    }

    rv_size = read( po[0], rv, MCA_MAX_KEY_SIZE );
    if( rv_size < 1 ) {
        debug_print( "Can't read anything.\n" );
        *err_code = MCA_ERROR_PIPE_READ;
        goto error;
    }
    close( po[0] );
    
    do {
        if( waitpid(pid, &status, 0) == -1 ) {
            if( errno != EINTR ) {
                debug_print( "gen_rsa_key: Problem, can't wait right." );
                *err_code = MCA_ERROR_WAIT;
                goto error;
            }
        } else {
            return rv;
        }
    } while(1);

  error:
    free( rv );
    return NULL;
}

/**
 * Given a password, repeatedly call gen_rsa_key until a key gets generated.
 **/
char *
repeat_gen_rsa_key( char * pass, int attempts, int wait_time, int *err_code )
{
    int attempt_number = 0;
    char * key_block = NULL;

    *err_code = MCA_ERROR_NOERROR;
    
    while( key_block == NULL ) {
        debug_print( "Attempt number %d.\n", attempt_number+1 );
        attempt_number++;
        if( attempt_number >= attempts ) {
            debug_print( "Unable to generate key after %d tries.\n", attempt_number );
            // OK to fall through because this is checked in the next block.
            continue;
        }
        key_block = gen_rsa_key( pass, err_code );
        if( key_block == NULL ) {
            sleep( wait_time );
        } else {
            continue;
        }
    }
    
    if( (key_block == NULL) || strlen( key_block ) == 0 ) {
        debug_print( "Generate key failed.\nExiting.\n" );
        exit( 1 );
    }
    return key_block;
}

/**
 * Given a private key, and associated password, generate a
 * certificate signing request and return it.
 */
char *
gen_signing_request( char * key,
                     char * common_name, char * pass, int *err_code )
{
    int pid, status;

    int key_fd;
    char key_filename[] = "/tmp/tmp_mcaXXXXXX";
    char * rv = NULL; 
    int rv_size = 0;

    char fdstr[ 16 ];

    debug_print( "Entering gen_signing_request.\n" );

    /* check the globals */
    if( _mca_path_to_openssl == NULL ) {
        debug_print( "Error: _mca_path_to_openssl not defined.\n" );
        *err_code = MCA_ERROR_NULL_GLOBAL;
        return NULL;
    }
    if( _mca_path_to_openssl_cnf == NULL ) {
        debug_print( "Error: _mca_path_to_openssl_cnf not defined.\n" );
        *err_code = MCA_ERROR_NULL_GLOBAL;
        return NULL;
    }

    char *argv[ ] = { _mca_path_to_openssl, "req", "-new", "-key", key_filename,
                      "-config", _mca_path_to_openssl_cnf, //"openssl.cnf",
                      "-passin", "",
                      0 };
    int pipeid_form[ 2 ];
    int pipeid_pass[ 2 ];
    int pipeid_csr[ 2 ];

    int keylen = 0;
    
    *err_code = MCA_ERROR_NOERROR;

    rv = (char *)malloc( MCA_MAX_CSR_SIZE );
    if( rv == NULL ) {
        debug_print( "gen_signing_request: Error in malloc.\n" );
        *err_code = MCA_ERROR_MALLOC;
        return NULL;
    }
    
    if( ( key_fd = mkstemp( key_filename ) ) == -1 ) {
        *err_code = MCA_ERROR_TEMP_FILE;
        goto error_csr_free;
    }

    keylen = strlen( key );
    if( write( key_fd, key, keylen ) != keylen ) {
        debug_print( "gen_signing_request: Error writing key to temp file.\n" );
        *err_code = MCA_ERROR_WRITE_FILE;
        goto error_csr_unlink;
    }

    if( close( key_fd ) != 0 ) {
        debug_print( "gen_signing_request: Error closing temp file.\n" );
        *err_code = MCA_ERROR_CLOSE_FILE;
        goto error_csr_unlink;
    }

    if( pipe( pipeid_pass ) ) {
        debug_print( "gen_signing_request: Error creating pipe.\n" );
        // exit( 1 );
        *err_code = MCA_ERROR_PIPE;
        goto error_csr_unlink;
    }
    snprintf( fdstr, 16, "fd:%d", pipeid_pass[0] );
    argv[8] = fdstr;

    print_argv_to_fork( argv );
    
    if( pipe( pipeid_form ) ) {
        debug_print( "gen_signing_request: Error creating pipe.\n" );
        *err_code = MCA_ERROR_PIPE;
        goto error_csr_unlink;
    }

    if( pipe( pipeid_csr ) ) {
        debug_print( "gen_signing_request: Error creating pipe.\n" );
        *err_code = MCA_ERROR_PIPE;
        goto error_csr_unlink;
    }

    pid = fork();
    if(pid == -1) {
        debug_print( "gen_signing_request: Error forking.\n" );
        *err_code = MCA_ERROR_FORK;
        goto error_csr_unlink;
    }
    if(pid == 0) { // child

        // Get stdin for form from parent.
        close( pipeid_form[1] );
        dup2( pipeid_form[0], 0 );
        close( pipeid_form[0] );
        
        close( pipeid_pass[1] );

        close( pipeid_csr[0] );
        dup2( pipeid_csr[1], 1 );
        close( pipeid_csr[1] );

        close( 2 );

        /* This global is cleared in this function */
        execve( _mca_path_to_openssl, argv, NULL );
        exit(127);
    }

    // parent
    close( pipeid_pass[0] );
    if( pass == NULL ) {
        pass = strdup("\n");
        write( pipeid_pass[1], pass, strlen( pass ) );
        free( pass );
        pass = NULL;
    } else {
        write( pipeid_pass[1], pass, strlen( pass ) );
    }
    close( pipeid_pass[1] );
    
    close( pipeid_form[0] );
    write( pipeid_form[1], "\n\n\n\n\n", 5 );
    write( pipeid_form[1], common_name, strlen( common_name ) );
    write( pipeid_form[1], "\n\n\n\n\n\n", 6 );
    close( pipeid_form[1] );

    rv_size = read( pipeid_csr[0], rv, MCA_MAX_CSR_SIZE );
    if( rv_size < 1 ) {
        debug_print( "Can't read certificate signing request - "
					 "why not?" );
        *err_code = MCA_ERROR_PIPE_READ;
        goto error_csr_unlink;
    }
    close( pipeid_csr[0] );

    if( unlink( key_filename ) != 0 ) {
        debug_print( "gen_signing_request: Problem, can't unlink "
					 "private key file!\n" );
        // *err_code = MCA_CLEANUP_ERROR;
        exit( 1 ); 
    }

    do {
        if( waitpid(pid, &status, 0) == -1 ) {
            if( errno != EINTR ) {
                debug_print( "gen_signing_request: Problem, "
							 "can't wait right.\n" );
                *err_code = MCA_ERROR_WAIT;
                goto error_csr_unlink;
            }
        } else {
            if( rv == NULL || strlen( rv ) == 0 ) {
                debug_print( "gen_signing_request: Certificate request "
							 "is zero length.  Aborting.\n" );
                perror( "" );
                *err_code = MCA_ERROR_NO_RESPONSE;
                goto error_csr_unlink;
            }
			debug_print( "Returning OK.\n" );
            return rv;
        }
    } while( 1 );
    
  error_csr_unlink:
    debug_print( "Got error creating request.\n" );
    if( unlink( key_filename ) != 0 ) {
        debug_print( "gen_signing_request: "
					 "Problem, can't unlink private key file!\n" );
        // *err_code = MCA_CLEANUP_ERROR;
        exit( 1 ); 
    }
  error_csr_free:
    debug_print( "Got error creating request.\n" );
    free( rv );
    return NULL;
}

