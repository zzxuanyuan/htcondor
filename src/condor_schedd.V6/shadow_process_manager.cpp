
#include "shadow_process_manager.h"

shadow_rec::shadow_rec():
	universe(0),
	match(NULL),
	preempted(FALSE),
	conn_fd(-1),
	removed(FALSE),
	isZombie(FALSE),
	is_reconnect(false),
	keepClaimAttributes(false),
	recycle_shadow_stream(NULL),
	exit_already_handled(false)
{
	prev_job_id.proc = -1;
	prev_job_id.cluster = -1;
	job_id.proc = -1;
	job_id.cluster = -1;
}

shadow_rec::~shadow_rec()
{
	if( recycle_shadow_stream ) {
		dprintf(D_ALWAYS,"Failed to finish switching shadow to new job %d.%d\n",job_id.cluster,job_id.proc);
		delete recycle_shadow_stream;
		recycle_shadow_stream = NULL;
	}
}

ShadowProcessManager::ShadowProcessManager() {}

ShadowProcessManager::add_shadow_rec()
{

}

bool
ShadowProcessManager::hasProcess(shadow_rec* srec)
{

}

bool
ShadowProcessManager::isAlive(shadow_rec* srec)
{
	pid_t pid = getShadowPid(srec);
	return daemonCore->Is_Pid_Alive(pid);
}

int
ShadowProcessManager::sendSignal(shadow_rec* srec, int kill_sig, bool blocking)
{
		// TODO: Switch to asynchronous messaging.
	PROC_ID proc = srec->job_id;
	pid_t pid = getShadowPid(srec);
	classy_counted_ptr<DCShadowKillMsg> msg = new DCShadowKillMsg(pid, sig, proc);    
	daemonCore->Send_Signal_nonblocking(msg.get());

		// When this operation completes, the handler in DCShadowKillMsg
		// will take care of setting shadow_rec->preempted = TRUE.
}

pid_t
ShadowProcessManager::getShadowPid(shadow_rec* srec)
{

}

int
ShadowProcessManager::createShadow(shadow_rec* srec)
{

}


// TODO: make this stuff working; right now, I just have the stubs for it to compile
int
ShadowProcessManager::registerReaper()
{
	m_reaperId = daemonCore->Register_Reaper("reaper",
		(ReaperHandlercpp)&ShadowProcessManager::shadowExit,
		"shadowExit", this);
}

int
ShadowProcessManager::registerChildExitHandler(ChildExitHandler handler, Service* svc)
{
	m_childExitHandler = handler;
	m_childExitService = svc;
	m_reaperId = daemonCore->Register_Reaper("reaper",
		(ReaperHandlercpp)&ShadowProcessManager::shadowExit,
		"shadowExit", this);
	return 0;
}

int
ShadowProcessManager::shadowExit(int pid, int sig)
{
	assert(m_childExitService);
	assert(m_childExitHandler);
	// TODO: what happens if there's no such srec?
	ShadowRecVec shadows = m_pidshadow[pid];
	m_pidshadow.remove(pid);
	bool was_not_responding = daemonCore->Was_Not_Responding(pid);
	// TODO: Cleanup local data structs
	ShadowRecVec::const_iterator it;
	for (it = shadows.begin(); it != shadows.end(); ++it) {
		m_childExitService->*m_childExitHandler(srec, sig, was_not_responding);
	}
}

