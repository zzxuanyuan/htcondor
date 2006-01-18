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
#ifndef USER_ERROR_POLICY_H
#define USER_ERROR_POLICY_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "user_job_policy.h"

//
// Error Actions Codes
//
enum {	ERROR_ACTION_DEFAULT = 0,
		ERROR_ACTION_RETRY,
		ERROR_ACTION_HOLD,
		ERROR_ACTION_REMOVE,
		ERROR_ACTION_UNKNOWN
};
//
// Error Actions String Versions
// These will be converted to the appropriate int values 
// by condor_submit. These are defined in error_job_policy.C
//
extern const char ERROR_ACTION_DEFAULT_STR[];
extern const char ERROR_ACTION_RETRY_STR[];
extern const char ERROR_ACTION_HOLD_STR[];
extern const char ERROR_ACTION_REMOVE_STR[];

/**
 * 
 * We will use the action identifiers from user_job_policy:
 * 	STAYS_IN_QUEUE
 * 	REMOVE_FROM_QUEUE
 * 	HOLD_IN_QUEUE
 * 	UNDEFINED_EVAL
 * 	RELEASE_FROM_HOLD
 * 
 **/
class ErrorPolicy {
	public:
			/**
			 * 
			 **/
		ErrorPolicy();
		
			/**
			 * 
			 **/
		~ErrorPolicy();
		
			/**
			 * 
			 **/
		static bool containsError( ClassAd *ad );

			/**
			 * This class NEVER owns this memory, it just has a reference
			 * to it. It also makes sure the default policy expressions
			 * are set in the classad if they were undefined. This must be
			 * called FIRST when you initially set up one of these classes.
			 * 
			 **/
		void init( ClassAd *ad );

			/**
			 * 
			 * 
			 **/
		int analyzePolicy();
		
			/**
			 * This returns a string explaining what expression fired, useful
			 * for a Reason string in the job ad. If no firing expression 
			 * occurred, then NULL is returned. The user does NOT free this
			 * memory and it is overwritten when FiringReason() is called
			 * again (for any ErrorPolicy object).
			 **/
		MyString errorActionReason(void);

			/**
			 * 
			 * 
			 **/
		int errorAction( void );

			/**
			 * 
			 * 
			 **/
		bool errorActionDefault( void );

	protected:

			/**
			 * This function inserts the four user job policy expressions with
			 * default values into the classad if they are not already present.
			 **/
		void setDefaults(void);
		
			/**
			 * 
			 * 
			 **/
		MyString actionCodetoString( int action );

		/* I can't be copied */
		ErrorPolicy(const ErrorPolicy&);
		ErrorPolicy& operator=(const ErrorPolicy&);

			///
			/// The job ad that will analyze 
			///
		ClassAd *ad;
			///
			///
			///
		int action;
			///
			///
			///
		bool action_default;
			///
			///
			///
		int job_state;		
}; // END CLASS

#endif
