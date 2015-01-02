
#ifndef __JOB_ROUTER_UTIL_H_
#define __JOB_ROUTER_UTIL_H_

namespace classad {
class ClassAdCollection;
}
class CondorError;
class StringList;
class JobRouter;

bool
read_classad_file(const char *filename, classad::ClassAdCollection &classads, const char * constr);

bool
RouteAd(JobRouter &router, StringList &routes, classad::ClassAd &ad, CondorError &errstack);

#endif

