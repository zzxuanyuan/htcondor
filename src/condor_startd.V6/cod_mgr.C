/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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


#include "condor_common.h"
#include "startd.h"


CODMgr::CODMgr( Resource* my_rip )
{
	rip = my_rip;
}


CODMgr::~CODMgr()
{
	Claim* tmp_claim;
	claims.Rewind();
	while( claims.Next(tmp_claim) ) {
		delete( tmp_claim );
		claims.DeleteCurrent();
	}
}


void
CODMgr::publish( ClassAd* ad, amask_t mask )
{
	int num_claims = numClaims();
	if( ! (IS_PUBLIC(mask) && IS_UPDATE(mask)) ) {
		return;
	}
	if( ! num_claims ) {
		return;
	}
	Claim* tmp_claim;
	claims.Rewind();
	while( claims.Next(tmp_claim) ) {
			// publish as appropriate :)
	}
	MyString line = ATTR_NUM_COD_CLAIMS;
	line += '=';
	line += num_claims;
	ad->Insert( line.Value() );
}



Claim*
CODMgr::findClaimById( const char* id )
{
	Claim* tmp_claim;
	claims.Rewind();
	while( claims.Next(tmp_claim) ) {
		if( tmp_claim->cap()->matches(id) ) {
			return tmp_claim;
		}
	}
	return NULL;
}


Claim*
CODMgr::findClaimByPid( pid_t pid )
{
	Claim* tmp_claim;
	claims.Rewind();
	while( claims.Next(tmp_claim) ) {
		if( tmp_claim->starterPidMatches(pid) ) {
			return tmp_claim;
		}
	}
	return NULL;
}


Claim*
CODMgr::addClaim( ) 
{
	Claim* new_claim;
	new_claim = new Claim( rip, true );
	new_claim->beginClaim();
	claims.Append( new_claim );
	return new_claim;
}


bool
CODMgr::removeClaim( Claim* c ) 
{
	bool found_it = false;
	Claim* tmp;
	claims.Rewind();
	while( claims.Next(tmp) ) {
		if( tmp == c ) {
			found_it = true;
			claims.DeleteCurrent();
		}
	}
	if( found_it ) {
		delete c;
	} else {
		dprintf( D_ALWAYS, 
				 "WARNING: CODMgr::removeClaim() could not find claim %s\n", 
				 c->id() );
	}
	return found_it;
}



int
CODMgr::numClaims( void )
{
	return claims.Number();
}


int
CODMgr::release( Stream* s, ClassAd* req, Claim* claim )
{
	VacateType vac_type = getVacateType( req );

		// tell this claim we're trying to release it
	claim->setWantsRelease( true );

		// stash the stream so we can notify it when we're done
	claim->setRequestStream( s );

	switch( claim->state() ) {

	case CLAIM_UNCLAIMED:
			// This is a programmer error.  we can't possibly get here  
		EXCEPT( "Trying to release a claim that was never claimed!" ); 
		break;

	case CLAIM_IDLE:
			// it's not running a job, so we can remove it
			// immediately.
		claim->finishRelease();
		break;

	case CLAIM_RUNNING:
	case CLAIM_SUSPENDED:
			// for these two, we have to kill the starter, and then
			// clean up the claim when it's gone.  so, all we can do
			// now is stash the Stream in the claim, and signal the
			// starter as appropriate;
		claim->deactivateClaim( vac_type == VACATE_GRACEFUL );
		break;

	case CLAIM_VACATING:
			// if we're already preempting gracefully, but the command
			// requested a fast shutdown, do the hardkill.  otherwise,
			// now that we know to release this claim, there's nothing
			// else to do except wait for the starter to exit.
			// work for us to do except wait.
		if( vac_type == VACATE_FAST ) {
			claim->deactivateClaim( false );
		}
		break;

	case CLAIM_KILLING:
			// if we're already trying to fast-kill, there's nothing
			// we can do now except wait for the starter to exit. 
		break;

	}
		// in general, we're going to have to wait to reply to the
  		// requesting entity until the starter exists.  even if we're
		// ready to reply right now, the finishRelease() method will
		// have deleted the stream, so in all cases, we want
		// DaemonCore to leave it alone.
	return KEEP_STREAM;
}


int
CODMgr::activate( Stream* s, ClassAd* req, Claim* claim )
{
		// TODO!
	return true;
}


int
CODMgr::deactivate( Stream* s, ClassAd* req, Claim* claim )
{
		// TODO!
	return true;
}


int
CODMgr::suspend( Stream* s, ClassAd* req, Claim* claim )
{
		// TODO!
	return true;
}


int
CODMgr::resume( Stream* s, ClassAd* req, Claim* claim )
{
		// TODO!
	return true;
}

