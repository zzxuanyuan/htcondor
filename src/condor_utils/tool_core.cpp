#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "simple_arg.h"
#include "condor_version.h"
#include "tool_core.h"
#include "internet.h"
#include "match_prefix.h"
#include "basename.h"

const char *toolname = NULL;
const char *addr_arg = NULL;
const char *pool_arg = NULL;
const char *name_arg = NULL;
static USAGE_FUNC usage_func = NULL;

static void tool_version();

void tool_parse_command_line(int argc, char *argv[])
{
	if(!argv)
	{
		fprintf(stderr, "ERROR: Invalid argv pointer.\n");
		tool_exit(1);
	}

	toolname = condor_basename(argv[0]);
	int argno = 1;
	for(int i=1; i<argc; i++)
	{
		if(match_prefix(argv[i], "-a"))
		{
			if(!strstr(argv[i], "-addr"))
				continue;
			++i;
			if(!argv[i])
			{
				option_needs_arg("-addr");
			}
			addr_arg = argv[i];
			if(!is_valid_sinful(addr_arg))
			{
				fprintf(stderr, "%s: invalid address %s\n"
						 "Address must be of the form \"<111.222.333.444:555>\n"
						 "   where 111.222.333.444 is the ip address and 555 is the port\n"
						 "   you wish to connect to (the punctuation is important).\n", 
						 toolname, addr_arg);
				tool_exit(1);
			}
		}
		else if(match_prefix(argv[i], "-d") || match_prefix(argv[i], "-D"))
		{
			if((strlen(argv[i]) == 2) || match_prefix(argv[i], "-debug"))
			{
				Termlog = 1;
				dprintf_config ("TOOL", get_param_functions());
				int next = i + 1;
				if(argv[next] && !match_prefix(argv[next], "-"))
				{
					set_debug_flags(argv[next]);
					i = next;
				}
			}
		}
		else if(match_prefix(argv[i], "-h"))
		{
			if((strlen(argv[i]) == 2) || match_prefix(argv[i], "-help"))
				tool_usage(0);
		}
		else if(match_prefix(argv[i], "-n"))
		{
			if((strlen(argv[i]) == 2) || match_prefix(argv[i], "-name"))
			{
				if(name_arg)
				{
					fprintf(stderr, "ERROR: only one '-n' arg my be provided\n\n");
					tool_usage(1);
				}
				++i;
				if(!argv[i])
				{
					option_needs_arg("-name");
				}
				name_arg = argv[i];
			}
		}
		else if(match_prefix(argv[i], "-pool"))
		{
			++i;
			if(!argv[i])
			{
				option_needs_arg("-pool");
			}
			pool_arg = argv[i];
		}
		else if(match_prefix(argv[i], "-v"))
		{
			if(strlen(argv[i]) == 2 || match_prefix(argv[i], "-version"))
				tool_version();
		}
	}
}

void tool_version()
{
	printf("%s\n%s\n", CondorVersion(), CondorPlatform());
	exit(0);
}

void tool_usage(int exitcode = 0)
{
	if(!usage_func)
	{
		fprintf(stderr,"Usage: %s [options]\n", toolname);
		fprintf(stderr,"Where options are:\n");
		fprintf(stderr,"    -help             Display options\n");
		fprintf(stderr,"    -version          Display Condor version\n");
		fprintf(stderr,"    -pool <hostname>  Use this central manager\n");
		fprintf(stderr,"    -debug            Show extra debugging info\n");
		fprintf(stderr,"    -address <addr>   Connect to this IP address\n");
		tool_exit(exitcode);
	}

	usage_func(exitcode);
}

void set_usage(USAGE_FUNC u_func)
{
	usage_func = u_func;
}

void tool_exit(int exit_code)
{
	fflush(stdout);
	fflush(stderr);
	exit(exit_code);
}

void option_needs_arg(const char* option)
{
	fprintf(stderr, "ERROR: Option '%s' requires an argument\n\n", option);
	tool_usage(1);
}