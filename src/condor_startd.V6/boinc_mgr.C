/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_debug.h"
#include "status_string.h"

#include "startd.h"
#include "boinc_mgr.h"


BOINC_BackfillVM::BOINC_BackfillVM( int vm_id )
	    : BackfillVM( vm_id )
{
		// TODO
}


BOINC_BackfillVM::~BOINC_BackfillVM()
{
		// TODO
}


bool
BOINC_BackfillVM::init()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::start()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::suspend()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::resume()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::softkill()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::hardkill()
{
		// TODO
	return true;
}


void
BOINC_BackfillVM::publish( ClassAd* ad )
{
		// TODO
}


BOINC_BackfillMgr::BOINC_BackfillMgr()
	: BackfillMgr()
{
	dprintf( D_ALWAYS, "Instantiating a BOINC_BackfillMgr\n" );
	m_boinc_dir = NULL;
	m_boinc_client_exe = NULL;
	m_boinc_pid = 0;
	m_reaper_id = -1;
}


BOINC_BackfillMgr::~BOINC_BackfillMgr()
{
	dprintf( D_FULLDEBUG, "Destroying a BOINC_BackfillMgr\n" );
	if( m_boinc_dir ) {
		free( m_boinc_dir );
	}
	if( m_boinc_client_exe ) {
		free( m_boinc_client_exe );
	}
	if( m_boinc_pid ) {
			// our child is still around, hardkill "all" VMs
		hardkill( 0 );
	}
}


static bool
param_boinc( const char* attr_name, char** str_p )
{
	if( ! str_p ) {
		EXCEPT( "param_boinc() called with NULL string" );
	}
	if( ! attr_name ) {
		EXCEPT( "param_boinc() called with NULL attr_name" );
	}

	if( *str_p != NULL ) {
		free( *str_p );
	}
	*str_p = param( attr_name );
	if( ! *str_p ) {
		dprintf( D_ALWAYS, "Trying to initialize a BOINC backfill manager, "
				 "but %s not defined, failing\n", attr_name );
		return false;
	}
	return true;
}


bool
BOINC_BackfillMgr::init()
{
	return reconfig();
}


bool
BOINC_BackfillMgr::reconfig()
{
		// TODO be smart about if anything changes...

	if( ! param_boinc("BOINC_HOMEDIR", &m_boinc_dir) ) {
		return false;
	}
	if( ! param_boinc("BOINC_CLIENT_EXE", &m_boinc_client_exe) ) {
		return false;
	}
	
	dprintf( D_FULLDEBUG, "BOINC homedir: '%s'\n", m_boinc_dir );
	dprintf( D_FULLDEBUG, "BOINC client: '%s'\n", m_boinc_client_exe );
	return true;
}


bool
BOINC_BackfillMgr::addVM( BOINC_BackfillVM* boinc_vm )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::rmVM( int vm_id )
{
	if( ! m_vms[vm_id] ) {
		return false;
	}
	delete m_vms[vm_id];
	m_vms[vm_id] = NULL;
	m_num_vms--;

		// let the corresponding Resource know we're no longer running
		// a backfill client for it
	Resource* rip = resmgr->get_by_vm_id( vm_id );
	if( ! rip ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::rmVM(): "
				 "can't find resource with VM id %d\n", vm_id );
		return false;
	}
	rip->backfillGone();

	return true;
}


bool
BOINC_BackfillMgr::start( int vm_id )
{
	if( m_vms[vm_id] ) {
		dprintf( D_ALWAYS, "BackfillVM object for VM %d already exists\n",
				 vm_id );
		return true;
	}

	Resource* rip = resmgr->get_by_vm_id( vm_id );
	if( ! rip ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "can't find resource with VM id %d\n", vm_id );
		return false;
	}
	State s = rip->state();
	Activity a = rip->activity();

	if( s != backfill_state ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "Resource for VM id %d not in Backfill state (%s/%s)\n",
				 vm_id, state_to_string(s), activity_to_string(a) );
		return false;
	}
	if( a != idle_act ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::start(): "
				 "Resource for VM id %d not in Backfill/Idle (%s/%s)\n",
				 vm_id, state_to_string(s), activity_to_string(a) );
		return false;
	}

	if( m_boinc_pid ) {
			// already have a BOINC client running, allocate a new
			// BackfillVM for this vm_id, and consider this done.
		dprintf( D_FULLDEBUG, "VM %d wants to do backfill, already have "	
				 "a BOINC client running (pid %d)\n", vm_id, m_boinc_pid );
	} else { 
		// no BOINC client running, we need to spawn it
		if( ! spawnClient() ) {
			dprintf( D_ALWAYS,
					 "ERROR spawning BOINC client, can't start backfill!\n" );
			return false;
		}
	}

		// PHASE 2: split up slots, remove monolithic BOINC client
	m_vms[vm_id] = new BOINC_BackfillVM( vm_id );
	m_num_vms++;

		// now that we have a BOINC client and a BOINC_BackfillVM
		// object for this VM, change to Backfill/BOINC
	dprintf( D_ALWAYS, "State change: BOINC client running for vm%d\n",
			 vm_id ); 
	return rip->change_state( boinc_act );
}


bool
BOINC_BackfillMgr::spawnClient( void )
{ 
	dprintf( D_FULLDEBUG, "Entering BOINC_BackfillMgr::spawnClient()\n" );

	if( m_reaper_id < 0 ) {
		m_reaper_id = daemonCore->Register_Reaper( "BOINC reaper",
			(ReaperHandlercpp)&BOINC_BackfillMgr::reaper,
			"BOINC_BackfillMgr::reaper()", this );
		ASSERT( m_reaper_id != FALSE );
	}

	if( m_boinc_pid ) {
			// shouldn't happen, but bail out, just in case
		dprintf( D_ALWAYS, "ERROR: BOINC_BackfillMgr::spawnClient() "
				 "called with m_boinc_pid=%d\n", m_boinc_pid );
		return false;
	}

	int boinc_nice;
	boinc_nice = param_integer( "BACKFILL_RENICE_INCREMENT", -1, 0, 19 );
	if( boinc_nice == -1 ) {
		boinc_nice = param_integer( "BOINC_RENICE_INCREMENT", 10, 0, 19 );
	}

		// now, we can actually spawn the BOINC client
		// TODO we need better priv state handling for this!
	m_boinc_pid = daemonCore->
		Create_Process( m_boinc_client_exe, NULL, PRIV_CONDOR, m_reaper_id,
						FALSE, NULL, m_boinc_dir, TRUE, NULL, NULL, 
						boinc_nice, 0, NULL );
	if( ! m_boinc_pid ) {
		dprintf( D_ALWAYS, "ERROR spawning BOINC client\n" );
		return false;
	}
	dprintf( D_FULLDEBUG, "Spawned BOINC client: (pid %d)\n", m_boinc_pid );
	return true;
}


int
BOINC_BackfillMgr::reaper( int pid, int status )
{
	MyString status_str;
	statusString( status, status_str );
	dprintf( D_ALWAYS, "BOINC client (pid %d) %s\n", pid, status_str.Value() );
	if( m_boinc_pid != pid ) {
		EXCEPT( "Impossible: BOINC_BackfillMgr::reaper() pid [%d] "
				"doesn't match m_boinc_pid [%d]", pid, m_boinc_pid );
	}
	
		// clear our pid so we know it's gone...
	m_boinc_pid = 0;

		// once the client is gone, delete all our compute slots
	int i, max = m_vms.getsize();
	for( i=0; i < max; i++ ) {
		rmVM( i );
	}

	return TRUE;
}


bool
BOINC_BackfillMgr::suspend( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::resume( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::softkill( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::hardkill( int vm_id )
{
		// PHASE 2: handle different vm_ids differently...
	if( vm_id ) {
		if( m_vms[vm_id] ) {
			delete m_vms[vm_id];
			m_vms[vm_id] = NULL;
			m_num_vms--;
		} else {
			dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::hardkill(%d) "
					 "no BackfillVM object with that id\n", vm_id );
			return false;
		}
	} else {
			// kill all
		int i;
		for( i=0; i <= m_vms.getlast(); i++ ) { 
			if( m_vms[i] ) {
				delete m_vms[i];
				m_vms[i] = NULL;
				m_num_vms--;
			}
		}
	}
			
	if( m_num_vms > 0 ) {
			// we still have some VMs left, we have to return
		return true;
	}

	if( m_boinc_pid <= 0 ) {
			// no BOINC client running, we're done
		return true;
	}

		// if we're here, we're done and we should really kill it
#ifdef WIN32
	EXCEPT( "Condor BOINC support does NOT work on windows" ); 
#else 
	priv_state old_state = set_root_priv();
	int rval = ::kill( m_boinc_pid, SIGKILL );
	set_priv( old_state );

	if( rval < 0 ) {
		dprintf( D_ALWAYS, "ERROR in BOINC_BackfillMgr::hardkill(): "
				 "kill returned %d - %s (errno: %d)\n", rval,
				 strerror(errno), errno );
		return false;
	}

	dprintf( D_FULLDEBUG, "BOINC_BackfillMgr::hardkill(): "
			 "sent SIGKILL to BOINC client (pid %d)\n", m_boinc_pid );

#endif /* WIN32 */

	return true;
}


bool
BOINC_BackfillMgr::walk( BoincVmMember member_func )
{
	bool rval = true;
	int i, num = 0, max = m_vms.getsize();
	for( i = 0; num < m_num_vms && i < max; i++ ) {
		if( m_vms[i] ) { 
			num++;
			if( ! (((BOINC_BackfillVM*)m_vms[i])->*(member_func))() ) {
				rval = false;
			}
		}
	}
	return rval;
}
