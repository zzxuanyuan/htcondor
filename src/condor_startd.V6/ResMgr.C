/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "startd.h"
static char *_FileName_ = __FILE__;


ResMgr::ResMgr()
{
	coll_sock = NULL;
	view_sock = NULL;
	this->init_socks();

	m_attr = new MachAttributes;
}


void
ResMgr::init_resources()
{
	int i;
	CpuAttributes* cap;
	float share;

	nresources = m_attr->num_cpus();
	share = (float)1 / nresources;

	resources = new Resource*[nresources];

	for( i = 0; i < nresources; i++ ) {
		cap = new CpuAttributes( m_attr, share, share, share );
		resources[i] = new Resource( cap, i+1 );
	}
}


void
ResMgr::init_socks()
{
	if( coll_sock ) {
		delete coll_sock;
	}
	coll_sock = new SafeSock( collector_host, 
							  COLLECTOR_UDP_COMM_PORT );

	if( view_sock ) {
		delete view_sock;
	}
	if( condor_view_host ) {
		view_sock = new SafeSock( condor_view_host, 
								  CONDOR_VIEW_PORT );
	}
}


ResMgr::~ResMgr()
{
	delete m_attr;

	delete coll_sock;
	if( view_sock ) {
		delete view_sock;
	}
	delete [] resources;
}


void
ResMgr::walk( int(*func)(Resource*) )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		func(resources[i]);
	}
}


void
ResMgr::walk( ResourceMember memberfunc )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))();
	}
}


void
ResMgr::walk( VoidResourceMember memberfunc )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))();
	}
}


void
ResMgr::walk( ResourceMaskMember memberfunc, amask_t mask ) 
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))(mask);
	}
}


int
ResMgr::sum( ResourceMember memberfunc )
{
	int i, tot = 0;
	for( i = 0; i < nresources; i++ ) {
		tot += (resources[i]->*(memberfunc))();
	}
	return tot;
}


float
ResMgr::sum( ResourceFloatMember memberfunc )
{
	int i;
	float tot = 0;
	for( i = 0; i < nresources; i++ ) {
		tot += (resources[i]->*(memberfunc))();
	}
	return tot;
}


Resource*
ResMgr::max( ResourceMember memberfunc, int* val )
{
	Resource* rip = NULL;
	int i, tmp, max = INT_MIN;

	for( i = 0; i < nresources; i++ ) {
		tmp = (resources[i]->*(memberfunc))();
		if( tmp > max ) {
			max = tmp;
			rip = resources[i];
		}
	}
	if( val ) {
		*val = max;
	}
	return rip;
}


bool
ResMgr::in_use( void )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->in_use() ) {
			return true;
		}
	}
	return false;
}


Resource*
ResMgr::get_by_pid( int pid )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_starter->pid() == pid ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_cur_cap( char* cap )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->cap()->matches(cap) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_any_cap( char* cap )
{
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->cap()->matches(cap) ) {
			return resources[i];
		}
		if( (resources[i]->r_pre) &&
			(resources[i]->r_pre->cap()->matches(cap)) ) {
			return resources[i];
		}
	}
	return NULL;
}


State
ResMgr::state()
{
		// This needs serious help when we get to multiple resources
	return resources[0]->state();
}


void
ResMgr::final_update()
{
	walk( Resource::final_update );
}


int
ResMgr::force_benchmark()
{
	return resources[0]->force_benchmark();
}


int
ResMgr::send_update( ClassAd* public_ad, ClassAd* private_ad )
{
	int num = 0;
	if( coll_sock &&
		(send_classad_to_sock(coll_sock, public_ad, private_ad)) ) {
		num++;
	}  

		// If we have an alternate collector, send public CA there.
	if( view_sock && 
		(send_classad_to_sock(view_sock, public_ad, NULL)) ) {
		num++;
	}

		// Increment the resmgr's count of updates.
	num_updates++;
	return num;
}


void
ResMgr::eval_and_update_all()
{
	num_updates = 0;
	compute( A_TIMEOUT | A_UPDATE );
	walk( Resource::eval_and_update );
	report_updates();
}


void
ResMgr::eval_all()
{
	num_updates = 0;
	compute( A_TIMEOUT );
	walk( Resource::eval_state );
	report_updates();
}


void
ResMgr::report_updates()
{
	if( coll_sock ) {
		dprintf( D_FULLDEBUG,
				 "Sent %d update(s) to the collector (%s)\n", 
				 num_updates, collector_host );
	}  
	if( view_sock ) {
		dprintf( D_FULLDEBUG, 
				 "Sent %d update(s) to the condor_view host (%s)\n",
				 num_updates, condor_view_host );
	}
}


void
ResMgr::compute( amask_t how_much )
{
	m_attr->compute( (how_much & ~(A_SUMMED)) | A_SHARED );
	resmgr->walk( Resource::compute, (how_much & ~(A_SHARED)) );
	m_attr->compute( how_much | A_SUMMED );
	walk( Resource::compute, (how_much | A_SHARED) );
	assign_load();
}


void
ResMgr::assign_load()
{
	int i;
	Resource *rip, *next;
	float total_owner_load = m_attr->load() - m_attr->condor_load();
	if( total_owner_load < 0 ) {
		total_owner_load = 0;
	}
	if( is_smp() ) {
		dprintf( D_LOAD, 
				 "%s %.3f\t%s %.3f\t%s %.3f\n",  
				 "SystemLoad:", m_attr->load(),
				 "TotalCondorLoad:", m_attr->condor_load(),
				 "TotalOwnerLoad:", total_owner_load );
	}

		// Initialize everything to 0.
	for( i = 0; i < nresources; i++ ) {
		resources[i]->set_owner_load( 0 );
	}

		// So long as there's at least two more resources and the
		// total owner load is greater than 1.0, assign an owner load
		// of 1.0 to each CPU.  Once we get below 1.0, we assign all
		// the rest to the next CPU.  So, for non-SMP machines, we
		// never hit this code, and always assign all owner load to
		// cpu1 (since i will be initialized to 0 but we'll never
		// enter the for loop).  
	for( i = 0; i < (nresources - 1) && total_owner_load > 1; i++ ) {
		resources[i]->set_owner_load( 1.0 );
		total_owner_load -= 1.0;
	}
	resources[i]->set_owner_load( total_owner_load );

		// Now that we're done assigning, display all values for
		// people that have D_LOAD turned on.
	walk( Resource::display_load );
}


int
ResMgr::start_update_timer()
{
	up_tid = 
		daemonCore->Register_Timer( update_interval, update_interval,
									(TimerHandlercpp)eval_and_update_all,
									"eval_and_update_all", this );
	if( up_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


int
ResMgr::start_poll_timer()
{
	if( poll_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	poll_tid = 
		daemonCore->Register_Timer( polling_interval,
									polling_interval, 
									(TimerHandlercpp)eval_all,
									"poll_resources", this );
	if( poll_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


void
ResMgr::cancel_poll_timer()
{
	if( poll_tid != -1 ) {
		daemonCore->Cancel_Timer( poll_tid );
		poll_tid = -1;
		dprintf( D_FULLDEBUG, "Canceled polling timer.\n" );
	}
}

