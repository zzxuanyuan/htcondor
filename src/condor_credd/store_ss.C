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
#include "condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "my_username.h"
#include "condor_config.h"
#include "credential.h"
#include "condor_distribution.h"
#include "sslutils.h"
#include "internet.h"
#include <unistd.h>
#include "dc_credd.h"
#include "credential.h"
#include "condor_version.h"

/*char Myproxy_pw[512];	// pasaword for credential access from MyProxy
// Read MyProxy password from terminal, or stdin.
bool Read_Myproxy_pw_terminal = true;

int parseMyProxyArgument(const char*, char*&, char*&, int&);
char * prompt_password (const char *);
char * stdin_password (const char *); */
bool read_file (const char * filename, char *& data, int & size);

void
version()
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}

void
usage(const char *myName)
{
	fprintf( stderr, "Usage: %s secret\n", myName );
/*	fprintf( stderr, "      Valid options:\n" );
	fprintf( stderr, "      -d\tdebug output\n\n" );
	fprintf( stderr, "      -n <host:port>\tsubmit to the specified credd\n" );
	fprintf( stderr, "      \t(e.g. \"-s myhost.cs.wisc.edu:1234\")\n\n");
	fprintf( stderr, "      -t x509\tspecify credential type\n");
	fprintf( stderr, "      \tonly the x509 type is currently available\n");
	fprintf( stderr, "      \thowever this option is now REQUIRED\n\n");
	fprintf( stderr, "      -f <file>\tspecify where credential is stored\n\n");
	fprintf( stderr, "      -N <name>\tspecify credential name\n\n");
	fprintf( stderr, "      -m [user@]host[:port]\tspecify MyProxy user/server\n" );
	fprintf( stderr, "      \t(e.g. \"-m wright@myproxy.cs.wisc.edu:1234\")\n\n");
	fprintf( stderr, "      -D <DN>\tspecify myproxy server DN (if not standard)\n" );
	fprintf( stderr, "      \t(e.g. \"-D \'/CN=My/CN=Proxy/O=Host\'\")\n\n" );
	fprintf( stderr, "      -S\tread MyProxy password from standard input\n\n");
*/
	fprintf( stderr, "      -v\tprint version\n\n" );
	fprintf( stderr, "      -h\tprint this message\n\n");
	exit( 1 );
}


int main(int argc, char **argv)
{
	char ** ptr;
	const char * myName;

	// find our name
	myName = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !myName ) {
		myName = argv[0];
	} else {
		myName++;
	}

	MyString ssn;
	char * ss_file_name = NULL;

	char * server_address= NULL;

	char * secret = NULL;

	config();

	for (ptr=argv+1,argc--; argc > 0; argc--,ptr++) {
		if ( ptr[0][0] == '-' ) {
			switch ( ptr[0][1] ) {
			case 'h':
				usage(myName);
				exit(0);
				break;
			case 'd':

					// dprintf to console
				Termlog = 1;
				dprintf_config ("TOOL");

				break;
			case 'S':

					// dprintf to console
				Termlog = 1;

				break;

			case 'n':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -n requires another argument\n",
							 myName );
					exit(1);
				}
	
				server_address = strdup (*ptr);

				break;

				// Read the secret from a file so evildoers can't use 'ps'.
			case 'f':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -f requires another argument\n",
							 myName );
					exit(1);
				}
				ss_file_name = strdup (*ptr);
				break;

				// Or, just put it right there for everyone to see...
			case 's':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -s requires another argument\n",
							 myName );
					exit(1);
				}
				secret = strdup (*ptr);
				break;

			case 'v':
				version();	// this function calls exit(0)
				break;

			default:
				fprintf( stderr, "%s: Unknown option %s\n",
						 myName, *ptr);
				usage(myName);
				exit(1);
			}
		} //fi
	} //rof

	if( !secret ) {
		if ( ss_file_name == NULL ) {
			fprintf ( stderr, "Shared secret filename not specified.\n");
			exit (1);
			
		}
		
		int data_size;
		// We don't really care about data_size.
		if (!read_file (ss_file_name, secret, data_size)) {
			fprintf (stderr, "Can't open %s\n", ss_file_name);
			exit (1);
		}
	}

	CondorError errstack;
	DCCredd dc_credd (server_address);

	// resolve server address
	if ( ! dc_credd.locate() ) {
		fprintf (stderr, "%s\n", dc_credd.error() );
		return 1;
	}

	if (dc_credd.storeSharedSecret(secret, ssn, errstack)) {
		printf ("Shared secret submitted successfully\n");
		printf ("Name: %s\n", ssn.GetCStr());
	} else {
		fprintf (stderr, "Unable to submit shared secret\n%s\n",
				 errstack.getFullText(true));
		return 1;
	}

	return 0;
}


bool
read_file (const char * filename, char *& data, int & size) {

	int fd = safe_open_wrapper(filename, O_RDONLY);
	if (fd == -1) {
		return false;
	}

	struct stat my_stat;
	if (fstat (fd, &my_stat) != 0) {
		close (fd);
		return false;
	}

	size = (int)my_stat.st_size;
	data = (char*)malloc(size+1);
	data[size]='\0';
	if (!data) 
		return false;
 	
	if (!read (fd, data, size)) {
		free (data);
		return false;
	}

	close (fd);
	return true;
}

