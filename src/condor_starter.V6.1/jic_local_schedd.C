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
#include "condor_string.h"
#include "condor_attributes.h"
#include "exit.h"

#include "starter.h"
#include "jic_local.h"
#include "jic_local_schedd.h"

extern CStarter *Starter;


JICLocalSchedd::JICLocalSchedd( const char* classad_filename, 
								int cluster, int proc, int subproc )
	: JICLocalFile( classad_filename, cluster, proc, subproc )
{
		// initialize this to something reasonable.  we'll change it
		// if anything special happens which needs a different value.
	exit_code = JOB_EXITED;
}



JICLocalSchedd::~JICLocalSchedd()
{
}


void
JICLocalSchedd::allJobsGone( void )
{
		// Since there's no shadow to tell us to go away, we have to
		// exit ourselves.  However, we need to use the right code so
		// the schedd knows to remove the job from the queue
	dprintf( D_ALWAYS, "All jobs have exited... starter exiting\n" );
	Starter->StarterExit( exit_code );
}


void
JICLocalSchedd::gotShutdownFast( void )
{
		// Set our flag so we know we were asked to vacate.
	requested_exit = true;
	exit_code = JOB_SHOULD_REQUEUE;

}


void
JICLocalSchedd::gotShutdownGraceful( void )
{
		// Set our flag so we know we were asked to vacate.
	requested_exit = true;
	exit_code = JOB_SHOULD_REQUEUE;
}


void
JICLocalSchedd::gotRemove( void )
{
		// Set our flag so we know we were asked to vacate.
	requested_exit = true;
	exit_code = JOB_KILLED;
}


void
JICLocalSchedd::gotHold( void )
{
		// Set our flag so we know we were asked to vacate.
	requested_exit = true;
	exit_code = JOB_KILLED;
}


bool
JICLocalSchedd::getUniverse( void )
{
	int univ;

		// first, see if we've already got it in our ad
	if( ! job_ad->LookupInteger(ATTR_JOB_UNIVERSE, univ) ) {
		dprintf( D_ALWAYS, "\"%s\" not found in job ClassAd\n", 
				 ATTR_JOB_UNIVERSE );
		return false;
	}

	if( univ != CONDOR_UNIVERSE_LOCAL ) {
		dprintf( D_ALWAYS,
				 "ERROR: %s %s (%d) is not supported by JICLocalSchedd\n", 
				 ATTR_JOB_UNIVERSE, CondorUniverseName(univ), univ );
		return false;
	}
	
	return true;
}


bool
JICLocalSchedd::initLocalUserLog( void )
{
	return u_log->initFromJobAd( job_ad, ATTR_ULOG_FILE,
								 ATTR_ULOG_USE_XML );
}


