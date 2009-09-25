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
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_qmgr.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "get_daemon_name.h"
#include "condor_attributes.h"
#include "condor_distribution.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "MyString.h"
#include "my_getopt.h"

void
usage(char name[])
{
	fprintf(stderr, "Usage: %s [--debug] [-n schedd-name] [--pool pool-name] { cluster | cluster.proc | owner | --constraint constraint } attribute-name attribute-value ...\n", name);
	exit(1);
}

void
old_usage(char name[])
{
	fprintf(stderr, "Usage: %s [-debug] [-n schedd-name] [-pool pool-name] { cluster | cluster.proc | owner | -constraint constraint } attribute-name attribute-value ...\n", name);
	exit(1);
}

bool
ProtectedAttribute(char attr[])
{
	return (strcmp(attr, ATTR_OWNER) == 0) ||
		(strcmp(attr, ATTR_CLUSTER_ID) == 0) ||
		(strcmp(attr, ATTR_PROC_ID) == 0) ||
		(strcmp(attr, ATTR_MY_TYPE) == 0) ||
		(strcmp(attr, ATTR_TARGET_TYPE) == 0) ||
		(strcmp(attr, ATTR_JOB_STATUS) == 0);
}

int
main(int argc, char *argv[])
{
	MyString constraint = "";
	Qmgr_connection *q;
	int nextarg = 1, cluster, proc;
	bool UseConstraint = false;
	MyString schedd_name;
	MyString pool_name;

	myDistro->Init( argc, argv );
	config();

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

///////////////////////////////////////////////////////
	bool gnuargs = param_boolean("USE_GNU_ARGS", false);

	if (argc < 2) {
		if(gnuargs)
		  usage(argv[0]);
		else
		  old_usage(argv[0]);
	}

    if (!gnuargs) {
///////////////////////////////////////////////////////
	
		// if -debug is present, it must be first. sigh.
		if (argv[nextarg][0] == '-' && argv[nextarg][1] == 'd') {
			// output dprintf messages to stderror at TOOL_DEBUG level
			Termlog = 1;
			dprintf_config ("TOOL");
			nextarg++;
		}
	
		// if it is present, it must be first after debug.
		if (argv[nextarg][0] == '-' && argv[nextarg][1] == 'n') {
			nextarg++;
			// use the given name as the schedd name to connect to
			if (argc <= nextarg) {
				fprintf(stderr, "%s: -n requires another argument\n", 
						argv[0]);
				exit(1);
			}				
			schedd_name = argv[nextarg];
			nextarg++;
		}
	
		if (argc <= nextarg) {
			old_usage(argv[0]);
		}
	
		// if it is present, it must be just after -n flag
		if (argv[nextarg][0] == '-' && argv[nextarg][1] == 'p') {
			nextarg++;
			if (argc <= nextarg) {
				fprintf(stderr, "%s: -pool requires another argument\n", 
						argv[0]);
				exit(1);
			}
			pool_name = argv[nextarg];
			nextarg++;
		}

		DCSchedd schedd((schedd_name.Length() == 0) ? NULL : schedd_name.Value(),
						(pool_name.Length() == 0) ? NULL   : pool_name.Value());
		if ( schedd.locate() == false ) {
			if (schedd_name == "") {
				fprintf( stderr, "%s: ERROR: Can't find address of local schedd\n",
					argv[0] );
				exit(1);
			}
	
			if (pool_name == "") {
				fprintf( stderr, "%s: No such schedd named %s in local pool\n",
					argv[0], schedd_name.Value() );
			} else {
				fprintf( stderr, "%s: No such schedd named %s in "
					"pool %s\n",
					argv[0], schedd_name.Value(), pool_name.Value() );
			}
			exit(1);
		}
	
		// Open job queue 
		q = ConnectQ( schedd.addr() );
		if( !q ) {
			fprintf( stderr, "Failed to connect to queue manager %s\n", 
					schedd.addr() );
			exit(1);
		}
	
		if (argc <= nextarg) {
			old_usage(argv[0]);
		}
	
		if (match_prefix(argv[nextarg], "-constraint")) {
			nextarg++;
			if (argc <= nextarg) {
				old_usage(argv[0]);
			}
			constraint = argv[nextarg];
			nextarg++;
			UseConstraint = true;
		} else if (isdigit(argv[nextarg][0])) {
			char *tmp;
			cluster = strtol(argv[nextarg], &tmp, 10);
			if (cluster <= 0) {
				fprintf( stderr, "Invalid cluster # from %s.\n", argv[nextarg]);
				exit(1);
			}
			if (*tmp == '.') {
				proc = strtol(tmp + 1, &tmp, 10);
				if (cluster <= 0) {
					fprintf( stderr, "Invalid proc # from %s.\n", argv[nextarg]);
					exit(1);
				}
				UseConstraint = false;
			} else {
				constraint.sprintf("(%s == %d)", ATTR_CLUSTER_ID, cluster);
				UseConstraint = true;
			}
			nextarg++;
		} else {
			constraint.sprintf("(%s == \"%s\")", ATTR_OWNER, argv[nextarg]);
			nextarg++;
			UseConstraint = true;
		}
	
		if (argc <= nextarg) {
			old_usage(argv[0]);
		}
	
		for (; nextarg < argc; nextarg += 2) {
			if (argc <= nextarg+1) {
				old_usage(argv[0]);
			}
			if (ProtectedAttribute(argv[nextarg])) {
				fprintf(stderr, "Update of attribute \"%s\" is not allowed.\n",
						argv[nextarg]);
				exit(1);
			}
			if (UseConstraint) {
				if (SetAttributeByConstraint(constraint.Value(), argv[nextarg],
											argv[nextarg+1]) < 0) {
					fprintf(stderr,
							"Failed to set attribute \"%s\" by constraint: %s\n",
							argv[nextarg], constraint.Value());
					exit(1);
				}
			} else {
				if (SetAttribute(cluster, proc, argv[nextarg],
								argv[nextarg+1]) < 0) {
					fprintf(stderr,
							"Failed to set attribute \"%s\" for job %d.%d.\n",
							argv[nextarg], cluster, proc);
					exit(1);
				}
			}
			printf("Set attribute \"%s\".\n", argv[nextarg]);
		}

//GET OPT STYLE
	} else {
		int c;

		while(1)
		{
			static struct option long_options[] =
			{
				{"constraint",  required_argument,  0,   'c'},
				{"debug",       no_argument,        0,   'd'},
				{"name",        required_argument,  0,   'n'},
				{"pool",        required_argument,  0,   'p'},
				{0, 0, 0, 0,}
			};
			/* getopt_long stores the option index here */
			int option_index = 0;
			
			c = my_getopt_long (argc, argv, "dn:p:", long_options,
						&option_index);
			
			if (c == -1)
				break;
			
			switch (c)
			{
			case 'c':
				constraint = my_optarg;
				break;

			case 'd':
				// output dprintf messages to stderror at TOOL_DEBUG level
				Termlog = 1;
				dprintf_config ("TOOL");
				break;

			case 'n':
				// use the given name as the schedd name to connect to
				schedd_name = my_optarg;
				break;


			case 'p':
				pool_name = my_optarg;
				break;

			case '?':
				usage(argv[0]);
				break;

			default:
				abort();
			}
		}

		DCSchedd schedd((schedd_name.Length() == 0) ? NULL : schedd_name.Value(),
					(pool_name.Length() == 0) ? NULL   : pool_name.Value());
		if ( schedd.locate() == false ) {
			if (schedd_name == "") {
				fprintf( stderr, "%s: ERROR: Can't find address of local schedd\n",
					argv[0] );
				exit(1);
			}
	
			if (pool_name == "") {
				fprintf( stderr, "%s: No such schedd named %s in local pool\n",
					argv[0], schedd_name.Value() );
			} else {
				fprintf( stderr, "%s: No such schedd named %s in "
					"pool %s\n",
					argv[0], schedd_name.Value(), pool_name.Value() );
			}
			exit(1);
		}

		// Open job queue 
		q = ConnectQ( schedd.addr() );
		if( !q ) {
			fprintf( stderr, "Failed to connect to queue manager %s\n", 
					schedd.addr() );
			exit(1);
		}

		if (argc <= my_optind) {
			usage(argv[0]);
		}

		if (constraint != "")
			UseConstraint = true;

 		else if (isdigit(argv[my_optind][0])) {
			char *tmp;
			cluster = strtol(argv[my_optind], &tmp, 10);
			if (cluster <= 0) {
				fprintf( stderr, "Invalid cluster # from %s.\n", argv[my_optind]);
				exit(1);
			}
			if (*tmp == '.') {
				proc = strtol(tmp + 1, &tmp, 10);
				if (cluster <= 0) {
					fprintf( stderr, "Invalid proc # from %s.\n", argv[my_optind]);
					exit(1);
				}
				UseConstraint = false;
			} else {
				constraint.sprintf("(%s == %d)", ATTR_CLUSTER_ID, cluster);
				UseConstraint = true;
			}
			my_optind++;
		} else {
			constraint.sprintf("(%s == \"%s\")", ATTR_OWNER, argv[my_optind]);
			my_optind++;
			UseConstraint = true;
		}

		if (argc <= my_optind) {
			usage(argv[0]);
		}
	
		for (; my_optind < argc; my_optind += 2) {
			if (argc <= my_optind+1) {
				usage(argv[0]);
			}
			if (ProtectedAttribute(argv[my_optind])) {
				fprintf(stderr, "Update of attribute \"%s\" is not allowed.\n",
						argv[my_optind]);
				exit(1);
			}
			if (UseConstraint) {
				if (SetAttributeByConstraint(constraint.Value(), argv[my_optind], argv[my_optind+1]) < 0) {
					fprintf(stderr,
							"Failed to set attribute \"%s\" by constraint: %s\n",
							argv[my_optind], constraint.Value());
					exit(1);
				}
			} else {
				if (SetAttribute(cluster, proc, argv[my_optind],
								argv[my_optind+1]) < 0) {
					fprintf(stderr,
							"Failed to set attribute \"%s\" for job %d.%d.\n",
							argv[my_optind], cluster, proc);
					exit(1);
				}
			}
			printf("Set attribute \"%s\".\n", argv[my_optind]);
		}
	}

	if (!DisconnectQ(q)) {
		fprintf(stderr,
				"Queue transaction failed.  No attributes were set.\n");
		exit(1);
	}

	return 0;
}

#include "daemon_core_stubs.h"
