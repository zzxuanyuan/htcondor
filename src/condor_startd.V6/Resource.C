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

Resource::Resource( CpuAttributes* cap, int rid )
{
	r_classad = NULL;
	r_state = new ResState( this );
	r_starter = new Starter( this );
	r_cur = new Match( this );
	r_pre = NULL;
	r_reqexp = new Reqexp( this );
	r_id = rid;
	
	if( resmgr->is_smp() ) {
		char tmp[256];
		sprintf( tmp, "cpu%d@%s", rid, my_full_hostname() );
		r_name = strdup( tmp );
	} else {
		r_name = strdup( my_full_hostname() );
	}

	r_attr = cap;
	r_attr->attach( this );

	kill_tid = -1;
}


Resource::~Resource()
{
	this->cancel_kill_timer();

	delete r_state;
	delete r_classad;
	delete r_starter;
	delete r_cur;		
	delete r_pre;		
	delete r_reqexp;   
	delete r_attr;		
	free( r_name );
}


int
Resource::release_claim()
{
	switch( state() ) {
	case claimed_state:
		return change_state( preempting_state, vacating_act );
	case matched_state:
		return change_state( owner_state );
	default:
			// For good measure, try directly killing the starter if
			// we're in any other state.  If there's no starter, this
			// will just return without doing anything.  If there is a
			// starter, it shouldn't be there.
		return r_starter->kill( DC_SIGSOFTKILL );
	}
}


int
Resource::kill_claim()
{
	switch( state() ) {
	case claimed_state:
		return change_state( preempting_state, killing_act );
	case matched_state:
		return change_state( owner_state );
	default:
			// In other states, try direct kill.  See above.
		return hardkill_starter();
	}
	return TRUE;
}


int
Resource::got_alive()
{
	if( state() != claimed_state ) {
		return FALSE;
	}
	if( !r_cur ) {
		dprintf( D_ALWAYS, "Got keep alive with no current match object.\n" );
		return FALSE;
	}
	if( !r_cur->client() ) {
		dprintf( D_ALWAYS, "Got keep alive with no current client object.\n" );
		return FALSE;
	}
	r_cur->alive();
	return TRUE;
}


int
Resource::periodic_checkpoint()
{
	char tmp[80];
	if( state() != claimed_state ) {
		return FALSE;
	}
	dprintf( D_ALWAYS, "Performing a periodic checkpoint on %s.\n", r_name );
	if( r_starter->kill( DC_SIGPCKPT ) < 0 ) {
		return FALSE;
	}
	r_cur->setlastpckpt((int)time(NULL));

		// Now that we updated this time, be sure to insert those
		// attributes into the classad right away so we don't keep
		// periodically checkpointing with stale info.
	r_cur->publish( r_classad, A_PUBLIC );

	return TRUE;
}


int
Resource::request_new_proc()
{
	if( state() == claimed_state ) {
		return r_starter->kill( DC_SIGHUP );
	} else {
		return FALSE;
	}
}	


int
Resource::deactivate_claim()
{
	dprintf(D_ALWAYS, "Called deactivate_claim()\n");
	if( state() == claimed_state ) {
		return r_starter->kill( DC_SIGSOFTKILL );
	} else {
		return FALSE;
	}
}


int
Resource::deactivate_claim_forcibly()
{
	dprintf(D_ALWAYS, "Called deactivate_claim_forcibly()\n");
	if( state() == claimed_state ) {
		return hardkill_starter();
	} else {
		return FALSE;
	}
}


int
Resource::hardkill_starter()
{
	if( ! r_starter->active() ) {
		return TRUE;
	}
	if( r_starter->kill( DC_SIGHARDKILL ) < 0 ) {
		r_starter->killpg( DC_SIGKILL );
		return FALSE;
	} else {
		start_kill_timer();
		return TRUE;
	}
}


int
Resource::sigkill_starter()
{
		// Now that the timer has gone off, clear out the tid.
	kill_tid = -1;
	if( r_starter->active() ) {
			// Kill all of the starter's children.
		r_starter->killkids( DC_SIGKILL );
			// Kill the starter's entire process group.
		return r_starter->killpg( DC_SIGKILL );
	}
	return TRUE;
}


int
Resource::change_state( State newstate )
{
	return r_state->change( newstate );
}


int
Resource::change_state( Activity newact )
{
	return r_state->change( newact );
}


int
Resource::change_state( State newstate, Activity newact )
{
	return r_state->change( newstate, newact );
}


bool
Resource::in_use()
{
	State s = state();
	if( s == owner_state || s == unclaimed_state ) {
		return false;
	}
	return true;
}


void
Resource::starter_exited()
{
	dprintf( D_ALWAYS, "Starter pid %d has exited.\n",
			 r_starter->pid() );

		// Let our starter object know it's starter has exited.
	r_starter->exited();

		// Now that this starter has exited, cancel the timer that
		// would send it SIGKILL.
	cancel_kill_timer();

	State s = state();
	switch( s ) {
	case claimed_state:
		change_state( idle_act );
		break;
	case preempting_state:
		leave_preempting_state();
		break;
	default:
		dprintf( D_ALWAYS, 
				 "Warning: starter exited while in unexpected state %s\n",
				 state_to_string(s) );
		change_state( owner_state );
		break;
	}
}


/* 
   This function is called whenever we're in the preempting state
   without a starter.  This situation occurs b/c either the starter
   has finally exited after being told to go away, or we preempted a
   match that wasn't active with a starter in the first place.  In any
   event, leave_preempting_state is the one place that does what needs
   to be done to all the current and preempting matches we've got, and
   decides which state we should enter.
*/
void
Resource::leave_preempting_state()
{
	r_cur->vacate();	// Send a vacate to the client of the match
	delete r_cur;		
	r_cur = NULL;

		// In english:  "If the machine is available and someone
		// is waiting for it..." 
	if( (r_reqexp->eval() != 0) &&
		r_pre && r_pre->agentstream() ) {
		r_cur = r_pre;
		r_pre = NULL;
			// STATE TRANSITION preempting -> claimed
		accept_request_claim( this );
	} else {
			// STATE TRANSITION preempting -> owner
		if( r_pre ) {
			if( r_pre->agentstream() ) {
				r_pre->refuse_agent();
			}
			delete r_pre;
			r_pre = NULL;
		}
		change_state( owner_state );
	}
}


int
Resource::init_classad()
{
	char 	tmp[1024];
	char*	ptr;

	if( r_classad )	delete(r_classad);
	r_classad 		= new ClassAd();

		// Initialize classad types.
	r_classad->SetMyTypeName( STARTD_ADTYPE );
	r_classad->SetTargetTypeName( JOB_ADTYPE );

		// Read in config files and fill up local ad with all attributes 
	config( r_classad );

		// Name of this resource
	sprintf( tmp, "%s = \"%s\"", ATTR_NAME, r_name );
	r_classad->Insert( tmp );

		// Grab the hostname of this machine
	sprintf( tmp, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() );
	r_classad->Insert( tmp );

		// Insert all machine-wide attributes.
	resmgr->m_attr->publish( r_classad, A_ALL );

		// Insert all cpu-specific attributes.
	r_attr->publish( r_classad, A_ALL );

		// Insert state and activity attributes.
	r_state->publish( r_classad, A_ALL );


	return TRUE;
}


void
Resource::update_classad()
{
	this->publish( r_classad, A_UPDATE );
}


void
Resource::timeout_classad()
{
	this->publish( r_classad, A_TIMEOUT );
}


int
Resource::force_benchmark()
{
		// Force this resource to run benchmarking.
	resmgr->m_attr->benchmark( this, 1 );
	return TRUE;
}


int
Resource::update()
{
	int rval;
	ClassAd private_ad;
	ClassAd public_ad;

		// Recompute stats needed for updates and refresh classad. 
	this->update_classad();

	this->make_public_ad( &public_ad );
	this->make_private_ad( &private_ad );

		// Send class ads to collector(s)
	rval = resmgr->send_update( &public_ad, &private_ad );
	if( rval ) {
		dprintf( D_ALWAYS, "Sent update to %d collector(s).\n", rval );
	} else {
		dprintf( D_ALWAYS, "Error sending update to collector(s).\n" );
	}

		// Set a flag to indicate that we've done an update.
	did_update = TRUE;
}


void
Resource::final_update() 
{
	ClassAd public_ad;
	this->make_public_ad( &public_ad );
	r_reqexp->unavail();
	r_state->publish( &public_ad, A_PUBLIC );
	r_reqexp->publish( &public_ad, A_PUBLIC );
	resmgr->send_update( &public_ad, NULL );
}


int
Resource::eval_and_update()
{
	did_update = FALSE;

		// Evaluate the state of this resource.
	eval_state();

		// If we didn't update b/c of the eval_state, we need to
		// actually do the update now.
	if( ! did_update ) {
		update();
	}
	return TRUE;
}


int
Resource::start_kill_timer()
{
	if( kill_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	kill_tid = 
		daemonCore->Register_Timer( killing_timeout,
									0, 
									(TimerHandlercpp)sigkill_starter,
									"sigkill_starter", this );
	if( kill_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


void
Resource::cancel_kill_timer()
{
	if( kill_tid != -1 ) {
		daemonCore->Cancel_Timer( kill_tid );
		kill_tid = -1;
		dprintf( D_FULLDEBUG, "Canceled kill timer.\n" );
	}
}


int
Resource::wants_vacate()
{
	int want_vacate = 0;
	if( r_cur->universe() == VANILLA ) {
		if( r_classad->EvalBool( "WANT_VACATE_VANILLA",
								   r_cur->ad(),
								   want_vacate ) == 0) { 
			want_vacate = 1;
		}
	} else {
		if( r_classad->EvalBool( "WANT_VACATE",
								   r_cur->ad(),
								   want_vacate ) == 0) { 
			want_vacate = 1;
		}
	}
	return want_vacate;
}


int 
Resource::wants_suspend()
{
	int want_suspend;
	if( r_cur->universe() == VANILLA ) {
		if( r_classad->EvalBool( "WANT_SUSPEND_VANILLA",
								   r_cur->ad(),
								   want_suspend ) == 0) {  
			want_suspend = 1;
		}
	} else {
		if( r_classad->EvalBool( "WANT_SUSPEND",
								   r_cur->ad(),
								   want_suspend ) == 0) { 
			want_suspend = 1;
		}
	}
	return want_suspend;
}


int 
Resource::wants_pckpt()
{
	int want_pckpt; 

	if( r_cur->universe() != STANDARD ) {
		return FALSE;
	}

	if( r_classad->EvalBool( "PERIODIC_CHECKPOINT",
							 r_cur->ad(),
							 want_pckpt ) == 0) { 
			// Default to no, if not defined.
		want_pckpt = 0;
	}
	return want_pckpt;
}


int
Resource::eval_kill()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "KILL_VANILLA",
									r_cur->ad(), tmp) ) == 0 ) {  
			if( (r_classad->EvalBool( "KILL",
										r_classad,
										tmp) ) == 0 ) { 
				EXCEPT("Can't evaluate KILL");
			}
		}
	} else {
		if( (r_classad->EvalBool( "KILL",
									r_cur->ad(), 
									tmp)) == 0 ) { 
			EXCEPT("Can't evaluate KILL");
		}	
	}
	return tmp;
}


int
Resource::eval_preempt()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "PREEMPT_VANILLA",
								   r_cur->ad(), 
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "PREEMPT",
									   r_cur->ad(), 
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate PREEMPT");
			}
		}
	} else {
		if( (r_classad->EvalBool( "PREEMPT",
								   r_cur->ad(), 
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate PREEMPT");
		}
	}
	return tmp;
}


int
Resource::eval_suspend()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "SUSPEND_VANILLA",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "SUSPEND",
									   r_cur->ad(),
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate SUSPEND");
			}
		}
	} else {
		if( (r_classad->EvalBool( "SUSPEND",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate SUSPEND");
		}
	}
	return tmp;
}


int
Resource::eval_continue()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "CONTINUE_VANILLA",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "CONTINUE",
									   r_cur->ad(),
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate CONTINUE");
			}
		}
	} else {	
		if( (r_classad->EvalBool( "CONTINUE",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate CONTINUE");
		}
	}
	return tmp;
}


void
Resource::make_public_ad(ClassAd* pubCA)
{
	char*	expr;
	char*	ptr;
	char	tmp[1024];
	State	s;

	pubCA->SetMyTypeName( STARTD_ADTYPE );
	pubCA->SetTargetTypeName( JOB_ADTYPE );

	caInsert( pubCA, r_classad, ATTR_NAME );
	caInsert( pubCA, r_classad, ATTR_MACHINE );

		// Insert all state info.
	r_state->publish( pubCA, A_PUBLIC );

		// Insert all info from the machine and CPU we care about. 
	resmgr->m_attr->publish( pubCA, A_PUBLIC );
	r_attr->publish( pubCA, A_PUBLIC );

		// Put everything in the public classad from STARTD_EXPRS. 
	config_fill_ad( pubCA );

		// Insert the currently active requirements expression, and
		// any other expressions it depends on (like START).
	r_reqexp->publish( pubCA, A_PUBLIC );

	caInsert( pubCA, r_classad, ATTR_RANK );
	caInsert( pubCA, r_classad, ATTR_CURRENT_RANK );

	s = this->state();
	if( s == claimed_state || s == preempting_state ) {
		caInsert( pubCA, r_classad, ATTR_CLIENT_MACHINE );
		caInsert( pubCA, r_classad, ATTR_REMOTE_USER );
		caInsert( pubCA, r_classad, ATTR_JOB_ID );
		caInsert( pubCA, r_classad, ATTR_JOB_START );
		caInsert( pubCA, r_classad, ATTR_LAST_PERIODIC_CHECKPOINT );
		if( startd_job_exprs ) {
			startd_job_exprs->rewind();
			while( (ptr = startd_job_exprs->next()) ) {
				caInsert( pubCA, r_cur->ad(), ptr );
			}
		}
	}		
}


void
Resource::make_private_ad(ClassAd* privCA)
{
	privCA->SetMyTypeName( STARTD_ADTYPE );
	privCA->SetTargetTypeName( JOB_ADTYPE );

	caInsert( privCA, r_classad, ATTR_NAME );
	caInsert( privCA, r_classad, ATTR_STARTD_IP_ADDR );
	caInsert( privCA, r_classad, ATTR_CAPABILITY );
}


void
Resource::publish( ClassAd* cap, amask_t mask ) 
{
	char line[128];

		// Put in cpu-specific attributes
	r_attr->publish( cap, mask );
	
		// Put in machine-wide attributes 
	resmgr->m_attr->publish( r_classad, A_UPDATE );

		// Put in state info
	r_state->publish( r_classad, mask );

		// Put in requirement expression info
	r_reqexp->publish( r_classad, mask );

		// Update info from the current Match object 
	r_cur->publish( r_classad, mask );

	if( IS_PUBLIC(mask) ) {
			// Add currently useful capability.  If r_pre exists, we  
			// need to advertise it's capability.  Otherwise, we
			// should  get the capability from r_cur.
		if( r_pre ) {
			sprintf( line, "%s = \"%s\"", ATTR_CAPABILITY, r_pre->capab() );
		} else {
			sprintf( line, "%s = \"%s\"", ATTR_CAPABILITY, r_cur->capab() );
		}		
		r_classad->Insert( line );
	}
}


void
Resource::compute( amask_t mask ) 
{
	r_attr->compute( mask );
}


void
Resource::dprintf( int flags, char* fmt, va_list args )
{
	if( resmgr->is_smp() ) {
		::dprintf( flags, "cpu%d: ", r_id );
		::_condor_dprintf_va( flags | D_NOHEADER, fmt, args );
	} else {
		::_condor_dprintf_va( flags, fmt, args );
	}
}


void
Resource::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	this->dprintf( flags, fmt, args );
	va_end( args );
}


int
Resource::display_load()
{
	dprintf( D_LOAD, 
			 "%s %.3f\t%s %.3f\t%s %.3f\n",  
			 "SystemLoad:", condor_load() + owner_load(),
			 "CondorLoad:", condor_load(),
			 "OwnerLoad:", owner_load() );
}

