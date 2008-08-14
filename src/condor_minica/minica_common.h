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

/* minica_common.h
 *
 * $Id$
 *
 * This file contains code shared by both the minica server and
 * client.  The primary reason for this is that either the client or
 * the server can perform the key generation and certificate request
 * steps.
 */

/* These response codes are shared between the server and the client.
 * In situations where versions of server and client differ, these
 * should stay the same.  If the server wants to send more information
 * about the nature of the error, it can use the separate error string
 * which is passed back to the client.
 */
#define MCA_RESPONSE_ALL_OK 0
#define MCA_RESPONSE_KEY_FAILED 1
#define MCA_RESPONSE_CSR_FAILED 2
#define MCA_RESPONSE_CERT_FAILED 3
#define MCA_RESPONSE_CERT_POLICY_FAILED 4

#define MCA_PROTOCOL_VERSION "1.0"
#define MCA_ACCEPT_PASSIVE_CLIENTS 1

/* When communication between the client and the server is initiated,
 * the server can reject the client's connection by sending an abort.
 * This might be because of a version incompatability.
 */
#define MCA_PROTOCOL_ABORT 0
#define MCA_PROTOCOL_PROCEED 1

/* Sometimes we can't generate a key because there isn't enough
   randomness.  How many attempts should be made before we give up?  */
#define MCA_MAX_KEYGEN_ATTEMPTS 8

/* How long between attempts? */
#define MCA_KEYGEN_WAIT_TIME_SECS 15

/* How big can a CN be? */
#define MCA_MAX_COMMONNAME_LEN 128

/* How long can a password from the keyboard be? */
#define MCA_PASSWORD_MAX_SIZE 128

/* The maximum size of the key file */
#define MCA_MAX_SMALL_FILE_SIZE 50000 // Must be big enough for openssl.cnf
#define MCA_MAX_KEY_SIZE MCA_MAX_SMALL_FILE_SIZE
#define MCA_MAX_CSR_SIZE MCA_MAX_SMALL_FILE_SIZE

#define MCA_ERROR_BUFSIZE 5000

/* When you add an error here, add a case to the switch in
 * minica_common.C get_mca_error_message.  The code does not assume
 * that these are shared between the client and the server (although
 * each uses them).
 */
#define MCA_ERROR_NOERROR                     0
#define MCA_ERROR_MALLOC                      1
#define MCA_ERROR_PIPE                        2
#define MCA_ERROR_FORK                        3
#define MCA_ERROR_PIPE_READ                   4
#define MCA_ERROR_WAIT                        5
#define MCA_ERROR_BUFFER_OVERFLOW             6
#define MCA_ERROR_OPEN_FILE                   7
#define MCA_ERROR_TEMP_FILE                   8
#define MCA_ERROR_NO_RESPONSE                 9
#define MCA_ERROR_LOCK_FILE                   10
#define MCA_ERROR_WRITE_FILE                  11
#define MCA_ERROR_CLOSE_FILE                  12
#define MCA_ERROR_OPENSSL_INDEX_CLASH         13
#define MCA_ERROR_POLICY_FAILURE              14
#define MCA_ERROR_NULL_GLOBAL                 15

#define MCA_ERROR_NOERROR_STRING    "No errors encountered."
#define MCA_ERROR_MALLOC_STRING     "Error allocating memory."
#define MCA_ERROR_PIPE_STRING       "Error creating pipe."
#define MCA_ERROR_FORK_STRING       "Error forking."
#define MCA_ERROR_PIPE_READ_STRING  "Error reading from pipe."
#define MCA_ERROR_WAIT_STRING       "Error while waiting."
#define MCA_ERROR_BUFFER_OVERFLOW_STRING   "Overflowed buffer: attack?"
#define MCA_ERROR_OPEN_FILE_STRING  "Error opening file."
#define MCA_ERROR_TEMP_FILE_STRING  "Error creating temporary file."
#define MCA_ERROR_NO_RESPONSE_STRING "Expected response but got nothing."
#define MCA_ERROR_LOCK_FILE_STRING  "Error creating lock or lockfile."
#define MCA_ERROR_WRITE_FILE_STRING "Error writing to file."
#define MCA_ERROR_CLOSE_FILE_STRING "Error closing file."
#define MCA_ERROR_OPENSSL_INDEX_CLASH_STRING "Certificate already issued."
#define MCA_ERROR_POLICY_FAILURE_STRING "Request violates policy."
#define MCA_ERROR_NULL_GLOBAL_STRING "Sanity error: expected global to be defined."

enum request_type { NONE, CLIENT, HOST };

/* Given a string and a file name, write the string to the file.
 */
int str_to_file( char * str, char * fn );

/* Given a file name, return a string.
 */
char * file_to_str( char * filename, int * error );

/* Look up the string associated with an error code.
 */
char * get_mca_error_message( int error_code );

/* Generate a rsa key by calling "openssl genrsa".  Sometimes this
 * fails because there isn't enough randomness to satisfy the
 * /dev/random paranoia.  So we wait, and retry a few times.
 */
char * repeat_gen_rsa_key( char * pass, int attempts, int wait_time,
                           int * err_code );

/* Given a key string, and a CN, and a password to access the key,
 * return the signing request.
 */
char * gen_signing_request( char * key, char * common_name,
                            char * pass, int * err_code );

/* Read a password from the terminal */
char * my_getpass( char * prompt );

/* Print information.  This differs for client and server... */
void debug_print( char * fmt, ... );

/* Before forking, print what's going to happen. */
void print_argv_to_fork ( char * argv[] );

