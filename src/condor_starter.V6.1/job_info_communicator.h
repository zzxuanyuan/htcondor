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

#if !defined(_CONDOR_JOB_INFO_COMMUNICATOR_H)
#define _CONDOR_JOB_INFO_COMMUNICATOR_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_classad.h"
#include "user_proc.h"

/** 
	This class is a base class for the various ways a starter can
	recieve and send information about the underlying job.  For now,
	there are two main ways to do this: 1) to talk to a condor_shadow
	and 2) the local filesystem, command line args, etc.
*/

class JobInfoCommunicator : public Service {
public:
		/// Constructor
	JobInfoCommunicator();

		/// Destructor
	virtual ~JobInfoCommunicator();

		/// Pure virtual functions:

		// // // // // // // // // // // //
		// Initialization
		// // // // // // // // // // // //

		/** Initialize ourselves.  This should perform the following
			actions, no matter what kind of job controller we're
			dealing with:
			- aquire the job classad
			- call registerStarterInfo()
			- call initUserPriv()
			- call initJobInfo()
			@return true if we successfully initialized, false if we
			   failed and need to abort
		*/
	virtual bool init( void ) = 0;

		/// Read anything relevent from the config file
	virtual void config( void ) = 0;

		/** Setup the execution environment for the job.  
		 */
	virtual void setupJobEnvironment( void ) = 0;

		// // // // // // // // // // // //
		// Information about the job 
		// // // // // // // // // // // //
	
		/** Return a pointer to the filename to use for the job's
			standard input file.
		*/
	virtual const char* jobInputFilename( void );	

		/** Return a pointer to the filename to use for the job's
			standard output file.
		*/
	virtual const char* jobOutputFilename( void );	

		/** Return a pointer to the filename to use for the job's
			standard error file.
		*/
	virtual const char* jobErrorFilename( void );	

		/** Return a pointer to the job's initial working directory. 
		*/
	virtual const char* jobIWD( void );

		/// Return a pointer to the original name for the job.
	virtual const char* origJobName( void );

		/// Return a pointer to the ClassAd for our job.
	virtual ClassAd* jobClassAd( void );


		// // // // // // // // // // // //
		// Job execution and state changes
		// // // // // // // // // // // //

		/** All jobs have been spawned by the starter.
		 */
	virtual void allJobsSpawned( void ) = 0;

		/** The starter has been asked to suspend.  Take whatever
			steps make sense for the JIC, and notify our job
			controller what happend.
		*/
	virtual void Suspend( void ) = 0;

		/** The starter has been asked to continue.  Take whatever
			steps make sense for the JIC, and notify our job
			controller what happend.
		*/
	virtual void Continue( void ) = 0;

		/** The last job this starter is controlling has exited.  Do
			whatever we have to do to cleanup and notify our
			controller. 
		*/
	virtual void allJobsDone( void ) = 0;

		/** The starter has been asked to shutdown fast.
		 */
	virtual void gotShutdownFast( void );

		/** The starter has been asked to shutdown gracefully.
		 */
	virtual void gotShutdownGraceful( void );


		// // // // // // // // // // // //
		// Notfication to our controller
		// // // // // // // // // // // //

		/** Notifyour controller that the job is about to spawn
		 */
	virtual void notifyJobPreSpawn( void ) = 0;

		/** Notify our controller the given info about the job
			@param update_ad A ClassAd with updated info about the job
		*/
	virtual bool updateJobInfo( ClassAd* update_ad ) = 0;

		/** Notify our controller that the job exited
			@param exit_status The exit status from wait()
			@param reason The Condor-defined exit reason
			@param user_proc The UserProc that was running the job
		*/
	virtual bool notifyJobExit( int exit_status, int reason, 
								UserProc* user_proc ) = 0;


		// // // // // // // // // // // //
		// Misc utilities
		// // // // // // // // // // // //

		/** Make sure the given filename will be included in the
			output files of the job that are sent back to the job
			submitter.  
			@param filename File to add to the job's output list 
		*/
	virtual void addToOutputFiles( const char* filename ) = 0;

		/** Compare our own UIDDomain vs. where the job came from.
			@return true if they match, false if not
		*/
	virtual bool sameUidDomain( void ) = 0;


protected:

		// // // // // // // // // // // //
		// Protected helper methods
		// // // // // // // // // // // //

		/** Register some important information about ourself that the
			job controller might needs.
			@return true on success, false on failure
		*/
	virtual	bool registerStarterInfo( void ) = 0;

		/** Initialize the priv_state code with the appropriate user
			for this job.
			@return true on success, false on failure
		*/
	virtual bool initUserPriv( void ) = 0;

		/** Publish information into the given classad for updates to
			our job controller
			@param ad ClassAd pointer to publish into
			@return true if success, false if failure
		*/ 
	virtual bool publishUpdateAd( ClassAd* ad ) = 0;

		/** Initialize our version of important information for this
			job which the starter will want to know.  This should
			init the following: orig_job_name, job_input_name, 
			job_output_name, job_error_name, and job_iwd.
			@return true on success, false on failure */
	virtual	bool initJobInfo( void ) = 0;

		/** Since we want to support the ATTR_STARTER_WAIT_FOR_DEBUG,
			as soon as we have the job ad, each JIC subclass will want
			to do this work at a different time.  However, since the
			code is the same in all cases, we use this helper in the
			base class to do the work, which looks up the attr in the
			job ad, and if it's defined as true, we go into the
			infinite loop, waiting for someone to attach with a
			debugger.  This also handles printing out the job classad
			to D_JOB if that's in our DebugFlags.
		 */
	virtual void checkForStarterDebugging( void );


		// // // // // // // // // // // //
		// Protected data members
		// // // // // // // // // // // //

		/** The real job executable name (after ATTR_JOB_CMD
			is switched to condor_exec).
		*/
	char* orig_job_name;

	char* job_input_name;

	char* job_output_name;

	char* job_error_name;

	char* job_iwd;
	
		/// The ClassAd for our job.  We control the memory for this.
	ClassAd* job_ad;

		/// if true, we were asked to shutdown
	bool requested_exit;
};


#endif /* _CONDOR_JOB_INFO_COMMUNICATOR_H */
