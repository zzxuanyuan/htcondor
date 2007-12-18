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
#include "condor_debug.h"
#include "proc_family_state.h"
#include "../condor_procapi/procapi.h"
#include "local_client.h"

template class HashTable<int, Tree<ProcFamilyState::Family*>*>;

ProcFamilyState::ProcFamilyState(pid_t root, pid_t watcher) : m_family_table(pidHashFunc)
{
	int iret;
	bool bret;

	Family* root_family = new Family;
	ASSERT(root_family != NULL);

	m_family_tree = new Tree<Family*>(root_family);
	ASSERT(m_family_tree != NULL);
	iret = m_family_table.insert(root, m_family_tree);
	ASSERT(iret != -1);

	root_family->m_root = root;
	root_family->m_watcher = watcher;
	bret = root_family->m_process_list.Append(root);
	ASSERT(bret);
}

ProcFamilyState::ProcFamilyState(LocalClient* client) :
	m_family_table(pidHashFunc)
{
	bool bret;
	int iret;

	Family* root_family = new Family;
	ASSERT(root_family != NULL);

	m_family_tree = new Tree<Family*>(root_family);
	ASSERT(m_family_tree != NULL);

	read_family(client, m_family_tree);

	while (true) {

		pid_t parent_pid;
		bret = client->read_data(&parent_pid, sizeof(pid_t));
		ASSERT(bret);

		if (parent_pid == 0) {
			break;
		}

		Tree<Family*>* family_node;
		iret = m_family_table.lookup(parent_pid, family_node);
		ASSERT(iret != -1);

		Family* family = new Family;
		ASSERT(family != NULL);

		family_node = family_node->add_child(family);

		read_family(client, family_node);
	}

	client->end_connection();
}

void
ProcFamilyState::read_family(LocalClient* client, Tree<Family*>* family_node)
{
	bool bret;
	pid_t pid;

	bret = client->read_data(&pid, sizeof(pid_t));
	ASSERT(bret);
	family_node->get_data()->m_root = pid;

	bret = client->read_data(&pid, sizeof(pid_t));
	ASSERT(bret);
	family_node->get_data()->m_watcher = pid;

	while (true) {

		bret = client->read_data(&pid, sizeof(pid_t));
		ASSERT(bret);

		if (pid == 0) {
			break;
		}

		bret = family_node->get_data()->m_process_list.Append(pid);
		ASSERT(bret);
	}
}

ProcFamilyState::~ProcFamilyState()
{

}

void
ProcFamilyState::process_created(pid_t parent_pid,
                                 pid_t child_pid,
                                 bool registered)
{
	int iret;
	bool bret;

	Tree<Family*>* family_node;
	iret = m_family_table.lookup(parent_pid, family_node);
	ASSERT(iret != -1);

	if (registered) {
		Family* family = new Family;
		ASSERT(family != NULL);
		family_node = family_node->add_child(family);
		ASSERT(family_node != NULL);
		family->m_root = child_pid;
		family->m_watcher = parent_pid;
	}

	iret = m_family_table.insert(child_pid, family_node);
	ASSERT(iret != -1);

	bret = family_node->get_data()->m_process_list.Append(child_pid);
	ASSERT(bret);
}

void
ProcFamilyState::process_exited(pid_t pid)
{
	int iret;
	bool bret;

	Tree<Family*>* family_node;
	iret = m_family_table.lookup(pid, family_node);
	ASSERT(iret != -1);

	bret = family_node->get_data()->m_process_list.Delete(pid);
	ASSERT(bret);

	Tree<Family*>* child_node = family_node->get_child();
	while (child_node != NULL) {
		Tree<Family*>* next_node = child_node->get_sibling();
		if (child_node->get_data()->m_watcher == pid) {
			family_unregistered(child_node);
		}
		child_node = next_node;
	}

	iret = m_family_table.remove(pid);
	ASSERT(iret != -1);
}

void
ProcFamilyState::family_unregistered(pid_t pid)
{
	Tree<Family*>* family_node;
	int ret = m_family_table.lookup(pid, family_node);
	ASSERT(ret != -1);

	family_unregistered(family_node);
}

void
ProcFamilyState::family_unregistered(Tree<Family*>* family_node)
{
	family_node->get_data()->m_process_list.Rewind();
	pid_t tmp_pid;
	while (family_node->get_data()->m_process_list.Next(tmp_pid)) {
		family_node->get_parent()->get_data()->m_process_list.Append(tmp_pid);
	}

	family_node->remove();

	delete family_node->get_data();
	delete family_node;
}

void
ProcFamilyState::display()
{
	display(m_family_tree, 0);
}

void
ProcFamilyState::display(Tree<Family*>* family_node, int depth)
{
	MyString str;

	for (int i = 0; i < depth; i++) {
		str += "    ";
	}

	Family* family = family_node->get_data();

	str.sprintf_cat("%u (%u):", family->m_root, family->m_watcher);

	family->m_process_list.Rewind();
	pid_t pid;
	while (family->m_process_list.Next(pid)) {
		str.sprintf_cat(" %u", pid);
	}

	dprintf(D_ALWAYS, "%s\n", str.Value());
}
