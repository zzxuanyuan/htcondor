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


void
CODMgr::starterExited( Claim* c ) 
{
	if( c->hasPendingCmd() ) {
			// if we're in the middle of a pending command, we can
			// finally complete it and reply now that the starter is
			// gone and we're done cleaning up everything.
		c->finishPendingCmd();
		return;
	}

		// otherwise, the claim is back to idle again, so we should
		// see if we can resume our opportunistic claim, if we've got
		// one... 
		// TODO!!!
}


int
CODMgr::numClaims( void )
{
	return claims.Number();
}


bool
CODMgr::inUse( void )
{
	Claim* tmp;
	claims.Rewind();
	while( claims.Next(tmp) ) {
		if( tmp->isActive() ) {
			return true;
		}
	}
	return false;
}


void
CODMgr::shutdownAllClaims( bool graceful )
{
	Claim* tmp;
	claims.Rewind();
	while( claims.Next(tmp) ) {
		tmp->deactivateClaim( graceful );
	}
}


int
CODMgr::release( Stream* s, ClassAd* req, Claim* claim )
{
	VacateType vac_type = getVacateType( req );

		// tell this claim we're trying to release it
	claim->setPendingCmd( CA_RELEASE_CLAIM );
	claim->setRequestStream( s );

	switch( claim->state() ) {

	case CLAIM_UNCLAIMED:
			// This is a programmer error.  we can't possibly get here  
		EXCEPT( "Trying to release a claim that was never claimed!" ); 
		break;

	case CLAIM_IDLE:
			// it's not running a job, so we can remove it
			// immediately.
		claim->finishPendingCmd();
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
		// ready to reply right now, the finishPendingCmd() method will
		// have deleted the stream, so in all cases, we want
		// DaemonCore to leave it alone.
	return KEEP_STREAM;
}


int
CODMgr::activate( Stream* s, ClassAd* req, Claim* claim )
{
	MyString err_msg;
	ClassAd *mach_classad = rip->r_classad;

		// first, we have to find a Starter that matches the request
	Starter* tmp_starter;
	tmp_starter = resmgr->starter_mgr.findStarter( req, mach_classad );
	if( ! tmp_starter ) {
		char* tmp = NULL;
		req->LookupString( ATTR_REQUIREMENTS, &tmp );
		if( ! tmp ) {
			err_msg = "Request does not contain ";
			err_msg += ATTR_REQUIREMENTS;
			err_msg += ", can't find a valid starter to activate";
			return sendErrorReply( s, "CA_ACTIVATE_CLAIM",
								   CA_INVALID_REQUEST, err_msg.Value() ); 
		}
		err_msg = "Cannot find starter that satisfies requirements '";
		err_msg += tmp;
		err_msg = "'";
		free( tmp );
		return sendErrorReply( s, "CA_ACTIVATE_CLAIM",
							   CA_INVALID_REQUEST, err_msg.Value() );
	}

	char* keyword = NULL;
	if( ! req->LookupString(ATTR_JOB_KEYWORD, &keyword) ) {
		err_msg = "Request does not contain ";
		err_msg += ATTR_JOB_KEYWORD;
		err_msg += ", so server cannot find job in config file\n";
		delete tmp_starter;
		return sendErrorReply( s, "CA_ACTIVATE_CLAIM",
							   CA_INVALID_REQUEST, err_msg.Value() ); 
	}
	tmp_starter->setCODArgs( keyword );
	free( keyword );

		// Grab the job ID so we've got it, and can use it to spawn
		// the starter with the right args if needed...
	claim->getJobId( req );

	time_t now = time(NULL);

	claim->setStarter( tmp_starter );	
	claim->spawnStarter( now );

		// we need to make a copy of this, since the original is on
		// the stack in command.C:command_classad_handler().
		// otherwise, once the handler completes, we've got a dangling
		// pointer, and if we try to access this variable, we'll crash 
	ClassAd* new_req_ad = new ClassAd( *req );
	claim->beginActivation( new_req_ad, now );


		// TODO: deal w/ state interactions w/ opportunistic claim!!!

	ClassAd reply;

	MyString line = ATTR_RESULT;
	line += " = \"";
	line += getCAResultString( CA_SUCCESS );
	line += '"';
	reply.Insert( line.Value() );

		// TODO any other info for the reply?

	sendCAReply( s, "CA_ACTIVATE_CLAIM", &reply );

	return TRUE;
}


int
CODMgr::deactivate( Stream* s, ClassAd* req, Claim* claim )
{
	MyString err_msg;
	VacateType vac_type = getVacateType( req );

	claim->setPendingCmd( CA_DEACTIVATE_CLAIM );
	claim->setRequestStream( s );

	switch( claim->state() ) {

	case CLAIM_UNCLAIMED:
			// This is a programmer error.  we can't possibly get here  
		EXCEPT( "Trying to deactivate a claim that was never claimed!" ); 
		break;

	case CLAIM_IDLE:
			// it is not activate, so return an error
		err_msg = "Attempt to deactivate a claim that is not active ";
		err_msg += "(current state: '";
		err_msg += getClaimStateString( CLAIM_IDLE );
		err_msg += "')";

		claim->setRequestStream( NULL );
		claim->setPendingCmd( -1 );
		return sendErrorReply( s, "CA_DEACTIVATE_CLAIM",
							   CA_INVALID_STATE, err_msg.Value() ); 
		break;

	case CLAIM_RUNNING:
	case CLAIM_SUSPENDED:
			// for these two, we have to kill the starter, and then
			// notify the other side when it's gone.  so, all we can
			// do now is stash the Stream in the claim, and signal the
			// starter as appropriate;
		claim->deactivateClaim( vac_type == VACATE_GRACEFUL );
		break;

	case CLAIM_VACATING:
			// if we're already preempting gracefully, but the command
			// requested a fast shutdown, do the hardkill.  otherwise,
			// now that we set the flag so we know to reply to this
			// stream, there's nothing else to do except wait for the
			// starter to exit.
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
		// ready to reply right now, the finishPendingCmd() method will
		// have deleted the stream, so in all cases, we want
		// DaemonCore to leave it alone.
	return KEEP_STREAM;
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

