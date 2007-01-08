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
#include "pid_tracker.h"
#include "proc_family.h"
#include "procd_common.h"

PIDTracker::~PIDTracker()
{
	ListEntry* entry = m_list;
	while (m_list != NULL) {
		ListEntry* next = entry->next;
		delete entry;
		entry = next;
	}
}

void
PIDTracker::add_entry(ProcFamily* family, pid_t pid, birthday_t birthday)
{
	ListEntry* new_entry = new ListEntry;
	new_entry->family = family;
	new_entry->pid = pid;
	new_entry->birthday = birthday;
	new_entry->next = m_list;
	m_list = new_entry;
}

void PIDTracker::remove_entry(ProcFamily* family)
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
PIDTracker::check_process(procInfo* pi)
{
	ListEntry* entry = m_list;
	while (entry != NULL) {

		if ((pi->pid == entry->pid) && (pi->birthday == entry->birthday)) {
			entry->family->add_member(pi);
			return true;
		}
		
		entry = entry->next;
	}

	return false;
}
