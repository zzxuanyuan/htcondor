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
#include "condor_classad.h"
#include "condor_classad_util.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "user_error_policy.h"
#include "user_job_policy.h"

//
// Error Actions String Versions
// These will be converted to the appropriate int values 
// by condor_submit
//
const char ERROR_ACTION_DEFAULT_STR	[] = "Default";
const char ERROR_ACTION_RETRY_STR	[] = "Retry";
const char ERROR_ACTION_HOLD_STR	[] = "Hold";
const char ERROR_ACTION_REMOVE_STR	[] = "Remove";


/**
 * 
 **/
ErrorPolicy::ErrorPolicy()
{
	this->ad = NULL;
	this->action = ERROR_ACTION_UNKNOWN;
	this->action_default = false;
}

/**
 *
 **/
ErrorPolicy::~ErrorPolicy()
{
	this->ad = NULL;
}

/**
 * 
 **/
void
ErrorPolicy::init( ClassAd *ad )
{
	this->ad = ad;
	this->setDefaults();
}

/**
 * 
 **/
bool
ErrorPolicy::containsError( ClassAd *ad )
{
	return ( ad->Lookup( ATTR_ERROR_REASON_CODE ) );
}

/**
 *
 **/
void
ErrorPolicy::setDefaults( )
{
		//
		// If they didn't specify an ErrorAction, we'll use
		// our default value
		//
	if ( this->ad->Lookup( ATTR_ERROR_ACTION ) == NULL) {
		InsertIntoAd( this->ad, ATTR_ERROR_ACTION, ERROR_ACTION_DEFAULT );
	}
}

//
// !!!!!!!!!!!!!!
// ATTR_ERROR_ACTION_DEFAULT or ATTR_LAST_ERROR_ACTION_DEFAULT???
// !!!!!!!!!!!!!!

/**
 * 
 **/
int
ErrorPolicy::analyzePolicy( )
{
	int error_code;
	MyString error_reason;
	int ret = UNDEFINED_EVAL;
	int cluster, proc;

		//
		// Make sure we have a job add before we try to do anything
		//
	if ( this->ad == NULL) {
		EXCEPT("ErrorPolicy: analyzePolicy() called before init()!");
	}
		//
		// To make our output more useful, let's provide the job info
		//
	this->ad->LookupInteger( ATTR_CLUSTER_ID, cluster );
	this->ad->LookupInteger( ATTR_PROC_ID, proc );
	
		//
		// We need to get the job state out of the ad first
		// This is important because if they specify RETRY, then
		// we need to make sure we do the right action. We save the 
		// state in our object because it's important to know the 
		// state of the job at the moment we are being called to know
		// why we took the action that we did
		//
		// If there is no state, then we need to EXCEPT because this
		// is a big problem
		//
	if ( ! this->ad->LookupInteger( ATTR_JOB_STATUS, this->job_state ) ) {
		MyString error;
		error.sprintf( "ErrorPolicy: Job %d.%d does not have a state!\n",
						cluster, proc );
		EXCEPT( (char*)error.Value() );
	}
	
		//
		// Now make sure we have at least an error code in our 
		// job ad. If there isn't one, then we don't need to do
		// anything. I think it's appropriate to send a message 
		// in the error log, but we don't want to except because that would
		// take down this daemon, which is something we probably don't
		// want to do for this minor mistake
		//
	if ( ! this->ad->LookupInteger( ATTR_ERROR_REASON_CODE, error_code ) ) {
		dprintf( D_FULLDEBUG, "ErrorPolicy: Job %d.%d does not have an error code. "
				"Skipping\n",
				cluster, proc );
		return ( ret );
	}
	this->ad->LookupString( ATTR_ERROR_REASON, error_reason );
	
		//
		// Get the action we should take. If there isn't one
		// then we won't do anything
		//
	if ( ! this->ad->LookupInteger( ATTR_ERROR_ACTION, this->action ) ) {
		dprintf( D_ALWAYS, "ErrorPolicy: Job %d.%d does not have an ErrorAction. "
				"Skipping\n",
				cluster, proc );
		return ( ret );
	}
	
		//
		// We need to check first if we are to use Condor's default
		// action for handling this error.
		//
	if ( this->action == ERROR_ACTION_DEFAULT ) {
			//
			// Mark a flag so that we know that we were originally
			// told to use the default action for this error. This will
			// be useful later on if we need to figure out why we did
			// what we did
			//
		this->action_default = true;
			//
			// We need to pull out what Condor says we should be
			// doing in this case. If it the default isn't defined
			// then we need to....
			//
		if ( ! this->ad->LookupInteger( ATTR_ERROR_ACTION_DEFAULT,
									  	this->action ) ) {
			// Need to ask somebody!
		}
	}
	
		//
		// Based on their ErrorAction, determine the action 
		// that they should take with their job
		//
	switch ( this->action ) {
		// --------------------------------------------
		// ERROR_ACTION_DEFAULT
		// This is just a sanity check to make sure that the 
		// default action isn't set to be the default action,
		// which is something that should never happen and 
		// a developer made a mistake somewhere
		// --------------------------------------------
		case ERROR_ACTION_DEFAULT: {
				//
				// Let's try to be helpful and provide the error
				// code and reason
				//
			MyString error = "ErrorJob: The default action was set to ";
			error += "ERROR_ACTION_DEFAULT!\n";
			error += "ErrorCode   = ";
			error += error_code;
			error += "ErrorReason = ";
			error += (!error_reason.IsEmpty() ? error_reason : "<EMPTY>");
			EXCEPT( (char*)error.Value() );
			break;
		}
		// --------------------------------------------
		// ERROR_ACTION_RETRY
		// --------------------------------------------
		case ERROR_ACTION_RETRY:
				//
				//
				//
			switch ( this->job_state ) {
				case RUNNING:
					ret = STAYS_IN_QUEUE;
					break;
				case HELD:
					ret = HOLD_IN_QUEUE;
					break;
					
			} // SWITCH
		
			break;			
		// --------------------------------------------
		// ERROR_ACTION_HOLD
		// Always put the job on hold
		// --------------------------------------------
		case ERROR_ACTION_HOLD:
			ret = HOLD_IN_QUEUE;
			break;
		// --------------------------------------------
		// ERROR_ACTION_REMOVE
		// Always remove the job from the queue
		// --------------------------------------------
		case ERROR_ACTION_REMOVE:
			ret = REMOVE_FROM_QUEUE;
			break;
		// --------------------------------------------
		// UNKNOWN
		// --------------------------------------------
		default:
			//
			// Mmmm....
			//
			int x = 1;
		
	} // SWITCH

	return ( ret );
}


/**
 * 
 * 
 **/
int
ErrorPolicy::errorAction( )
{
	return ( this->action );
}

/**
 * 
 * 
 **/
bool
ErrorPolicy::errorActionDefault( )
{
	return ( this->action_default );
}

/**
 * 
 **/
MyString
ErrorPolicy::errorActionReason( )
{
	MyString reason;
	MyString action_str;

		//
		// If we don't have a job ad, or we haven't evaluated
		// it yet, then there is nothing we can do
		//
	if ( this->ad == NULL || this->action == ERROR_ACTION_UNKNOWN ) {
		return ( NULL );
	}
	
	action_str = this->actionCodetoString( this->action );

		//
		// If the original action was set to do whatever the default was,
		// we need to make sure that we let them know
		//
	if ( this->action_default ) {
		reason.sprintf( "The job attribute %s was set to %s. "
						"The default action was set to %s for the job.",
						ATTR_ERROR_ACTION,
						ERROR_ACTION_DEFAULT_STR,
						action_str.Value() );
	} else {
		reason.sprintf( "The job attribute %s evaluated to %s.",
						ATTR_ERROR_ACTION,
						action_str.Value() );
	}

	return ( reason );
}

MyString
ErrorPolicy::actionCodetoString( int action ) {
	MyString ret;
	switch ( action ) {
		// --------------------------------------------
		// ERROR_ACTION_DEFAULT
		// --------------------------------------------
		case ERROR_ACTION_DEFAULT:
			ret = ERROR_ACTION_DEFAULT_STR;
			break;
		// --------------------------------------------
		// ERROR_ACTION_RETRY
		// --------------------------------------------
		case ERROR_ACTION_RETRY:
			ret = ERROR_ACTION_RETRY_STR;
			break;
		// --------------------------------------------
		// ERROR_ACTION_HOLD
		// --------------------------------------------
		case ERROR_ACTION_HOLD:
			ret = ERROR_ACTION_HOLD_STR;
			break;
		// --------------------------------------------
		// ERROR_ACTION_REMOVE
		// --------------------------------------------
		case ERROR_ACTION_REMOVE:
			ret = ERROR_ACTION_REMOVE_STR;
			break;
		// --------------------------------------------
		// UNKNOWN
		// --------------------------------------------
		default:
			ret = "UNKNOWN (invalid action)";
	} // SWITCH
	return ( ret );
}




