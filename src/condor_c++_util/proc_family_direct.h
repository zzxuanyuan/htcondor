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

#ifndef _PROC_FAMILY_DIRECT_H
#define _PROC_FAMILY_DIRECT_H

#include "proc_family_interface.h"
#include "HashTable.h"

class KillFamily;
struct ProcFamilyDirectContainer;

class ProcFamilyDirect : public ProcFamilyInterface {

public:

	//constructor and destructor
	ProcFamilyDirect();
	~ProcFamilyDirect();

#if !defined(WIN32)
	// on UNIX, the registration logic should be
	// called from the parent for this class
	//
	bool register_from_child() { return false; }
#endif

	bool register_subfamily(pid_t,
	                        pid_t,
	                        int);

	bool track_family_via_environment(pid_t, PidEnvID&);
	bool track_family_via_login(pid_t, const char*);

#if defined(LINUX)
	// this class doesn't support tracking via supplementary
	// group
	//
	bool track_family_via_supplementary_group(pid_t, gid_t&) { return false; }
#endif

	bool get_usage(pid_t, ProcFamilyUsage&, bool);

	bool signal_process(pid_t, int);

	bool suspend_family(pid_t);

	bool continue_family(pid_t);

	bool kill_family(pid_t);

	bool unregister_family(pid_t);

private:

	HashTable<pid_t, ProcFamilyDirectContainer*> m_table;

	KillFamily* lookup(pid_t);
};

#endif
