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

/*  
  	This file implements the classes defined in claim.h.  See that
	file for comments and documentation on what it's about.

	Originally written 9/29/97 by Derek Wright <wright@cs.wisc.edu>

	Decided the Match object should really be called "Claim" (the
	files were renamed in cvs from Match.[Ch] to claim.[Ch], and
	renamed everything on 1/10/03 - Derek Wright
*/

#include "condor_common.h"
#include "startd.h"

///////////////////////////////////////////////////////////////////////////
// Claim
///////////////////////////////////////////////////////////////////////////

Claim::Claim( Resource* rip, bool is_cod )
{
	c_client = new Client;
	c_cap = new Capability( is_cod );
	c_ad = NULL;
	c_starter = NULL;
	c_rank = 0;
	c_oldrank = 0;
	c_universe = -1;
	c_agentstream = NULL;
	c_match_tid = -1;
	c_claim_tid = -1;
	c_aliveint = -1;
	c_cluster = -1;
	c_proc = -1;
	c_job_start = -1;
	c_last_pckpt = -1;
	c_rip = rip;
	c_state = CLAIM_UNCLAIMED;
	c_is_cod = is_cod;
}


Claim::~Claim()
{	
		// Cancel any timers associated with this claim
	this->cancel_match_timer();
	this->cancel_claim_timer();

		// Free up memory that's been allocated
	if( c_ad ) {
		delete( c_ad );
	}
	delete( c_cap );
	if( c_client ) {
		delete( c_client );
	}
	if( c_agentstream ) {
		delete( c_agentstream );
	}
	if( c_starter ) {
		delete( c_starter );
	}

}	
	

void
Claim::vacate() 
{
	assert( c_cap );
		// warn the client of this claim that it's being vacated
	if( c_client && c_client->addr() ) {
		c_client->vacate( c_cap->capab() );
	}
}


void
Claim::publish( ClassAd* ad, amask_t how_much )
{
	char line[256];
	char* tmp;

	if( IS_PRIVATE(how_much) ) {
		return;
	}

	sprintf( line, "%s = %f", ATTR_CURRENT_RANK, c_rank );
	ad->Insert( line );

	if( c_client ) {
		tmp = c_client->user();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_REMOTE_USER, tmp );
			ad->Insert( line );
		}
		tmp = c_client->owner();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_REMOTE_OWNER, tmp );
			ad->Insert( line );
		}
		tmp = c_client->host();
		if( tmp ) {
			sprintf( line, "%s=\"%s\"", ATTR_CLIENT_MACHINE, tmp );
			ad->Insert( line );
		}
	}

	if( (c_cluster > 0) && (c_proc >= 0) ) {
		sprintf( line, "%s=\"%d.%d\"", ATTR_JOB_ID, c_cluster, c_proc );
		ad->Insert( line );
	}

	if( c_job_start > 0 ) {
		sprintf(line, "%s=%d", ATTR_JOB_START, c_job_start );
		ad->Insert( line );
	}

	if( c_last_pckpt > 0 ) {
		sprintf(line, "%s=%d", ATTR_LAST_PERIODIC_CHECKPOINT, c_last_pckpt );
		ad->Insert( line );
	}
}	


void
Claim::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	c_rip->dprintf_va( flags, fmt, args );
	va_end( args );
}


void
Claim::refuse_agent()
{
	if( !c_agentstream ) return;
	dprintf( D_ALWAYS, "Refusing request from schedd agent.\n" );
	c_agentstream->encode();
	c_agentstream->put(NOT_OK);
	c_agentstream->end_of_message();
}


void
Claim::start_match_timer()
{
	if( c_match_tid != -1 ) {
			/*
			  We got matched twice for the same capability.  This
			  must be because we got matched, we sent an update that
			  said we're unavailable, but the collector dropped that
			  update, and we got matched again.  This shouldn't be a
			  fatal error, b/c UDP gets dropped all the time.  We just
			  need to cancel the old timer, print a warning, and then
			  continue. 
			*/
		
	   dprintf( D_FAILURE|D_ALWAYS, "Warning: got matched twice for same capability."
				" Canceling old match timer (%d)\n", c_match_tid );
	   if( daemonCore->Cancel_Timer(c_match_tid) < 0 ) {
		   dprintf( D_ALWAYS, "Failed to cancel old match timer (%d): "
					"daemonCore error\n", c_match_tid );
	   } else {
		   dprintf( D_FULLDEBUG, "Cancelled old match timer (%d)\n", 
					c_match_tid );
	   }
	   c_match_tid = -1;
	}

	c_match_tid = 
		daemonCore->Register_Timer( match_timeout, 0, 
								   (TimerHandlercpp)
								   &Claim::match_timed_out,
								   "match_timed_out", this );
	if( c_match_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started match timer (%d) for %d seconds.\n", 
			 c_match_tid, match_timeout );
}


void
Claim::cancel_match_timer()
{
	int rval;
	if( c_match_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( c_match_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel match timer (%d): "
					 "daemonCore error\n", c_match_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled match timer (%d)\n", 
					 c_match_tid );
		}
		c_match_tid = -1;
	}
}


int
Claim::match_timed_out()
{
	char* my_cap = capab();
	if( !my_cap ) {
			// We're all confused.
			// Don't use our dprintf(), use the "real" version, since
			// if we're this confused, our rip pointer might be messed
			// up, too, and we don't want to seg fault.
		::dprintf( D_FAILURE|D_ALWAYS,
				   "ERROR: Match timed out but there's no capability\n" );
		return FALSE;
	}
		
	Resource* rip = resmgr->get_by_any_cap( my_cap );
	if( !rip ) {
		::dprintf( D_FAILURE|D_ALWAYS,
				   "ERROR: Can't find resource of expired match\n" );
		return FALSE;
	}

	if( rip->r_cur->cap()->matches( capab() ) ) {
		if( rip->state() != matched_state ) {
				/* 
				   This used to be an EXCEPT(), since it really
				   shouldn't ever happen.  However, it kept happening,
				   and we couldn't figure out why.  For now, just log
				   it and silently ignore it, since there's no real
				   harm done, anyway.  We use D_FULLDEBUG, since we
				   don't want people to worry about it if they see it
				   in D_ALWAYS in the 6.2.X stable series.  However,
				   in the 6.3 series, we should probably try to figure 
				   out what's going on with this, for example, by
				   sending email at this point with the last 300 lines
				   of the log file or something.  -Derek 10/9/00
				*/
			dprintf( D_FAILURE|D_FULLDEBUG, 
					 "WARNING: Current match timed out but in %s state.",
					 state_to_string(rip->state()) );
			return FALSE;
		}
		delete rip->r_cur;
		rip->r_cur = new Claim( rip );
		dprintf( D_FAILURE|D_ALWAYS, "State change: match timed out\n" );
		rip->change_state( owner_state );
	} else {
			// The match that timed out was the preempting claim.
		assert( rip->r_pre->cap()->matches( capab() ) );
			// We need to generate a new preempting claim object,
			// restore our reqexp, and update the CM. 
		delete rip->r_pre;
		rip->r_pre = new Claim( rip );
		rip->r_reqexp->restore();
		rip->update();
	}		
	return TRUE;
}


void
Claim::start_claim_timer()
{
		// for now, we should change our claim state in here, since
		// this is called once the Claim is finally claimed by
		// someone.  this will all probably be changed soon, since
		// having the ResState code starting and stopping timers on
		// the Claim object isn't really a good idea. :)
	ASSERT( c_state == CLAIM_UNCLAIMED );
	c_state = CLAIM_IDLE;

	if( c_aliveint < 0 ) {
		dprintf( D_ALWAYS, 
				 "Warning: starting claim timer before alive interval set.\n" );
		c_aliveint = 300;
	}
	if( c_claim_tid != -1 ) {
	   EXCEPT( "Claim::start_claim_timer() called w/ c_claim_tid = %d", 
			   c_claim_tid );
	}
	c_claim_tid =
		daemonCore->Register_Timer( (3 * c_aliveint), 0,
				(TimerHandlercpp)&Claim::claim_timed_out,
				"claim_timed_out", this );
	if( c_claim_tid == -1 ) {
		EXCEPT( "Couldn't register timer (out of memory)." );
	}
	dprintf( D_FULLDEBUG, "Started claim timer (%d) w/ %d second "
			 "alive interval.\n", c_claim_tid, c_aliveint );
}


void
Claim::cancel_claim_timer()
{
	int rval;
	if( c_claim_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( c_claim_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel claim timer (%d): "
					 "daemonCore error\n", c_claim_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled claim timer (%d)\n",
					 c_claim_tid );
		}
		c_claim_tid = -1;
	}
}


int
Claim::claim_timed_out()
{
	Resource* rip = resmgr->get_by_cur_cap( capab() );
	if( !rip ) {
		EXCEPT( "Can't find resource of expired claim." );
	}
		// Note that this claim timed out so we don't try to send a 
		// command to our client.
	if( c_client ) {
		delete c_client;
		c_client = NULL;
	}

	dprintf( D_FAILURE|D_ALWAYS, "State change: claim timed out (condor_schedd gone?)\n" );

		// Kill the claim.
	rip->kill_claim();
	return TRUE;
}


void
Claim::alive()
{
		// Process a keep alive command
	daemonCore->Reset_Timer( c_claim_tid, (3 * c_aliveint), 0 );
}


// Set our ad to the given pointer
void
Claim::setad(ClassAd *ad) 
{
	if( c_ad ) {
		delete( c_ad );
	}
	c_ad = ad;
}


void
Claim::deletead(void)
{
	if( c_ad ) {
		delete( c_ad );
		c_ad = NULL;
	}
}


void
Claim::setagentstream(Stream* stream)
{
	if( c_agentstream ) {
		delete( c_agentstream );
	}
	c_agentstream = stream;
}


char*
Claim::capab( void )
{
	if( c_cap ) {
		return c_cap->capab();
	} else {
		return NULL;
	}
}


float
Claim::percentCpuUsage( void )
{
	if( c_starter ) {
		return c_starter->percentCpuUsage();
	} else {
		return 0.0;
	}
}


unsigned long
Claim::imageSize( void )
{
	if( c_starter ) {
		return c_starter->imageSize();
	} else {
		return 0;
	}
}


int
Claim::spawnStarter( start_info_t* info, time_t now )
{
	int rval;
	if( ! c_starter ) {
			// Big error!
		dprintf( D_ALWAYS, "ERROR! Claim::spawnStarter() called "
				 "w/o a Starter object! Returning failure\n" );
		return 0;
	}

	rval = c_starter->spawn( info, now );

	c_state = CLAIM_RUNNING; 

		// Fake ourselves out so we take another snapshot in 15
		// seconds, once the starter has had a chance to spawn the
		// user job and the job as (hopefully) done any initial
		// forking it's going to do.  If we're planning to check more
		// often that 15 seconds, anyway, don't bother with this.
	if( pid_snapshot_interval > 15 ) {
		c_starter->set_last_snapshot( (now + 15) -
									  pid_snapshot_interval );
	} 
	return rval;
}


void
Claim::setStarter( Starter* s )
{
	if( c_starter ) {
		EXCEPT( "Claim::setStarter() called with existing starter!" );
	}
	c_starter = s;
	if( s ) {
		s->setResource( this->c_rip );
	}
}


void
Claim::starterExited( void )
{
		// Now that the starter is gone, we need to change our state
	c_state = CLAIM_IDLE;

		// Notify our starter object that its starter exited, so it
		// can cancel timers any pending timers, cleanup the starter's
		// execute directory, and do any other cleanup. 
	c_starter->exited();
	
		// Next, we can delete the starter object itself.
	delete( c_starter );
	c_starter = NULL;
	
		// finally, let our resource know that our starter exited, so
		// it can do the right thing.
	c_rip->starterExited( this );
}


bool
Claim::starterPidMatches( pid_t starter_pid )
{
	if( c_starter && c_starter->pid() == starter_pid ) {
		return true;
	}
	return false;
}


bool
Claim::isDeactivating( void )
{
	if( c_state == CLAIM_PREEMPTING || c_state == CLAIM_KILLING ) {
		return true;
	}
	return false;
}


bool
Claim::isActive( void )
{
	if( c_starter && c_starter->active() ) {
			// TODO 
			// this assert is wrong, since we could be preempting or
			// killing, too.  i need to figure out if we should say
			// the claim is active if we're trying to deactivate it or
			// not...
			// ASSERT( c_state == CLAIM_RUNNING || c_state == CLAIM_SUSPENDED );
		return true;
	}
	return false;
}


bool
Claim::deactivateClaim( bool graceful )
{
	if( isActive() ) {
			// Singal the starter
		if( graceful ) {
			c_state = CLAIM_PREEMPTING;
			return starterKillSoft();
		} else {
			c_state = CLAIM_KILLING;
			return starterKillHard();
		}
	}
	return true;
}


bool
Claim::suspendClaim( void )
{
	c_state = CLAIM_SUSPENDED;
	if( c_starter ) {
		return (bool)c_starter->suspend();
	}
		// if there's no starter, we don't need to do anything, so
		// it worked...  
	return true;
}


bool
Claim::resumeClaim( void )
{
	if( c_starter ) {
		c_state = CLAIM_RUNNING;
		return (bool)c_starter->resume();
	}
		// if there's no starter, we don't need to do anything, so
		// it worked...  
	c_state = CLAIM_IDLE;
	return true;
}


bool
Claim::starterKill( int sig )
{
		// don't need to work about the state, since we don't use this
		// method to send any signals that change the claim state...
	if( c_starter ) {
		return (bool)c_starter->kill( sig );
	}
		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillPg( int sig )
{
	if( c_starter ) {
			// if we're using KillPg, we're trying to hard-kill the
			// starter and all its children
		c_state = CLAIM_KILLING;
		return (bool)c_starter->killpg( sig );
	}
		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillSoft( void )
{
	if( c_starter ) {
		c_state = CLAIM_PREEMPTING;
		return c_starter->killSoft();
	}
		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::starterKillHard( void )
{
	if( c_starter ) {
		c_state = CLAIM_KILLING;
		return c_starter->killHard();
	}
		// if there's no starter, we don't need to kill anything, so
		// it worked...  
	return true;
}


bool
Claim::periodicCheckpoint( void )
{
	if( c_starter ) {
		if( ! c_starter->kill(DC_SIGPCKPT) ) { 
			return false;
		}
	}
	setlastpckpt( (int)time(NULL) );
	return true;
}


///////////////////////////////////////////////////////////////////////////
// Client
///////////////////////////////////////////////////////////////////////////

Client::Client()
{
	c_user = NULL;
	c_owner = NULL;
	c_addr = NULL;
	c_host = NULL;
}


Client::~Client() 
{
	if( c_user) free( c_user );
	if( c_owner) free( c_owner );
	if( c_addr) free( c_addr );
	if( c_host) free( c_host );
}


void
Client::setuser( char* user )
{
	if( c_user ) {
		free( c_user);
	}
	if( user ) {
		c_user = strdup( user );
	} else {
		c_user = NULL;
	}
}


void
Client::setowner( char* owner )
{
	if( c_owner ) {
		free( c_owner);
	}
	if( owner ) {
		c_owner = strdup( owner );
	} else {
		c_owner = NULL;
	}
}


void
Client::setaddr(char* addr)
{
	if( c_addr ) {
		free( c_addr);
	}
	if( addr ) {
		c_addr = strdup( addr );
	} else {
		c_addr = NULL;
	}
}


void
Client::sethost(char* host)
{
	if( c_host ) {
		free( c_host);
	}
	if( host ) {
		c_host = strdup( host );
	} else {
		c_host = NULL;
	}
}


void
Client::vacate(char* cap)
{
	ReliSock* sock;

	if( ! (c_addr || c_host || c_owner ) ) {
			// Client not really set, nothing to do.
		return;
	}

	dprintf(D_FULLDEBUG, "Entered vacate_client %s %s...\n", c_addr, c_host);

	Daemon my_schedd( DT_SCHEDD, c_addr, NULL);
	sock = (ReliSock*)my_schedd.startCommand( RELEASE_CLAIM,
											  Stream::reli_sock, 20 );
	if( ! sock ) {
		dprintf(D_FAILURE|D_ALWAYS, "Can't connect to schedd (%s)\n", c_addr);
		return;
	}
	if( !sock->put( cap ) ) {
		dprintf(D_ALWAYS, "Can't send capability to client\n");
	} else if( !sock->eom() ) {
		dprintf(D_ALWAYS, "Can't send EOM to client\n");
	}

	sock->close();
	delete sock;
}


///////////////////////////////////////////////////////////////////////////
// Capability
///////////////////////////////////////////////////////////////////////////

char*
newCapabilityString()
{
	char cap[128];
	char tmp[128];
	char randbuf[12];
	randbuf[0] = '\0';
	int i, len;

		// Create a really mangled 10 digit random number: The first 6
		// digits are generated as follows: for the ith digit, pull
		// the ith digit off a new random int.  So our 1st slot comes
		// from the 1st digit of 1 random int, the 2nd from the 2nd
		// digit of a 2nd random it, etc...  If we're trying to get a
		// digit from a number that's too small to have that many, we
		// just use the last digit.  The last 4 digits of our number
		// come from the first 4 digits of the current time multiplied
		// by a final random int.  That should keep 'em guessing. :)
		// -Derek Wright 1/8/98
	for( i=0; i<6; i++ ) {
		sprintf( tmp, "%d", get_random_int() );
		len = strlen(tmp);
		if( i < len ) {
			tmp[i+1] = '\0';
			strcat( randbuf, tmp+i );
		} else {
			strcat( randbuf, tmp+(len-1) );
		}
	}
	sprintf( tmp, "%f", (double)((float)time(NULL) * (float)get_random_int()) );
	tmp[4]='\0';
	strcat( randbuf, tmp );

		// Capability string is "<ip:port>#random_number"
	strcpy( cap, daemonCore->InfoCommandSinfulString() );
	strcat( cap, "#" );
	strcat( cap, randbuf );
	return strdup( cap );
}


char*
newCODIdString()
{
		// COD id string (capability) is of the form:
		// "<ip:port>#COD#startd_bday#sequence_num"

	MyString id;
	char startd_bday_str[32];
	char seq_num_str[16];

	static int sequence_num = 0;

		// put the integers we need into a string so we can add them
		// to our MyString
	sprintf( startd_bday_str, "%ld", (long)startd_startup );
	sprintf( seq_num_str, "%d", sequence_num );

	id += daemonCore->InfoCommandSinfulString();
	id += '#';
	id += startd_bday_str;
	id += '#';
	id += seq_num_str;
	return strdup( id.Value() );
}


Capability::Capability( bool is_cod )
{
	if( is_cod ) { 
		c_capab = newCODIdString();
	} else {
		c_capab = newCapabilityString();
	}
}


Capability::~Capability()
{
	free( c_capab );
}


bool
Capability::matches( const char* capab )
{
	return( strcmp(capab, c_capab) == 0 );
}

