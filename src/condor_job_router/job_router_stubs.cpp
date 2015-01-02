
#include "condor_common.h"

#include "condor_config.h"
#include "Scheduler.h"
#include "JobRouter.h"
#include "submit_job.h"
#include "condor_event.h"
#include "write_user_log.h"

using namespace htcondor;

#ifdef __GNUC__
#if __GNUC__ >= 4
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #pragma GCC diagnostic ignored "-Wunused-variable"
  #pragma GCC diagnostic ignored "-Wunused-value"
#endif
#endif

extern classad::ClassAdCollection * g_jobs;

// this is how we will feed job ad's into the router
//
class JobLogMirror {
public:
	JobLogMirror(char const *spool_param=NULL) {}
	~JobLogMirror() {}

	void init() {}
	void config() {}
	void stop() {}

private:
};

Scheduler::Scheduler(char const *_alt_spool_param /*=NULL*/, int id /*=0*/)
	: m_consumer(NULL)
	, m_mirror(NULL)
	, m_id(id)
{ 
}

Scheduler::~Scheduler()
{
	delete m_mirror;
	m_mirror = NULL;
	m_consumer = NULL;
}

classad::ClassAdCollection *Scheduler::GetClassAds()
{
	if (m_id == 0) {
		return g_jobs;
	}
	return NULL;
}

void Scheduler::init() {  m_mirror->init(); }
void Scheduler::config() { m_mirror->config(); }
void Scheduler::stop()  { m_mirror->stop(); }
int  Scheduler::id() { return m_id; }


// needed by JobRouter
unsigned int hashFuncStdString( std::string const & key)
{
    return hashFuncChars(key.c_str());
}

// 
JobRouterHookMgr::JobRouterHookMgr() : HookClientMgr(), NUM_HOOKS(0), UNDEFINED("UNDEFINED"), m_default_hook_keyword(NULL), m_hook_paths(MyStringHash) {}
JobRouterHookMgr::~JobRouterHookMgr() {};
bool JobRouterHookMgr::initialize() { reconfig(); return true; /*HookClientMgr::initialize()*/; }
bool JobRouterHookMgr::reconfig() { m_default_hook_keyword = param("JOB_ROUTER_HOOK_KEYWORD"); return true; }

int JobRouterHookMgr::hookTranslateJob(RoutedJob* r_job, std::string &route_info) { return 1; }
int JobRouterHookMgr::hookUpdateJobInfo(RoutedJob* r_job) { return 1; }
int JobRouterHookMgr::hookJobExit(RoutedJob* r_job) { return 1; }
int JobRouterHookMgr::hookJobCleanup(RoutedJob* r_job) { return 1; }

std::string
JobRouterHookMgr::getHookKeyword(classad::ClassAd ad)
{
	std::string hook_keyword;

	if (false == ad.EvaluateAttrString(ATTR_HOOK_KEYWORD, hook_keyword))
	{
		if ( m_default_hook_keyword ) {
			hook_keyword = m_default_hook_keyword;
		}
	}
	return hook_keyword;
}




ClaimJobResult claim_job(int cluster, int proc, MyString * error_details, const char * my_identity)
{
	return CJR_OK;
}



ClaimJobResult claim_job(classad::ClassAd const &ad, const char * pool_name, const char * schedd_name, int cluster, int proc, MyString * error_details, const char * my_identity, bool target_is_sandboxed)
{
	return CJR_OK;
}

bool yield_job(bool done, int cluster, int proc, classad::ClassAd const &job_ad, MyString * error_details, const char * my_identity, bool target_is_sandboxed, bool release_on_hold, bool *keep_trying) {
	return true;
}


bool yield_job(classad::ClassAd const &ad,const char * pool_name,
	const char * schedd_name, bool done, int cluster, int proc,
	MyString * error_details, const char * my_identity, bool target_is_sandboxed,
        bool release_on_hold, bool *keep_trying)
{
	return true;
}

bool submit_job( ClassAd & src, const char * schedd_name, const char * pool_name, bool is_sandboxed, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	return true;
}

bool submit_job( classad::ClassAd & src, const char * schedd_name, const char * pool_name, bool is_sandboxed, int * cluster_out /*= 0*/, int * proc_out /*= 0 */)
{
	return true;
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Assumes the existance of an open qmgr connection (via ConnectQ).
*/
bool push_dirty_attributes(classad::ClassAd & src)
{
	return true;
}

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Establishes (and tears down) a qmgr connection.
*/
bool push_dirty_attributes(classad::ClassAd & src, const char * schedd_name, const char * pool_name)
{
	return true;
}

/*
	Update src in the queue so that it ends up looking like dest.
    This handles attribute deletion as well as change of value.
	Assumes the existance of an open qmgr connection (via ConnectQ).
*/
bool push_classad_diff(classad::ClassAd & src,classad::ClassAd & dest)
{
	return true;
}

/*
	Update src in the queue so that it ends up looking like dest.
    This handles attribute deletion as well as change of value.
	Establishes (and tears down) a qmgr connection.
*/
bool push_classad_diff(classad::ClassAd & src, classad::ClassAd & dest, const char * schedd_name, const char * pool_name)
{
	return true;
}

bool finalize_job(classad::ClassAd const &ad,int cluster, int proc, const char * schedd_name, const char * pool_name, bool is_sandboxed)
{
	return true;
}

bool remove_job(classad::ClassAd const &ad, int cluster, int proc, char const *reason, const char * schedd_name, const char * pool_name, MyString &error_desc)
{
	return true;
}

bool InitializeUserLog( classad::ClassAd const &job_ad, WriteUserLog *ulog, bool *no_ulog )
{
	return true;
}

bool InitializeAbortedEvent( JobAbortedEvent *event, classad::ClassAd const &job_ad )
{
	return true;
}

bool InitializeTerminateEvent( TerminatedEvent *event, classad::ClassAd const &job_ad )
{
	return true;
}

bool InitializeHoldEvent( JobHeldEvent *event, classad::ClassAd const &job_ad )
{
	return true;
}

bool WriteEventToUserLog( ULogEvent const &event, classad::ClassAd const &ad )
{
	return true;
}

bool WriteTerminateEventToUserLog( classad::ClassAd const &ad )
{
	return true;
}

bool WriteAbortEventToUserLog( classad::ClassAd const &ad )
{
	return true;
}

bool WriteHoldEventToUserLog( classad::ClassAd const &ad )
{
	return true;
}



// The following is copied from gridmanager/basejob.C
// TODO: put the code into a shared file.

void
EmailTerminateEvent(ClassAd * job_ad, bool   /*exit_status_known*/)
{
}

bool EmailTerminateEvent( classad::ClassAd const &ad )
{
	return true;
}

