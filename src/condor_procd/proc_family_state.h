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

#ifndef _PROC_FAMILY_STATE_H
#define _PROC_FAMILY_STATE_H

#include "condor_common.h"
#include "HashTable.h"
#include "tree.h"

class LocalClient;

class ProcFamilyState {

public:

	ProcFamilyState(pid_t root, pid_t watcher);

	ProcFamilyState(LocalClient*);

	~ProcFamilyState();

	void process_created(pid_t parent, pid_t child, bool registered);

	void process_exited(pid_t pid);

	void family_unregistered(pid_t pid);

	void display();

private:

	struct Family {
		pid_t             m_root;
		pid_t             m_watcher;
		SimpleList<pid_t> m_process_list;
	};

	void read_family(LocalClient* client, Tree<Family*>* family_node);

	void family_unregistered(Tree<Family*>* family_node);

	void display(Tree<Family*>* family_node, int depth);

	Tree<Family*>* m_family_tree;

	HashTable<pid_t, Tree<Family*>*> m_family_table;
};

#endif
