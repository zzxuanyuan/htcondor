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
#include "starter.h"
#include "StarterHookMgr.h"
#include "condor_attributes.h"
#include "hook_utils.h"

extern CStarter *Starter;


// // // // // // // // // // // // 
// StarterHookMgr
// // // // // // // // // // // // 

StarterHookMgr::StarterHookMgr()
	: HookClientMgr()
{
	m_hook_keyword = NULL;
	m_hook_prepare_job = NULL;
	m_hook_update_job_info = NULL;
	m_hook_final_job_info = NULL;
	m_hook_evict_job = NULL;

	dprintf( D_FULLDEBUG, "Instantiating a StarterHookMgr\n" );
}


StarterHookMgr::~StarterHookMgr()
{
	dprintf( D_FULLDEBUG, "Deleting the StarterHookMgr\n" );

		// Delete our copies of the paths for each hook.
	clearHookPaths();

	if (m_hook_keyword) {
		free(m_hook_keyword);
	}
}


void
StarterHookMgr::clearHookPaths()
{
	if (m_hook_prepare_job) {
		free(m_hook_prepare_job);
	}
	if (m_hook_update_job_info) {
		free(m_hook_update_job_info);
	}
	if (m_hook_final_job_info) {
		free(m_hook_final_job_info);
	}
	if (m_hook_evict_job) {
		free(m_hook_evict_job);
	}
}


bool
StarterHookMgr::initialize(ClassAd* job_ad)
{
	if (!job_ad->LookupString(ATTR_HOOK_KEYWORD, &m_hook_keyword)) {
		dprintf(D_FULLDEBUG,
				"Job does not define %s, not invoking any job hooks.\n",
				ATTR_HOOK_KEYWORD);
		return false;
	}
	reconfig();
	return HookClientMgr::initialize();
}


bool
StarterHookMgr::reconfig()
{
		// Clear out our old copies of each hook's path.
	clearHookPaths();

	m_hook_prepare_job = getHookPath(HOOK_PREPARE_JOB);
	m_hook_update_job_info = getHookPath(HOOK_UPDATE_JOB_INFO);
	m_hook_final_job_info = getHookPath(HOOK_FINAL_JOB_INFO);
	m_hook_evict_job = getHookPath(HOOK_EVICT_JOB);

	return true;
}


char*
StarterHookMgr::getHookPath(HookType hook_type)
{
	if (!m_hook_keyword) {
		return NULL;
	}
	MyString _param;
	_param.sprintf("%s_HOOK_%s", m_hook_keyword, getHookTypeString(hook_type));
	return validateHookPath(_param.Value());
}


int
StarterHookMgr::tryHookPrepareJob()
{
	if (!m_hook_prepare_job) {
		dprintf(D_FULLDEBUG, "HOOK_PREPARE_JOB not configured.\n");
		return 0;
	}

	MyString hook_stdin;
	ClassAd* job_ad = Starter->jic->jobClassAd();
	job_ad->sPrint(hook_stdin);

	HookClient* hook_client = new HookPrepareJobClient(m_hook_prepare_job);

	if (!spawn(hook_client, NULL, &hook_stdin)) {
		dprintf(D_ALWAYS|D_FAILURE,
				"ERROR in StarterHookMgr::tryHookPrepareJob: "
				"failed to spawn HOOK_PREPARE_JOB (%s)\n", m_hook_prepare_job);
		return -1;
	}

	dprintf(D_FULLDEBUG, "HOOK_PREPARE_JOB (%s) invoked.\n",
			m_hook_prepare_job);
	return 1;
}


// // // // // // // // // // // //
// HookPrepareJobClient class
// // // // // // // // // // // //

HookPrepareJobClient::HookPrepareJobClient(const char* hook_path)
	: HookClient(hook_path, true)
{
		// Nothing special needed in the child class.
}


void
HookPrepareJobClient::hookExited(int exit_status) {
	HookClient::hookExited(exit_status);
		// TODO: check exit status, bail on failure, etc.
	Starter->jobEnvironmentReady();
}


// // // // // // // // // // // //
// HookFinalJobInfoClient class
// // // // // // // // // // // //

HookFinalJobInfoClient::HookFinalJobInfoClient(const char* hook_path)
	: HookClient(hook_path, true)
{
		// Nothing special needed in the child class.
}


void
HookFinalJobInfoClient::hookExited(int exit_status) {
	HookClient::hookExited(exit_status);
		// TODO
}
 

// // // // // // // // // // // //
// HookEvictJobClient class
// // // // // // // // // // // //

HookEvictJobClient::HookEvictJobClient(const char* hook_path)
	: HookClient(hook_path, true)
{
		// Nothing special needed in the child class.
}


void
HookEvictJobClient::hookExited(int exit_status) {
	HookClient::hookExited(exit_status);
		// TODO
}


