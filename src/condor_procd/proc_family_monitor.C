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
#include "proc_family_monitor.h"

ProcFamilyMonitor::ProcFamilyMonitor(pid_t pid,
                                     birthday_t birthday,
                                     int snapshot_interval) :
	m_family_table(11, pidHashFunc, rejectDuplicateKeys),
	m_member_table(PHBUCKETS, pidHashFunc, rejectDuplicateKeys)
{
	// the snapshot interval must either be non-negative or -1, which
	// means infinite (higher layers should enforce this)
	//
	ASSERT(snapshot_interval >= -1);

	// create the "root" family; set the watcher to 0, which means this
	// family is unwatched
	//
	ProcFamily* family = new ProcFamily(this,
	                                    pid,
	                                    birthday,
	                                    0,
	                                    snapshot_interval);

	// make this family the root-level tree node
	//
	m_tree = new Tree<ProcFamily*>(family);

	// and insert the tree into our hash table for families
	//
	int ret = m_family_table.insert(pid, m_tree);
	ASSERT(ret != -1);

	// take an initial snapshot
	//
	snapshot();
}


ProcFamilyMonitor::~ProcFamilyMonitor()
{
	delete_all_families(m_tree);
	delete m_tree;
}

bool
ProcFamilyMonitor::register_subfamily(pid_t root_pid,
                                      pid_t watcher_pid,
                                      int max_snapshot_interval,
                                      PidEnvID* penvid,
                                      char* login)
{
	// root pid must be positive
	//
	if (root_pid <= 0) {
		dprintf(D_ALWAYS,
		        "register_subfamily failure: bad root pid: %u\n",
		        root_pid);
		return false;
	}

	// watcher pid must be non-negative; zero indicates the subfamily
	// is unwatched (which just means there will be no automatic
	// garbage collection for this family)
	//
	if (watcher_pid < 0) {
		dprintf(D_ALWAYS,
		        "register_subfamily failure; bad watcher pid: %u\n",
		        watcher_pid);
		return false;
	}

	// max_snapshot interval must either be non-negative, or -1
	// for infinite
	//
	if (max_snapshot_interval < -1) {
		dprintf(D_ALWAYS,
		        "register_subfamily failure: bad max snapshot interval: %d\n",
		        max_snapshot_interval);
		return false;
	}

	// get our family tree state as up to date as possible
	//
	snapshot();

	// find the root process of the (potential) new subfamily
	// in our snapshot. we require that the process of any newly
	// created subfamily is in a family we are already tracking
	//
	ProcFamily::Member* member;
	int ret;
	ret = m_member_table.lookup(root_pid, member);
	if (ret == -1) {
		dprintf(D_ALWAYS,
		        "register_subfamily failure: pid %u not in process tree\n",
		        root_pid);
		return false;
	}

	// finally create the new family
	//
	ProcFamily* family = new ProcFamily(this,
	                                    root_pid,
	                                    member->get_proc_info()->birthday,
	                                    watcher_pid,
	                                    max_snapshot_interval,
	                                    penvid,
	                                    login);

	// find the family that will be this new subfamily's parent and create
	// the parent-child link
	//
	pid_t parent_root = member->get_proc_family()->get_root_pid();
	Tree<ProcFamily*>* parent_tree_node;
	ret = m_family_table.lookup(parent_root, parent_tree_node);
	ASSERT(ret != -1);
	Tree<ProcFamily*>* child_tree_node = parent_tree_node->add_child(family);
	ASSERT(child_tree_node != NULL);

	// move the new family's root process into the correct family
	//
	member->move_to_subfamily(family);

	// and insert the tree into our hash table for families
	//
	ret = m_family_table.insert(root_pid, child_tree_node);
	ASSERT(ret != -1);

	dprintf(D_ALWAYS,
	        "new subfamily registered: root = %u, watcher = %u\n",
	        root_pid,
	        watcher_pid);

	return true;
}

bool
ProcFamilyMonitor::unregister_subfamily(pid_t pid)
{
	// lookup the family
	//
	Tree<ProcFamily*>* tree;
	int ret = m_family_table.lookup(pid, tree);
	if (ret == -1) {
		dprintf(D_ALWAYS,
		        "unregister_family failure: family with root %u not found\n",
		        pid);
		return false;
	}

	return unregister_subfamily(tree);
}

int
ProcFamilyMonitor::get_snapshot_interval()
{
	return get_snapshot_interval(m_tree);
}

bool
ProcFamilyMonitor::signal_process(pid_t pid, int sig)
{
	// make sure signals are only sent to subtree roots
	//
	Tree<ProcFamily*>* tree;
	int ret = m_family_table.lookup(pid, tree);
	if (ret == -1) {
		dprintf(D_ALWAYS,
		        "signal_process failure: family with root %u not found\n",
				pid);
		return false;
	}

	// look up the Member so we can get at the procInfo struct
	//
	ProcFamily::Member* pm;
	ret = m_member_table.lookup(pid, pm);
	if (ret == -1) {
		dprintf(D_ALWAYS,
		        "signal_process failure: family root pid %u not found\n",
		        pid);
		return false;
	}
	procInfo* pi = pm->get_proc_info();
	ASSERT(pi);

	dprintf(D_ALWAYS, "sending signal %d to process %u\n", sig, pid);
	send_signal(pi, sig);

	return true;
}

bool
ProcFamilyMonitor::signal_family(pid_t pid, int sig)
{
	// find the family
	//
	Tree<ProcFamily*>* tree;
	int ret = m_family_table.lookup(pid, tree);
	if (ret == -1) {
		dprintf(D_ALWAYS,
		        "signal_family error: family with root %u not found\n",
		        pid);
		return false;
	}

	// get as up to date as possible
	//
	snapshot();
	
	// now send the signal and return
	//
	dprintf(D_ALWAYS, "sending signal %d to family with root %u\n", sig, pid);
	signal_family(tree, sig);

	return true;
}

bool
ProcFamilyMonitor::get_family_usage(pid_t pid, ProcFamilyUsage* usage)
{
	// find the family
	//
	Tree<ProcFamily*>* tree;
	int ret = m_family_table.lookup(pid, tree);
	if (ret == -1) {
		dprintf(D_ALWAYS,
		        "get_family_usage failure: family with root %u not found\n",
		        pid);
		return false;
	}

	// get as up to date as possible
	//
	snapshot();

	// get usage from the requested family and all subfamilies
	//
	ASSERT(usage != NULL);
	usage->user_cpu_time = 0;
	usage->sys_cpu_time = 0;
	usage->percent_cpu = 0.0;
	usage->max_image_size = 0;
	usage->total_image_size = 0;
	usage->num_procs = 0;
	get_family_usage(tree, usage);

	return true;
}

void
ProcFamilyMonitor::snapshot()
{
	dprintf(D_ALWAYS, "taking a snapshot\n");

	// get a snapshot of all processes on the system
	// TODO: should we do something here if ProcAPI returns a NULL result?
	// (the algorithm below will handle it just fine, but its probably an
	// indication that something is wrong)
	//
	procInfo* pi_list = ProcAPI::getProcInfoList();

	// scan through the latest snapshot looking for processes
	// that we've determined to be in families we are monitoring
	// in previous calls to snapshot(); for each such process we
	// find:
	//   - call its ProcFamily::Member's still_alive method, which
	//     will mark it as such and update its procInfo
	//   - remove its procInfo struct from pi_list
	//
	procInfo** prev_ptr = &pi_list;
	procInfo* curr = pi_list;
	while (curr != NULL) {

		ProcFamily::Member* pm;
		int ret = m_member_table.lookup(curr->pid, pm);
		if (ret != -1 &&
		    pm->get_proc_info()->birthday == curr->birthday)
		{
			// we've seen this process before; update it with
			// its newer procInfo struct (this call will result
			// in the old procInfo struct being freed)
			//
			*prev_ptr = curr->next;
			pm->still_alive(curr);
			dprintf(D_ALWAYS, "marking %u still alive\n", curr->pid);
		}
		else {
			// this process is not in any of our families, so it stays
			// on the list
			//
			prev_ptr = &curr->next;
		}

		curr = curr->next;
	}

	// now tell all our ProcFamily objects to get rid of the family members
	// that are no longer on the system (i.e. those that did not get the
	// still_alive method of ProcFamily::Member called in the loop above)
	//
	remove_exited_processes(m_tree);

	// we've now handled all processes that we've determined to be in monitored
	// families in previous calls to snapshot(). now we have to handle the
	// rest by determining whether they belong in any of the families we're
	// monitoring. there are (currently) three ways of determining if a process
	// belongs in a family
	//
	//   1) using our "pidenvid" environment-based matching
	//   2) using user id-based matching
	//   3) using the "parent pid"
	//
	// we'll do this in two passes; the first will use methods 1 and 2, and the
	// second will use method 3. the reason we do it this way is because adding
	// any processes using methods 1 or 2 may allow new processes to be added
	// using method 3; so we might as well make sure we've done all we can with
	// methods 1 and 2 before trying 3. make sense?

	// for methods 1 and 2, we delegate the work to a recursive helper function
	//
	find_family_processes(m_tree, pi_list);

	// for method 3, we iterate adding processes based on ppid until we can no
	// longer add any
	//
	int num_additions = 1;
	while( num_additions != 0 ) {
		num_additions = 0;
		prev_ptr = &pi_list;
		curr = pi_list;
		while( curr != NULL ) {
			ProcFamily::Member* pm;
			int ret = m_member_table.lookup(curr->ppid, pm);
			if ((ret != -1) &&
			    (pm->get_proc_info()->birthday <= curr->birthday))
			{
				// whew! found a parent; add it to the correct
				// family and remove it from our procInfo list
				//
				dprintf(D_ALWAYS,
				        "adding %d to %d based on ppid\n",
				        curr->pid,
				        pm->get_proc_family()->get_root_pid());
			
				num_additions++;
				pm->get_proc_family()->add_member(curr);
				*prev_ptr = curr->next;
			}
			else {
				// parent not in any of our families (yet); keep this
				// on in pi_list
				//
				prev_ptr = &curr->next;
			}

			curr = curr->next;
		}
	}

	// cleanup any families whose "watchers" have exited
	//
	delete_unwatched_families(m_tree);

	// free up the procInfo's that we haven't taken ownership of
	//
	ProcAPI::freeProcInfoList(pi_list);
}

void
ProcFamilyMonitor::add_member_to_table(ProcFamily::Member* member)
{
	int ret = m_member_table.insert(member->get_proc_info()->pid, member);
	ASSERT(ret != -1);
}

void
ProcFamilyMonitor::remove_member_from_table(ProcFamily::Member* member)
{
	int ret = m_member_table.remove(member->get_proc_info()->pid);
	ASSERT(ret != -1);
}

int
ProcFamilyMonitor::get_snapshot_interval(Tree<ProcFamily*>* tree)
{
	// start with the value from the current tree node
	//
	int ret_value = tree->get_data()->get_max_snapshot_interval();
	
	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		int child_value = get_snapshot_interval(child);
		if (ret_value == -1) {
			ret_value = child_value;
		}
		else if (child_value < ret_value) {
			ret_value = child_value;
		}
		child = child->get_sibling();
	}

	return ret_value;
}

void
ProcFamilyMonitor::get_family_usage(Tree<ProcFamily*>* tree, ProcFamilyUsage* usage)
{
	// get usage from current tree node
	//
	tree->get_data()->aggregate_usage(usage);

	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		get_family_usage(child, usage);
		child = child->get_sibling();
	}
}

void
ProcFamilyMonitor::signal_family(Tree<ProcFamily*>* tree, int sig)
{
	// signal current tree node
	//
	tree->get_data()->spree(sig);


	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		signal_family(child, sig);
		child = child->get_sibling();
	}
}

void
ProcFamilyMonitor::remove_exited_processes(Tree<ProcFamily*>* tree)
{
	// remove exited processes from current tree node
	//
	tree->get_data()->remove_exited_processes();

	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		remove_exited_processes(child);
		child = child->get_sibling();
	}
}

void
ProcFamilyMonitor::find_family_processes(Tree<ProcFamily*>* tree, procInfo*& pi_list)
{
	// recurse on children
	// (NOTE: it's important that we recurse on children before calling
	//  find_processes() on the current node, since we want processes to be
	//  assigned to the _deepest_ matching family)
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		find_family_processes(child, pi_list);
		child = child->get_sibling();
	}

	// finally, call find_processes on our family
	tree->get_data()->find_processes(pi_list);
}

void
ProcFamilyMonitor::delete_unwatched_families(Tree<ProcFamily*>* tree)
{
	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {

		// save the next child in line, since recursing may result
		// in the current child begin removed from the tree
		//
		Tree<ProcFamily*>* next_child = child->get_sibling();
		delete_unwatched_families(child);
		child = next_child;
	}

	// check to see if the current tree node's watcher has exited
	//
	pid_t watcher_pid = tree->get_data()->get_watcher_pid();
	if (watcher_pid == 0) {
		dprintf(D_ALWAYS,
		        "still around: root = %u, watcher = %u\n",
		        tree->get_data()->get_root_pid(),
				watcher_pid);
		return;
	}
	ProcFamily::Member* member;
	int ret = m_member_table.lookup(watcher_pid, member);
	if ((ret != -1) && (member->get_proc_info()->birthday <=
	                    tree->get_data()->get_root_birthday()))
	{
		// this family's watcher is still around; do nothing
		//
		dprintf(D_ALWAYS,
		        "still around: root = %u, watcher = %u\n",
		        tree->get_data()->get_root_pid(),
				watcher_pid);
		return;
	}

	// it looks like the watcher has exited; unregister the subfamily
	//
	pid_t root_pid = tree->get_data()->get_root_pid();
	bool ok = unregister_subfamily(tree);
	ASSERT(ok);

	dprintf(D_ALWAYS,
	        "watcher %u of family with root %u has died; family removed\n",
	        watcher_pid,
	        root_pid);
}

bool
ProcFamilyMonitor::unregister_subfamily(Tree<ProcFamily*>* tree)
{
	// make sure this family isn't the root family
	//
	Tree<ProcFamily*>* parent = tree->get_parent();
	if (parent == NULL) {
		dprintf(D_ALWAYS,
		        "unregister_subfamily failure: can't unregister root family\n");
		return false;
	}

	// get rid of the hash table entry for this family
	//
	int ret = m_family_table.remove(tree->get_data()->get_root_pid());
	ASSERT(ret != -1);

	// this will move all family members of the current
	// family up into its parent family
	//
	tree->get_data()->give_away_members(parent->get_data());

	// remove the current node from the tree, reparenting all
	// children up to our parent
	//
	tree->remove();

	// clean up the current node now that its relationships
	// with other ProcFamilys and ProcFamily::Members have
	// been removed
	//
	delete tree->get_data();
	delete tree;

	return true;
}

void
ProcFamilyMonitor::delete_all_families(Tree<ProcFamily*>* tree)
{
	// recurse on chlidren
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		delete_all_families(child);
		child = child->get_sibling();
	}

	// free up the family object for the current node
	//
	delete tree->get_data();
}

#if defined(PROCD_DEBUG)

void
ProcFamilyMonitor::output(LocalServer& server, pid_t pid)
{
	// lookup the family. if the look fails, send "false" to the
	// client and return. otherwise, send "true" and keep going
	//
	Tree<ProcFamily*>* tree;
	int ret = m_family_table.lookup(pid, tree);
	bool ok = (ret != -1);
	server.write_data(&ok, sizeof(bool));
	if (!ok) {
		dprintf(D_ALWAYS,
		        "output failure: family with root %u not found\n",
		        pid);
		return;
	}

	// begin recusrsion
	//
	output(server, pid, tree);

	// write a zero when we're done sending family info
	//
	int zero = 0;
	server.write_data(&zero, sizeof(int));
}

void
ProcFamilyMonitor::output(LocalServer& server,
                          pid_t pid,
                          Tree<ProcFamily*>* tree)
{
	// output the current family first. for each subfamily (i.e. all families
	// except the one rooted at the given pid), we'll first send back the
	// family's parent-family's root pid
	//
	if (pid != tree->get_data()->get_root_pid()) {
		Tree<ProcFamily*>* parent = tree->get_parent();
		ASSERT(parent != NULL);
		pid_t parent_root_pid = parent->get_data()->get_root_pid();
		server.write_data(&parent_root_pid, sizeof(pid_t));
	}
	tree->get_data()->output(server);

	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		output(server, pid, child);
		child = child->get_sibling();
	}
}

#endif
