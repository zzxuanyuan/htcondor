
#include "condor_common.h"

#include "classad/collection.h"
#include "job_router_util.h"
#include "JobRouter.h"
#include "CondorError.h"
#include "subsystem_info.h"
#include "match_prefix.h"
#include "condor_distribution.h"
#include "condor_config.h"
#include "condor_version.h"
#include "Scheduler.h"

const char * MyName = NULL;
classad::ClassAdCollection * g_jobs = NULL;

using namespace htcondor;

void PREFAST_NORETURN
my_exit( int status )
{
	fflush(stdout);
	fflush(stderr);
	exit(status);
}


void
usage(int retval = 1)
{
	fprintf(stderr, "Apply one or more routes to a given set of ads.\n\n");
	fprintf(stderr, "Usage: %s [options]\n", MyName);
	fprintf(stderr,
		"    where [options] is one or more of:\n"
		"\t-help\t\tPrint this screen and exit\n"
		"\t-version\tPrint HTCondor version and exit\n"
		"\t-route <route>\t\tName of a route to use\n"
		"\t-jobads <file>\t<file> contains a list of jobs to route against <route>\n\t\t\tIf <file> is -, read from stdin\n"
		"\n"
		);
	my_exit(retval);
}


static const char * use_next_arg(const char * arg, const char * argv[], int & i)
{
	if (argv[i+1]) {
		return argv[++i];
	}

	fprintf(stderr, "-%s requires an argument\n", arg);
	my_exit(1);
	return NULL;
}


void TransformAds(JobRouter &router, StringList &routes);


int main(int argc, const char *argv[])
{
	MyName = argv[0];
	myDistro->Init( argc, argv );
	config();
	set_mySubSystem("TOOL", SUBSYSTEM_TYPE_TOOL);

	StringList bare_args;
	StringList job_files;
	StringList route_names;

	g_jobs = new classad::ClassAdCollection();

	for (int i = 1; i < argc; ++i)
	{
		if (is_dash_arg_prefix(argv[i], "help", 1)) {
			usage(0);
		} else if (is_dash_arg_prefix(argv[i], "version", 1)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			my_exit(0);
		} else if (is_dash_arg_prefix(argv[i], "route", 1)) {
			const char * routename = use_next_arg("route", argv, i);
			StringList tmp_route_names(routename);
			route_names.create_union(tmp_route_names, false);
		} else if (is_dash_arg_prefix(argv[i], "jobads", 1)) {
			const char * filename = use_next_arg("jobads", argv, i);
			job_files.append(filename);
		} else if (*argv[i] != '-') {
			// arguments that don't begin with "-" are bare arguments
			bare_args.append(argv[i]);
			continue;
		} else {
			fprintf(stderr, "ERROR: %s is not a valid argument\n", argv[i]);
			usage(1);
		}
	}

	if (route_names.isEmpty())
	{
		fprintf(stderr, "No routes specified.\n");
		return 1;
	}

	Scheduler* schedd = new Scheduler("JOB_ROUTER_SCHEDD1_SPOOL");
	Scheduler* schedd2 = NULL;
	std::string spool2;
	if (param(spool2, "JOB_ROUTER_SCHEDD2_SPOOL")) {
		schedd2 = new Scheduler("JOB_ROUTER_SCHEDD2_SPOOL");
	}

	JobRouter job_router(true);
	job_router.set_schedds(schedd, schedd2);
	job_router.init();

	if (!job_router.isEnabled())
	{
		fprintf(stderr, "JobRouter is disabled.\n");
		return 1;
	}

	if (!job_files.isEmpty()) {
		job_files.rewind();
		const char * filename;
		while ((filename = job_files.next()))
		{
			read_classad_file(filename, *g_jobs, NULL);
		}
	}

	const classad::View *root_view = g_jobs->GetView("root");
	if (root_view && (root_view->begin() != root_view->end())) {
		TransformAds(job_router, route_names);
	} else {
		fprintf(stdout, "There are no jobs to match\n");
	}
	return 0;
}


void
TransformAds(JobRouter &router, StringList &routes)
{
	CondorError errstack;
	classad::LocalCollectionQuery query;
	query.Bind(g_jobs);
	query.Query("root");
	std::string key;
	query.ToFirst();
	if (query.Current(key)) do
	{
		classad::ClassAd *ad = g_jobs->GetClassAd(key);
		if (!ad) {continue;}
		printf("Routing ad %s.\n", key.c_str());
		errstack.clear();
		if (!RouteAd(router, routes, *ad, errstack))
		{
			printf("Error when routing: %s.", errstack.getFullText().c_str());
			continue;
		}
		classad::PrettyPrint pp;
		std::string printed;
		pp.Unparse(printed, ad);
		printf("Transformed ad:\n%s\n", printed.c_str());
	}
	while (query.Next(key));
}

