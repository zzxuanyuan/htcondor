
#include "classad/classad.h"
#include "classad/classad_stl.h"
#include "classad_log.h"
#include "dc_service.h"

class Stream;

namespace condor_schedd {

class QueryProtocol : Service {

public:
	virtual ~QueryProtocol() {}

	static int execute(Stream *);

private:
	QueryProtocol(Stream *, int timeslice_ms=1000);
        classad_shared_ptr<classad::ExprTree> m_requirements;
        classad::References m_projection;
        ClassAdLog::filter_iterator m_it;
	Stream *m_stream;
	int m_timeslice_ms;
	int m_matched;
        bool m_unfinished_eom;
        bool m_registered_socket;

	int execute_internal();
        int finish(Stream *);
	bool sendJobErrorAd(int errorCode, const std::string &errorString);
	bool sendDone();
};

}

