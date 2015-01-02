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
//#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "match_prefix.h"
#include "condor_distribution.h"
#include "write_user_log.h"
#include "dprintf_internal.h" // for dprintf_set_outputs
#include "condor_version.h"
#include "job_router_util.h"

#include "Scheduler.h"
#include "JobRouter.h"
#include "submit_job.h"

using namespace htcondor;

#ifdef __GNUC__
#if __GNUC__ >= 4
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #pragma GCC diagnostic ignored "-Wunused-variable"
  #pragma GCC diagnostic ignored "-Wunused-value"
#endif
#endif


//JobRouter *job_router = NULL;

//-------------------------------------------------------------
const char * MyName = NULL;
classad::ClassAdCollection * g_jobs = NULL;

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
	fprintf(stderr, "Usage: %s [options]\n", MyName);
	fprintf(stderr,
		"    where [options] is one or more of:\n"
		"\t-help\t\tPrint this screen and exit\n"
		"\t-version\tPrint HTCondor version and exit\n"
		"\t-config\t\tPrint configured routes\n"
		"\t-match-jobs\tMatch jobs to routes and print the first match\n"
		"\t-ignore-prior-routing\tRemove routing attributes from the job ClassAd and set JobStatus to IDLE before matching\n"
		"\t-jobads <file>\tWhen operation requires job ClassAds, Read them from <file>\n\t\t\tIf <file> is -, read from stdin\n"
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
	//usage(1);
	my_exit(1);
	return NULL;
}

static StringList saved_dprintfs;
static void print_saved_dprintfs(FILE* hf)
{
	saved_dprintfs.rewind();
	const char * line;
	while ((line = saved_dprintfs.next())) {
		fprintf(hf, "%s", line);
	}
	saved_dprintfs.clearAll();
}


bool g_silence_dprintf = false;
bool g_save_dprintfs = false;
void _dprintf_intercept(int cat_and_flags, int hdr_flags, DebugHeaderInfo & info, const char* message, DebugFileInfo* dbgInfo)
{
	//if (cat_and_flags & D_FULLDEBUG) return;
	if (g_silence_dprintf) {
		if (g_save_dprintfs) {
			saved_dprintfs.append(message);
		}
		return;
	}
	if (is_arg_prefix("JobRouter", message, 9)) { message += 9; if (*message == ':') ++message; if (*message == ' ') ++message; }
	int cch = strlen(message);
	fprintf(stdout, &"\n%s"[(cch > 150) ? 0 : 1], message);
}

static void dprintf_set_output_intercept (
	int cat_and_flags,
	DebugOutputChoice choice,
	DprintfFuncPtr fn)
{

	dprintf_output_settings my_output;
	my_output.choice = choice;
	my_output.accepts_all = true;
	my_output.logPath = ">BUFFER";	// this is a special case of intercept
	my_output.HeaderOpts = (cat_and_flags & ~(D_CATEGORY_RESERVED_MASK | D_FULLDEBUG | D_VERBOSE_MASK));
	my_output.VerboseCats = (cat_and_flags & (D_FULLDEBUG | D_VERBOSE_MASK)) ? choice : 0;
	dprintf_set_outputs(&my_output, 1);

	// throw away any dprintf messages up to this point.
	bool was_silent = g_silence_dprintf;
	g_silence_dprintf = true;
	dprintf_WriteOnErrorBuffer(NULL, true);
	g_silence_dprintf = was_silent;

	// PRAGMA_REMIND("tj: fix this hack when the dprintf code has a proper way to register an intercept.")
	// HACK!!! there is no properly exposed way to set an intercept function, so for now, we reach into
	// the dprintf internal data structures and just set one. 
	extern std::vector<DebugFileInfo> * DebugLogs;
	if (DebugLogs) { (*DebugLogs)[0].dprintfFunc = fn; }
}


int main(int argc, const char *argv[])
{
	MyName = argv[0];
    myDistro->Init( argc, argv );
	config();
	set_mySubSystem("TOOL", SUBSYSTEM_TYPE_TOOL);

	StringList bare_args;
	StringList job_files;
	bool dash_config = false;
	bool dash_match_jobs = false;
	bool dash_diagnostic = false;
	bool dash_d_always = true;
	bool dash_ignore_prior_routing = false;
	//bool dash_d_fulldebug = false;

	g_jobs = new classad::ClassAdCollection();

	for (int i = 1; i < argc; ++i) {

		const char * pcolon = NULL;
		if (is_dash_arg_prefix(argv[i], "help", 1)) {
			usage(0);
		} else if (is_dash_arg_prefix(argv[i], "version", 1)) {
			printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
			my_exit(0);
		} else if (is_dash_arg_colon_prefix(argv[i], "debug", &pcolon, 1)) {
			dash_d_always = true;
			if (pcolon && (is_arg_prefix(pcolon+1, "verbose", 1) || is_arg_prefix(pcolon+1, "full", 1))) {
				//dash_d_fulldebug = true;
			}
		} else if (is_dash_arg_colon_prefix(argv[i], "diagnostic", &pcolon, 4)) {
			dash_diagnostic = true;
		} else if (is_dash_arg_prefix(argv[i], "config", 2)) {
			dash_config = true;
		} else if (is_dash_arg_prefix(argv[i], "match-jobs", 2)) {
			dash_match_jobs = true;
		} else if (is_dash_arg_prefix(argv[i], "ignore-prior-routing", 2)) {
			dash_ignore_prior_routing = true;
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


	// tell the dprintf code to had messages to our callback function.
	unsigned int cat_and_flags = D_FULLDEBUG | D_CAT;
	//if (dash_d_fulldebug) { cat_and_flags |= D_FULLDEBUG; }
	DebugOutputChoice choice=1<<D_ERROR;
	if (dash_d_always || dash_diagnostic) { choice |= 1<<D_ALWAYS; }
	dprintf_set_output_intercept(cat_and_flags, choice, _dprintf_intercept);


	// before we call init() for the router, we need to install a pseudo-schedd object
	// so that init() doesn't install a real schedd object.
	Scheduler* schedd = new Scheduler("JOB_ROUTER_SCHEDD1_SPOOL");
	Scheduler* schedd2 = NULL;
	std::string spool2;
	if (param(spool2, "JOB_ROUTER_SCHEDD2_SPOOL")) {
		schedd2 = new Scheduler("JOB_ROUTER_SCHEDD2_SPOOL");
	}

	g_silence_dprintf = dash_diagnostic ? false : true;
	g_save_dprintfs = true;
	JobRouter job_router(true);
	job_router.set_schedds(schedd, schedd2);
	job_router.init();
	g_silence_dprintf = false;
	g_save_dprintfs = false;

	// if the job router is not enabled at this point, say so, and print out saved dprintfs.
	if ( ! job_router.isEnabled()) {
		print_saved_dprintfs(stderr);
		fprintf(stderr, "JobRouter is disabled.\n");
	}

	if (dash_config) {
		fprintf (stdout, "\n\n");
		job_router.dump_routes(stdout);
	}

	if ( ! job_files.isEmpty()) {
		job_files.rewind();
		const char * filename;
		while ((filename = job_files.next())) {
			read_classad_file(filename, *g_jobs, NULL);
		}
	}

	if (dash_ignore_prior_routing) {
		// strip attributes that indicate that the job has already been routed
		std::string key;
		classad::LocalCollectionQuery query;
		query.Bind(g_jobs);
		query.Query("root");
		query.ToFirst();
		if (query.Current(key)) do {
			classad::ClassAd *ad = g_jobs->GetClassAd(key);
			ad->Delete("Managed");
			ad->Delete("RoutedBy");
			ad->Delete("StageInStart");
			ad->Delete("StageInFinish");
			ad->InsertAttr("JobStatus", 1); // pretend job is idle so that it will route.
		} while (query.Next(key));
	}

	if (dash_match_jobs) {
		fprintf(stdout, "\nMatching jobs against routes to find candidate jobs.\n");
		const classad::View *root_view = g_jobs->GetView("root");
		if (root_view && (root_view->begin() != root_view->end())) {
			job_router.GetCandidateJobs();
		} else {
			fprintf(stdout, "There are no jobs to match\n");
		}
	}

	return 0;
}

