
#ifndef _SHADOW_PROCESS_MANAGER_H_
#define _SHADOW_PROCESS_MANAGER_H_

#include "shadow_rec.h"
#include "proc.h"

#include <sys/types.h>
#include <ext/hash_map>
#include <vector>

namespace ext = __gnu_cxx;

struct eqshadow
{
        bool operator()(const shadow_rec& p1, const shadow_rec& p2) const
        {
		// TODO: how to handle this?
                return p1.job_id == p2.job_id;
        }
};

struct hashClassShadowRec
{
        unsigned int operator()(const shadow_rec& p) const
        {
                return hashFuncPROC_ID(p.job_id);
        }
};

struct eqpid
{
	unsigned int operator()(const pid_t& p1, const pid_t& p2) const
	{
		return p1 == p2;
	}
};

struct hashClassPid
{
	unsigned int operator()(const pid_t& p) const
	{
		return p;
	}
};

typedef ext::hash_map<shadow_rec*, pid_t, hashClassShadowRec, eqshadow> ShadowPidMap;
typedef ext::hash_map<pid_t, ShadowRecVec, hashClassPid, eqpid> PidShadowMap;
typedef ext::hash_map<PROC_ID, shadow_rec*> ProcShadowMap;
typedef void (Service::*ChildExitHandler)(shadow_rec*, int, bool);

// Forward def for schedd
class Scheduler;

class ShadowProcessManager {

public:
	ShadowProcessManager();
	void add_shadow_rec(shadow_rec*);
	int sendSignal(shadow_rec*, int, bool blocking=false);
	pid_t getPid(shadow_rec*);

	bool isAlive(shadow_rec*);
	bool hasProcess(shadow_rec*);
	int registerChildExitHandler(ChildExitHandler, Service*);
	int shadowExit(int, int);
	int create(shadow_rec*);
	int swapShadows(shadow_rec*, shadow_rec*);

private:

	ShadowPidMap m_shadowpid;
	PidShadowMap m_pidshadow;
	ProcShadowMap m_procshadow;

	// Related to handling shadow death
	int m_reaperId;
	ReaperHandlercpp m_childExitHandler;
	Service* m_childExitService;

};

#endif

