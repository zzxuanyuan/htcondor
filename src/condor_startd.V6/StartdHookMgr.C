/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "startd.h"
#include "directory.h"



// // // // // // // // // // // // 
// FetchWorkMgr
// // // // // // // // // // // // 

FetchWorkMgr::FetchWorkMgr()
	: HookClientMgr()
{
	dprintf( D_FULLDEBUG, "Instantiating a FetchWorkMgr\n" );
	m_hook_fetch_work = NULL;
	m_hook_claim_response = NULL;
	m_hook_claim_destroy = NULL;
}


FetchWorkMgr::~FetchWorkMgr()
{
	dprintf( D_FULLDEBUG, "Destroying a FetchWorkMgr\n" );
		// TODO-fetch: clean up m_fetch_clients, too?

		// Delete our copies of the paths for each hook.
	clearHookPaths();
}


void
FetchWorkMgr::clearHookPaths()
{
	if (m_hook_fetch_work) {
		free(m_hook_fetch_work);
		m_hook_fetch_work = NULL;
	}
	if (m_hook_claim_response) {
		free(m_hook_claim_response);
		m_hook_claim_response = NULL;
	}
	if (m_hook_claim_destroy) {
		free(m_hook_claim_destroy);
		m_hook_claim_destroy = NULL;
	}
}


bool
FetchWorkMgr::initialize()
{
	reconfig();
    return HookClientMgr::initialize();
}


bool
FetchWorkMgr::reconfig()
{
		// Clear out our old copies of each hook's path.
	clearHookPaths();

	m_hook_fetch_work = initHookPath("STARTD_FETCH_WORK_HOOK");
	m_hook_claim_response = initHookPath("STARTD_CLAIM_RESPONSE_HOOK");
	m_hook_claim_destroy = initHookPath("STARTD_CLAIM_DESTROYED_HOOK");

	return true;
}


char*
FetchWorkMgr::initHookPath( const char* hook_param )
{
	char* tmp = param(hook_param);
	if (tmp) {
		StatInfo si(tmp);
		if (si.Error() != SIGood) {
			int si_errno = si.Errno();
			dprintf(D_ALWAYS, "ERROR: invalid path specified for %s (%s): "
					"stat() failed with errno %d (%s)\n",
					hook_param, tmp, si_errno, strerror(si_errno));
			free(tmp);
			return NULL;
		}
		mode_t mode = si.GetMode();
		if (mode & S_IWOTH) {
			dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
					"is world-writable! Refusing to use.\n",
					hook_param, tmp);
			free(tmp);
			return NULL;
		}
		if (!si.IsExecutable()) {
			dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
					"is not executable.\n", hook_param, tmp);
			free(tmp);
			return NULL;
		}
			// TODO: forbid symlinks, too?
		
			// Now, make sure the parent directory isn't world-writable.
		StatInfo dir_si(si.DirPath());
		mode_t dir_mode = dir_si.GetMode();
		if (dir_mode & S_IWOTH) {
			dprintf(D_ALWAYS, "ERROR: path specified for %s (%s) "
					"is a world-writable directory (%s)! Refusing to use.\n",
					hook_param, tmp, si.DirPath());
			free(tmp);
			return NULL;
		}
	}

	if (tmp) {
		dprintf(D_ALWAYS, "Hook configuration: %s is \"%s\"\n",
				hook_param, tmp);
	}
	else {
		dprintf(D_FULLDEBUG, "Hook configuration: %s is not defined\n",
				hook_param);
	}
		// If we got this far, we've either got a valid hook or it
		// wasn't defined. Either way, we can just return that directly.
	return tmp;
}


FetchClient*
FetchWorkMgr::buildFetchClient(Resource* rip)
{
	FetchClient* new_client = new FetchClient(rip, m_hook_fetch_work);
	if (new_client) {
		m_fetch_clients.Append(new_client);
	}
	return new_client;
}


bool
FetchWorkMgr::removeFetchClient(FetchClient* fetch_client)
{
	if (m_fetch_clients.Delete(fetch_client)) {
		remove((HookClient*)fetch_client);
		delete fetch_client;
		return true;
	}
	return false;
}


bool
FetchWorkMgr::fetchWork(Resource* rip)
{
	if (!rip->willingToRun(NULL)) {
			// START locally evaluates to FALSE, give up now.
		return false;
	}
	FetchClient* fetch_client = buildFetchClient(rip);
	if (!fetch_client) {
		return false;
	}
	return fetch_client->startFetch();
}


bool
FetchWorkMgr::handleFetchResult(FetchClient* fetch_client)
{
	ClassAd* job_ad = NULL;
	Resource* rip = fetch_client->m_rip;
	float rank = 0;
	bool willing = true;

	if (!(job_ad = fetch_client->reply())) {
			// No work or error reading the reply, bail out.
		removeFetchClient(fetch_client);
			// Try other hooks?
		return false;
	}
	
		// If we got here, we've got a ClassAd describing the job, so
		// see if this slot is willing to run it.
	if (!rip->willingToRun(job_ad)) {
		willing = false;
	}
	else {
		rank = compute_rank(rip->r_classad, job_ad);
		rip->dprintf(D_FULLDEBUG, "Rank of this fetched claim is: %f\n", rank);
		if (rip->state() == claimed_state) {
				// Make sure it's got a high enough rank to preempt us.
			if (rank <= rip->r_cur->rank()) {
					// For fetched jobs, there's no user priority
					// preemption, so the newer claim has to have higher,
					// not just equal rank.
				rip->dprintf(D_ALWAYS, "Fetched claim doesn't have sufficient rank, refusing.\n");
				willing = false;
			}
		}
	}

	if (!willing) {
			// TODO-fetch: tell the fetch client about this.
		removeFetchClient(fetch_client);
			// TODO-fetch: matchmaking on other slots?
		return false;
	}

		// We're ready to start running the job, so we need to update
		// the current Claim and Client objects to remember this work.
	rip->createFetchClaim(job_ad, rank);

		// Now, depending on our current state, initiate a state change.
	if (rip->state() == claimed_state) {
			// If we're already claimed, it means we're going to
			// preempt the current job first.
		rip->dprintf(D_ALWAYS, "State change: preempting claim based on "
					 "machine rank of fetched work.\n");

			// Force resource to take note of the preempting claim.
			// This results in a reversible transition to the
			// retiring activity.  If the preempting claim goes
			// away before the current claim retires, the current
			// claim can unretire and continue without any disturbance.
		rip->eval_state();
	}
	else {
			// Start moving towards Claimed so we actually spawn the job.
		dprintf(D_ALWAYS, "State change: Finished fetching work successfully\n");
		rip->r_state->set_destination(claimed_state);
	}

	return true;
}


bool
FetchWorkMgr::claimRemoved(Resource* /* rip */)
{
		// TODO-fetch
	return true;
}


// // // // // // // // // // // // 
// FetchClient class
// // // // // // // // // // // // 

FetchClient::FetchClient(Resource* rip, const char* hook_path)
	: HookClient(hook_path)
{
	m_rip = rip;
	m_job_ad = NULL;
}


FetchClient::~FetchClient()
{
	if (m_job_ad) {
		delete m_job_ad;
	}
}


bool
FetchClient::startFetch()
{
	ASSERT(m_rip);
	ArgList args;
	ClassAd slot_ad;
    m_rip->publish(&slot_ad, A_ALL_PUB);
	MyString slot_ad_txt;
	slot_ad.sPrint(slot_ad_txt);
	resmgr->m_fetch_work_mgr->spawn(this, args, &slot_ad_txt);
	return true;
}


ClassAd*
FetchClient::reply()
{
	return m_job_ad;
}


void
FetchClient::hookExited(int exit_status) {
	HookClient::hookExited(exit_status);
	if (m_std_err.Length()) {
		dprintf(D_ALWAYS,
				"Warning, hook %s (pid %d) printed to stderr: %s\n",
				m_hook_path, (int)m_pid, m_std_err.Value());
	}
	if (m_std_out.Length()) {
		ASSERT(m_job_ad == NULL);
		m_job_ad = new ClassAd();
		m_std_out.Tokenize();
		const char* hook_line = NULL;
		while ((hook_line = m_std_out.GetNextToken("\n", true))) {
			if (!m_job_ad->Insert(hook_line)) {
				dprintf(D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
						"ignoring invalid hook output\n", hook_line);
					// TODO-pipe howto abort?
				return;
			}
		}
	}
	else {
		dprintf(D_FULLDEBUG, "Hook %s (pid %d) returned no data\n",
				m_hook_path, (int)m_pid);
	}
		// Finally, let the work manager know this fetch result is done.
	resmgr->m_fetch_work_mgr->handleFetchResult(this);
}
