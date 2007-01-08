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
#include "environment_tracker.h"
#include "proc_family.h"

EnvironmentTracker::~EnvironmentTracker()
{
	ListEntry* entry = m_list;
	while (m_list != NULL) {
		ListEntry* next = entry->next;
		delete entry;
		entry = next;
	}
}

void
EnvironmentTracker::add_entry(ProcFamily* family, PidEnvID* pidenvid)
{
	ListEntry* new_entry = new ListEntry;
	new_entry->family = family;
	pidenvid_copy(&new_entry->pidenvid, pidenvid);
	new_entry->next = m_list;
	m_list = new_entry;
}

void EnvironmentTracker::remove_entry(ProcFamily* family)
{
	ListEntry** prev_ptr = &m_list;
	ListEntry* curr = m_list;
	while (curr != NULL) {
		if (curr->family == family) {
			*prev_ptr = curr->next;
			delete curr;
			return;
		}
		prev_ptr = &curr->next;
		curr = curr->next;
	}
}

bool
EnvironmentTracker::check_process(procInfo* pi)
{
	ListEntry* entry = m_list;
	while (entry != NULL) {

		if (pidenvid_match(&entry->pidenvid, &pi->penvid) == PIDENVID_MATCH) {
			entry->family->add_member(pi);
			return true;
		}
		
		entry = entry->next;
	}

	return false;
}
