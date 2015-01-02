
#include "condor_common.h"

#include "classad/classad.h"
#include "classad/collection.h"
#include "compat_classad.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "JobRouter.h"

class CondorQClassAdFileParseHelper : public compat_classad::ClassAdFileParseHelper
{
 public:
	virtual int PreParse(std::string & line, ClassAd & ad, FILE* file);
	virtual int OnParseError(std::string & line, ClassAd & ad, FILE* file);
	std::string schedd_name;
	std::string schedd_addr;
};

// this method is called before each line is parsed. 
// return 0 to skip (is_comment), 1 to parse line, 2 for end-of-classad, -1 for abort
int CondorQClassAdFileParseHelper::PreParse(std::string & line, ClassAd & /*ad*/, FILE* /*file*/)
{
	// treat blank lines as delimiters.
	if (line.size() <= 0) {
		return 2; // end of classad.
	}

	// standard delimitors are ... and ***
	if (starts_with(line,"\n") || starts_with(line,"...") || starts_with(line,"***")) {
		return 2; // end of classad.
	}

	// the normal output of condor_q -long is "-- schedd-name <addr>"
	// we want to treat that as a delimiter, and also capture the schedd name and addr
	if (starts_with(line, "-- ")) {
		if (starts_with(line.substr(3), "Schedd:")) {
			schedd_name = line.substr(3+8);
			size_t ix1 = schedd_name.find_first_of(": \t\n");
			if (ix1 != std::string::npos) {
				size_t ix2 = schedd_name.find_first_not_of(": \t\n", ix1);
				if (ix2 != std::string::npos) {
					schedd_addr = schedd_name.substr(ix2);
					ix2 = schedd_addr.find_first_of(" \t\n");
					if (ix2 != std::string::npos) {
						schedd_addr = schedd_addr.substr(0,ix2);
					}
				}
				schedd_name = schedd_name.substr(0,ix1);
			}
		}
		return 2;
	}


	// check for blank lines or lines whose first character is #
	// tell the parser to skip those lines, otherwise tell the parser to
	// parse the line.
	for (size_t ix = 0; ix < line.size(); ++ix) {
		if (line[ix] == '#' || line[ix] == '\n')
			return 0; // skip this line, but don't stop parsing.
		if (line[ix] != ' ' && line[ix] != '\t')
			break;
	}
	return 1; // parse this line
}

// this method is called when the parser encounters an error
// return 0 to skip and continue, 1 to re-parse line, 2 to quit parsing with success, -1 to abort parsing.
int CondorQClassAdFileParseHelper::OnParseError(std::string & line, ClassAd & ad, FILE* file)
{
	// when we get a parse error, skip ahead to the start of the next classad.
	int ee = this->PreParse(line, ad, file);
	while (1 == ee) {
		if ( ! readLine(line, file, false) || feof(file)) {
			ee = 2;
			break;
		}
		ee = this->PreParse(line, ad, file);
	}
	return ee;
}

bool
read_classad_file(const char *filename, classad::ClassAdCollection &classads, const char * constr)
{
	bool success = false;

	FILE* file = NULL;
	bool  read_from_stdin = false;
	if (MATCH == strcmp(filename, "-")) {
		read_from_stdin = true;
		file = stdin;
	} else {
		file = safe_fopen_wrapper_follow(filename, "r");
	}
	if (file == NULL) {
		fprintf(stderr, "Can't open file of job ads: %s\n", filename);
		return false;
	} else {
		// this helps us parse the output of condor_q -long
		CondorQClassAdFileParseHelper parse_helper;

		for (;;) {
			ClassAd* classad = new ClassAd();

			int error;
			bool is_eof;
			int cAttrs = classad->InsertFromFile(file, is_eof, error, &parse_helper);

			bool include_classad = cAttrs > 0 && error >= 0;
			if (include_classad && constr) {
				classad::Value val;
				if (classad->EvaluateExpr(constr,val)) {
					if ( ! val.IsBooleanValueEquiv(include_classad)) {
						include_classad = false;
					}
				}
			}
			if (include_classad) {
				int cluster, proc = -1;
				if (classad->LookupInteger(ATTR_CLUSTER_ID, cluster) && classad->LookupInteger(ATTR_PROC_ID, proc)) {
					std::string key;
					formatstr(key, "%d,%d", cluster, proc);
					if (classads.AddClassAd(key, classad)) {
						classad = NULL; // this is now owned by the collection.
					}
				} else {
					fprintf(stderr, "Skipping ad because it doesn't have a ClusterId and/or ProcId attribute\n");
				}
			}
			if (classad) {
				delete classad;
			}

			if (is_eof) {
				success = true;
				break;
			}
			if (error < 0) {
				success = false;
				break;
			}
		}

		if ( ! read_from_stdin) { fclose(file); }
	}
	return success;
}

bool
RouteAd(JobRouter &router, StringList &routes, classad::ClassAd &ad, CondorError &errstack)
{
	routes.rewind();
	const char *route_name;
	while ((route_name = routes.next()))
	{
		JobRoute *route = router.GetRouteByName(route_name);
		if (!route)
		{
			errstack.pushf("ROUTER", 1, "No route by name %s.", route_name);
			return false;
		}
		classad::ExprTree *reqs = route->RouteRequirementExpr();
		if (reqs)
		{
			classad::EvalState state;
			state.SetScopes(&ad);
			classad::Value val;
			if (!reqs->Evaluate(state, val) || val.IsErrorValue())
			{
				errstack.pushf("ROUTER", 2, "Requirement (%s) for route %s failed when evaluated.", route->RouteRequirementsString(), route_name);
				return false;
			}
			bool passes = false;
			if (!val.IsBooleanValueEquiv(passes))
			{
				errstack.pushf("ROUTER", 3, "Requirement (%s) for route %s does not evaluate to a boolean.", route->RouteRequirementsString(), route_name);
			}
			if (!passes) {continue;}
		}
		route->ApplyRoutingJobEdits(&ad);
	}
	return true;
}

