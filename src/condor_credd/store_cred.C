#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "my_username.h"
#include "condor_config.h"
#include "credential.h"
#include "condor_distribution.h"
#include "sslutils.h"
#include "internet.h"
#include "client_common.h"
#include <unistd.h>
#include "dc_credd.h"
#include "credential.h"

const char * MyName = "condor_store_cred";
char Myproxy_pw[512];	// pasaword for credential access from MyProxy
// Read MyProxy password from terminal, or stdin.
bool Read_Myproxy_pw_terminal = true;

int parseMyProxyArgument(const char*, char*&, char*&, int&);
char * prompt_password (const char *);
char * stdin_password (const char *);
bool read_file (const char * filename, char *& data, int & size);

void
usage()
{
	fprintf( stderr, "Usage: %s [options] [cmdfile]\n", MyName );
	fprintf( stderr, "      Valid options:\n" );
	fprintf( stderr, "      -v\tverbose output\n\n" );
	fprintf( stderr, "      -s <host>\tsubmit to the specified credd\n" );
	fprintf( stderr, "      \t(e.g. \"-s myhost.cs.wisc.edu\")\n\n");
	fprintf( stderr, "      -t <x509|password> specify credential type");
	fprintf( stderr, "      -f <file>\tspecify where credential is stored\n\n");
	fprintf( stderr, "      -n <name>\tspecify credential name\n\n");
	fprintf( stderr, "      -m [user@]host[:port]\tspecify MyProxy user/server\n" );
	fprintf( stderr, "      \t(e.g. \"-m wright@myproxy.cs.wisc.edu:1234\")\n\n");
	fprintf( stderr, "      -D <DN>\tspecify myproxy server DN (if not standard)\n" );
	fprintf( stderr, "      \t(e.g. \"-D \'/CN=My/CN=Proxy/O=Host\'\")\n\n" );
	fprintf( stderr, "      -S\tread MyProxy password from standard input\n\n");
	fprintf( stderr, "      -h\tprint this message\n\n");
	exit( 1 );
}


int main(int argc, char **argv)
{
	char ** ptr;


	int cred_type = 0;
	char * cred_name = NULL;
	char * cred_file_name = NULL;
	char * myproxy_user = NULL;

	char * myproxy_host = NULL;
	int myproxy_port = 0;

	char * myproxy_dn = NULL;

	char * server_address= NULL;

	myDistro->Init (argc, argv);

	for (ptr=argv+1,argc--; argc > 0; argc--,ptr++) {
		if ( ptr[0][0] == '-' ) {
			switch ( ptr[0][1] ) {
			case 'h':
				usage();
				exit(0);
				break;
			case 'v':

					// dprintf to console
				Termlog = 1;
				dprintf_config ("TOOL", 2 );

				break;
			case 'S':

					// dprintf to console
				Termlog = 1;
				Read_Myproxy_pw_terminal = false;

				break;
			case 's':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -s requires another argument\n",
							 MyName );
					exit(1);
				}
	
				server_address = strdup (*ptr);

				break;
			case 't':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -t requires another argument\n",
							 MyName );
					exit(1);
				}

				if (strcmp (*ptr, "x509") == 0) {
					cred_type = X509_CREDENTIAL_TYPE;
				} else {
					fprintf( stderr, "Invalid credential type %s\n",
							 *ptr );
					exit(1);
				}
				break;
			case 'f':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -f requires another argument\n",
							 MyName );
					exit(1);
				}
				cred_file_name = strdup (*ptr);
				break;
			case 'n':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -n requires another argument\n",
							 MyName );
					exit(1);
				}
				cred_name = strdup (*ptr);
				break;

			case 'm':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -m requires another argument\n",
							 MyName );
					exit(1);
				}
	
				parseMyProxyArgument (*ptr, myproxy_user, myproxy_host, myproxy_port);
				break;
			case 'D':
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -D requires another argument\n",
							 MyName );
					exit(1);
				}
				myproxy_dn = strdup (*ptr);
				break;
			default:
				fprintf( stderr, "%s: Unknown option %s\n",
						 MyName, *ptr);
				usage();
				exit(1);
			}
		} //fi
	} //rof

	config();

	if (( cred_file_name == NULL ) && (cred_type == 0)) {
		fprintf ( stderr, "Credential filename or type not specified\n");
		exit (1);

	}

    Credential * cred = NULL;
	if (cred_type == X509_CREDENTIAL_TYPE) {
		cred = new X509Credential();
	} else {
		fprintf ( stderr, "Invalid credential type\n");
		exit (1);
	}

    
	char * data = NULL;
	int data_size;
	if (!read_file (cred_file_name, data, data_size)) {
		fprintf (stderr, "Can't open %s\n", cred_file_name);
		exit (1);
	}

	cred->SetData (data, data_size);

	if (cred_name !=NULL) {
		cred->SetName(cred_name);
	} else {
		cred->SetName(DEFAULT_CREDENTIAL_NAME);
	}

	char * username = my_username(0);
	cred->SetOwner (username);
  
	if (cred_type == X509_CREDENTIAL_TYPE && myproxy_host != NULL) {
		X509Credential * x509cred = (X509Credential*)cred;

		MyString str_host_port = myproxy_host;
		if (myproxy_port != 0) {
			str_host_port += ":";
			str_host_port += myproxy_port;
		}
		x509cred->SetMyProxyServerHost (str_host_port.Value());

		if (myproxy_user != NULL) {
			x509cred->SetMyProxyUser (myproxy_user);
		} else {
			x509cred->SetMyProxyUser (username);
		}

		if (myproxy_dn != NULL) {
			x509cred->SetMyProxyServerDN (myproxy_dn);
		}

		char * myproxy_password;
		if ( Read_Myproxy_pw_terminal ) {
			myproxy_password = 
				prompt_password(
					"Please enter the MyProxy password:" );
		} else {
			myproxy_password = 
				stdin_password(
				"Please enter the MyProxy password from the standard input\n");
		}
		if (myproxy_password) {
			x509cred->SetRefreshPassword ( myproxy_password );
		}
	}

	CondorError errstack;
	DCCredd dc_credd (server_address);
	if (dc_credd.storeCredential(cred, errstack)) {
		printf ("Credential submitted successfully\n");
	} else {
		fprintf (stderr, "Unable to submit credential (%d : %s)!\n",
				 errstack.code(), 
				 errstack.message());
	}

	return 0;
}

int  
parseMyProxyArgument (const char * arg,
					  char * & user, 
					  char * & host, 
					  int & port) {

	MyString strArg (arg);
	int at_idx = strArg.FindChar ((int)'@');
	int colon_idx = strArg.FindChar ((int)':', at_idx+1);

	if (at_idx != -1) {
		MyString _user = strArg.Substr (0, at_idx-1);
		user = strdup(_user.Value());
	}
  
  
	if (colon_idx == -1) {
		MyString _host = strArg.Substr (at_idx+1, strArg.Length()-1);
		host = strdup(_host.Value());
	} else {
		MyString _host = strArg.Substr (at_idx+1, colon_idx-1);
		host = strdup(_host.Value());

		MyString _port = strArg.Substr (colon_idx+1, strArg.Length()-1);
		port = atoi(_port.Value());

	}

	return TRUE;
}

char *
prompt_password(const char * prompt) {
	int rc =
		// Read password from terminal.  Disable terminal echo.
		des_read_pw_string (
			Myproxy_pw,					// buffer
			sizeof ( Myproxy_pw ) - 1,	// length
			prompt,						// prompt
			true						// verify
		);
	if (rc) {
		return NULL;
	}

	return Myproxy_pw;
}

char *
stdin_password(const char * prompt) {
	int nbytes;
	printf("%s", prompt);
	nbytes =
		// Read password from stdin.
		read (
			0,							// file descriptor = stdin
			Myproxy_pw,					// buffer
			sizeof ( Myproxy_pw ) - 1	// length
		);
	if ( nbytes <= 0 ) {
		return NULL;
	}

	return Myproxy_pw;
}

bool
read_file (const char * filename, char *& data, int & size) {

	int fd = open (filename, O_RDONLY);
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
