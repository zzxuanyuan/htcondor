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

#if !defined(_CONDOR_JIC_LOCAL_SCHEDD_H)
#define _CONDOR_JIC_LOCAL_SCHEDD_H

#include "jic_local_file.h"

/** 
	This is the child class of JICLocalFile (and therefore JICLocal
	and JobInfoCommunicator) that deals with running "local universe"
	jobs directly under a condor_schedd.  This JIC gets the job
	ClassAd info from a file (a pipe to STDIN, in fact).  Instead of
	simply reporting everything to a file, it reports info back to the
	schedd via special exit status codes.
*/

class JICLocalSchedd : public JICLocalFile {
public:

		/** Constructor 
			@param classad_filename Full path to the ClassAd, "-" if STDIN
			@param schedd_address Sinful string of the schedd's qmgmt port
			@param cluster Cluster ID number (if any)
			@param proc Proc ID number (if any)
			@param subproc Subproc ID number (if any)
		*/
	JICLocalSchedd( const char* classad_filename,
					const char* schedd_address,
					int cluster, int proc, int subproc );

		/// Destructor
	virtual ~JICLocalSchedd();

	virtual void allJobsGone( void );

		/// The starter has been asked to shutdown fast.
	virtual void gotShutdownFast( void );

		/// The starter has been asked to shutdown gracefully.
	virtual void gotShutdownGraceful( void );

		/// The starter has been asked to evict for condor_rm
	virtual void gotRemove( void );

		/// The starter has been asked to evict for condor_hold
	virtual void gotHold( void );


protected:

		/// This version confirms we're handling a "local" universe job. 
	virtual bool getUniverse( void );

		/// Initialize our local UserLog-writing code.
	virtual bool initLocalUserLog( void );

		/// The value we will exit with to tell our schedd what happened
	int exit_code;

		/// The sinful string of the schedd's qmgmt command port
	char* schedd_addr;

};


#endif /* _CONDOR_JIC_LOCAL_SCHEDD_H */
