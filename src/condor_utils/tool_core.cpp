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

int tool_parse_command_line(int pos, char *argv[])
{
	if(!argv)
	{
		fprintf(stderr, "ERROR: Invalid argv pointer.\n");
		tool_exit();
	}

	const char* arg = argv[pos] + 1;

	if(tool_is_arg(arg, "addr", 4))
	{
		++pos;

		if(!argv[pos])
			option_needs_arg(argv[pos]);

		addr_arg = argv[pos];
		if(!is_valid_sinful(addr_arg))
		{
			fprintf(stderr, "%s: invalid address %s\n"
				"Address must be of the form \"<111.222.333.444:555>\n"
				"   where 111.222.333.444 is the ip address and 555 is the port\n"
				"   you wish to connect to (the punctuation is important).\n", 
				toolname, addr_arg);
			tool_exit(1);
		}

		return 2;
	}
	if(tool_is_arg(arg, "debug"))
	{

		Termlog = 1;
		dprintf_config ("TOOL", get_param_functions());
		
		return 1;
	}
	if(tool_is_arg(arg, "D"))
	{
		++pos;
		if(!argv[pos])
		{
			fprintf(stderr, "Error: Argument -D requires a list of flags as an argument.\n"
				"Extra Info: You need to specify debug flags "
				"as a quoted string. Common flags are D_ALL, and "
				"D_ALWAYS.");
			tool_exit(1);
		}
		set_debug_flags(argv[pos]);
		return 2;
	}
	if(tool_is_arg(arg, "help"))
	{
		tool_usage(0);
	}
	else if(tool_is_arg(arg, "name"))
	{
		if(name_arg)
		{
			fprintf(stderr, "ERROR: only one '-n' arg my be provided\n\n");
			tool_usage(1);
		}
		++pos;
		if(!argv[pos])
		{
			option_needs_arg("-name");
		}
		name_arg = argv[pos];

		return 2;
	}
	else if(tool_is_arg(arg, "pool"))
	{
		++pos;
		if(!argv[pos])
		{
			option_needs_arg("-pool");
		}
		pool_arg = argv[pos];

		return 2;
	}
	else if(tool_is_arg(arg, "version", 4))
	{
		tool_version();
	}

	return 0;
}

void tool_version()
{
	printf("%s\n%s\n", CondorVersion(), CondorPlatform());
	tool_exit(0);
}

void tool_usage(int exitcode)
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
	tool_usage();
}

bool tool_is_arg(const char * parg, const char * pval, int must_match_length) {
   if (*parg == '-') ++parg; // allow -- as well as - for an arg prefix.
   if (*parg != *pval) return false;

   // do argument matching based on a minimum prefix. when we run out
   // of characters in parg we must be at a \0 or no match and we
   // must have matched at least must_match_length characters or no match
   int match_length = 0;
   while (*parg == *pval) {
      ++match_length;
      ++parg; ++pval;
      if (!*pval) break;
   }
   if (*parg) return false;
   if (must_match_length < 0) return (*pval == 0);
   return match_length >= must_match_length;
}