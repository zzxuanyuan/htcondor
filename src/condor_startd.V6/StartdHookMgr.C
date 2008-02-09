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
#include "basename.h"
#include "status_string.h"


// // // // // // // // // // // // 
// FetchWorkMgr
// // // // // // // // // // // // 

FetchWorkMgr::FetchWorkMgr()
	: HookClientMgr(),
	  NUM_HOOKS(3),
	  UNDEFINED((char*)1),
	  m_slot_hook_keywords(resmgr->numSlots()),
	  m_keyword_hook_paths(MyStringHash)
{
	dprintf( D_FULLDEBUG, "Instantiating a FetchWorkMgr\n" );
	m_reaper_ignore_id = -1;
	m_slot_hook_keywords.setFiller(NULL);
	m_startd_job_hook_keyword = NULL;
}


FetchWorkMgr::~FetchWorkMgr()
{
	dprintf( D_FULLDEBUG, "Deleting the FetchWorkMgr\n" );
		// TODO-fetch: clean up m_fetch_clients, too?

		// Delete our copies of the paths for each hook.
	clearHookPaths();

	if (m_reaper_ignore_id != -1) {
		daemonCore->Cancel_Reaper(m_reaper_ignore_id);
	}
}


void
FetchWorkMgr::clearHookPaths()
{
	if (m_startd_job_hook_keyword) {
		free(m_startd_job_hook_keyword);
		m_startd_job_hook_keyword = NULL;
	}

	int i;
	for (i=0; i <= m_slot_hook_keywords.getlast(); i++) {
		if (m_slot_hook_keywords[i] && m_slot_hook_keywords[i] != UNDEFINED) {
			free(m_slot_hook_keywords[i]);
		}
		m_slot_hook_keywords[i] = NULL;
	}

	MyString key;
	char** hook_paths;
	m_keyword_hook_paths.startIterations();
	while (m_keyword_hook_paths.iterate(key, hook_paths)) {
		for (i=0; i<NUM_HOOKS; i++) {
			if (hook_paths[i] && hook_paths[i] != UNDEFINED) {
				free(hook_paths[i]);
			}
		}
		delete [] hook_paths;
	}
	m_keyword_hook_paths.clear();
}


bool
FetchWorkMgr::initialize()
{
	reconfig();
	m_reaper_ignore_id = daemonCore->
		Register_Reaper("FetchWorkMgr Ignore Reaper",
						(ReaperHandlercpp) &FetchWorkMgr::reaperIgnore,
						"FetchWorkMgr Ignore Reaper", this);
	return HookClientMgr::initialize();
}


bool
FetchWorkMgr::reconfig()
{
		// Clear out our old copies of each hook's path.
	clearHookPaths();

		// Grab the global setting if the per-slot keywords aren't defined.
	m_startd_job_hook_keyword = param("STARTD_JOB_HOOK_KEYWORD");

	return true;
}


char*
FetchWorkMgr::validateHookPath( const char* hook_param )
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

	dprintf(D_FULLDEBUG, "Hook %s: %s\n", hook_param, tmp ? tmp : "UNDEFINED");

		// If we got this far, we've either got a valid hook or it
		// wasn't defined. Either way, we can just return that directly.
	return tmp;
}


char*
FetchWorkMgr::getHookPath(HookType hook_type, Resource* rip)
{
	char* keyword = getHookKeyword(rip);

	if (!keyword) {
			// Nothing defined, bail now.
		return NULL;
	}

	int i;
	MyString key(keyword);
	char** hook_paths;
	if (m_keyword_hook_paths.lookup(key, hook_paths) < 0) {
			// No entry, initialize it.
		hook_paths = new char*[NUM_HOOKS];
		for (i=0; i<NUM_HOOKS; i++) {
			hook_paths[i] = NULL;
		}
		m_keyword_hook_paths.insert(key, hook_paths);
	}

	char* path = hook_paths[(int)hook_type];
	if (!path) {
		MyString _param;
		_param.sprintf("%s_HOOK_%s", keyword, getHookTypeString(hook_type));
		path = validateHookPath(_param.Value());
		if (!path) {
			hook_paths[(int)hook_type] = UNDEFINED;
		}
		else {
			hook_paths[(int)hook_type] = path;
		}
	}
	else if (path == UNDEFINED) {
		path = NULL;
	}
	return path;
}


char*
FetchWorkMgr::getHookKeyword(Resource* rip)
{
	int slot_id = rip->r_id;
	char* keyword = m_slot_hook_keywords[slot_id];
	if (!keyword) {
		MyString param_name;
		param_name.sprintf("%s_JOB_HOOK_KEYWORD", rip->r_id_str);
		keyword = param(param_name.Value());
		m_slot_hook_keywords[slot_id] = keyword ? keyword : UNDEFINED;
	}
	else if (keyword == UNDEFINED) {
		keyword = NULL;
	}
	return keyword ? keyword : m_startd_job_hook_keyword;
}


FetchClient*
FetchWorkMgr::buildFetchClient(Resource* rip)
{
	char* hook_path = getHookPath(HOOK_FETCH_WORK, rip);
	if (!hook_path) {
			// No fetch hook defined for this slot, abort.
		return NULL;
	}
	FetchClient* new_client = new FetchClient(rip, hook_path);
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
FetchWorkMgr::tryHookFetchWork(Resource* rip)
{
	if (!rip->willingToFetch()) {
		return false;
	}
	FetchClient* fetch_client = buildFetchClient(rip);
	if (!fetch_client) {
		return false;
	}
	return fetch_client->startFetch();
}


bool
FetchWorkMgr::handleHookFetchWork(FetchClient* fetch_client)
{
	ClassAd* job_ad = NULL;
	Resource* rip = fetch_client->m_rip;
	float rank = 0;
	bool willing = true;
		// Are we currently in Claimed/Idle with a fetched claim?
	bool idle_fetch_claim = (rip->r_cur->type() == CLAIM_FETCH
							 && rip->state() == claimed_state
							 && rip->activity() == idle_act);

	if (!(job_ad = fetch_client->reply())) {
			// No work or error reading the reply, bail out.
		removeFetchClient(fetch_client);
			// Try other hooks?
		if (idle_fetch_claim) {
				// we're currently Claimed/Idle with a fetched
				// claim. If the fetch hook just returned no data, it
				// means we're out of work, we should evict this
				// claim, and return to the Owner state.
			rip->terminateFetchedWork();
		}
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
		if (rip->state() == claimed_state && !idle_fetch_claim) {
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

		// Either way, if the reply claim hook is configured, invoke it.
	hookReplyClaim(willing, job_ad, rip);

	if (!willing) {
		removeFetchClient(fetch_client);
			// TODO-fetch: matchmaking on other slots?
		if (idle_fetch_claim) {
				// The slot is Claimed/Idle with a fetch claim. If we
				// just fetched work and aren't willing to run it, we
				// need to evict this claim and return to Owner.
			rip->terminateFetchedWork();
		}
		return false;
	}

		// We're ready to start running the job, so we need to update
		// the current Claim and Client objects to remember this work.
	rip->createOrUpdateFetchClaim(job_ad, rank);

		// Once we've done that, the Claim object in the Resource has
		// control over the job classad, so we want to NULL-out our
		// copy here to avoid a double-free.
	fetch_client->clearReplyAd();

		// Now, depending on our current state, initiate a state change.
	if (rip->state() == claimed_state) {
		if (idle_fetch_claim) {
				// We've got an idle fetch claim and we just got more
				// work, so we should spawn it.
			rip->spawnFetchedWork();
			return true;
		}
			// We're already claimed, but not via an idle fetch claim,
			// so we need to preempt the current job first.
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

		// And now that we've generated a Claim, saved the ClassAd,
		// and initiated our state change, we're done with this client.
	removeFetchClient(fetch_client);

	return true;
}

void
FetchWorkMgr::hookReplyClaim(bool claimed, ClassAd* job_ad, Resource* rip)
{
	char* hook_path = getHookPath(HOOK_REPLY_CLAIM, rip);
	if (!hook_path) {
		return;
	}

	ArgList args;
	args.AppendArg(condor_basename(hook_path));
	args.AppendArg((claimed ? "accept" : "reject"));
	int std_fds[3] = {DC_STD_FD_PIPE, -1, -1};
	int hook_pid = daemonCore->
		Create_Process(hook_path, args, PRIV_CONDOR, m_reaper_ignore_id,
					   FALSE, NULL, NULL, NULL, NULL, std_fds);
	if (hook_pid == FALSE) {		
		dprintf(D_ALWAYS, "ERROR: Create_Process() failed in "
				"FetchWorkMgr::hookReplyClaim()\n");
		return;
	}
	MyString hook_stdin;
	job_ad->sPrint(hook_stdin);
	hook_stdin += "-----\n";  // TODO-fetch: better delimiter?
	rip->r_classad->sPrint(hook_stdin);
	daemonCore->Write_Stdin_Pipe(hook_pid, hook_stdin.Value(),
								 hook_stdin.Length());
	daemonCore->Close_Stdin_Pipe(hook_pid);
		// That's it, we don't care about the output at all...
}


void
FetchWorkMgr::hookEvictClaim(Resource* rip)
{
	char* hook_path = getHookPath(HOOK_EVICT_CLAIM, rip);
	if (!hook_path) {
		return;
	}

	ArgList args;
	args.AppendArg(condor_basename(hook_path));
	int std_fds[3] = {DC_STD_FD_PIPE, -1, -1};
	int hook_pid = daemonCore->
		Create_Process(hook_path, args, PRIV_CONDOR, m_reaper_ignore_id,
					   FALSE, NULL, NULL, NULL, NULL, std_fds);
	if (hook_pid == FALSE) {		
		dprintf(D_ALWAYS, "ERROR: Create_Process() failed in "
				"FetchWorkMgr::hookEvictClaim()\n");
		return;
	}
	MyString hook_stdin;
	rip->r_cur->ad()->sPrint(hook_stdin);
	hook_stdin += "-----\n";  // TODO-fetch: better delimiter?
	rip->r_classad->sPrint(hook_stdin);
	daemonCore->Write_Stdin_Pipe(hook_pid, hook_stdin.Value(),
								 hook_stdin.Length());
	daemonCore->Close_Stdin_Pipe(hook_pid);
		// That's it, we don't care about the output at all...
}


int
FetchWorkMgr::reaperIgnore(int exit_pid, int exit_status)
{
		// Some hook that we don't care about the output for just
		// exited.  All we need is to print a log message (if that).
	MyString status_txt;
	status_txt.sprintf("Hook (pid %d) ", exit_pid);
	statusString(exit_status, status_txt);
	dprintf(D_FULLDEBUG, "%s\n", status_txt.Value());
	return TRUE;
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
		m_job_ad = NULL;
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
	m_rip->startedFetch();
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
	resmgr->m_fetch_work_mgr->handleHookFetchWork(this);
}


void
FetchClient::clearReplyAd(void) {
	m_job_ad = NULL;
}
