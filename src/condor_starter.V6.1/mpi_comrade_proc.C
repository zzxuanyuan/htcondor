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
#include "mpi_comrade_proc.h"
#include "condor_attributes.h"


MPIComradeProc::MPIComradeProc( ClassAd * jobAd ) : VanillaProc( jobAd )
{
    dprintf ( D_FULLDEBUG, "Constructor of MPIComradeProc::MPIComradeProc\n" );
	Node = -1;
}

MPIComradeProc::~MPIComradeProc()  {}


int 
MPIComradeProc::StartJob()
{ 
	dprintf(D_FULLDEBUG,"in MPIComradeProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in MPIComradeProc::StartJob()!\n" ); 
		return 0;
	}
		// Grab ATTR_NODE out of the job ad and stick it in our
		// protected member so we can insert it back on updates, etc. 
	if( JobAd->LookupInteger(ATTR_NODE, Node) != 1 ) {
		dprintf( D_ALWAYS, "ERROR in MPIComradeProc::StartJob(): "
				 "No %s in job ad, aborting!\n", ATTR_NODE );
		return 0;
	} else {
		dprintf( D_FULLDEBUG, "Found %s = %d in job ad\n", ATTR_NODE, 
				 Node ); 
	}

	addEnvVars();
    dprintf(D_PROTOCOL, "#11 - Comrade starting up....\n" );

        // special args already in ad; simply start it up
    return VanillaProc::StartJob();
}


int
MPIComradeProc::addEnvVars() 
{
   dprintf ( D_FULLDEBUG, "MPIComradeProc::addEnvVars()\n" );

	char *env_str = NULL;
	Env env;
	if ( !JobAd->LookupString( ATTR_JOB_ENVIRONMENT, &env_str )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ENVIRONMENT );
		return 0;
	}

	env.Merge(env_str);
	free(env_str);

    char spool[128];
    spool[0] = 0;
	if ( JobAd->LookupString( ATTR_REMOTE_SPOOL_DIR, spool ) < 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_REMOTE_SPOOL_DIR);
		return 0;
	}

    env.Put( "CONDOR_REMOTE_SPOOL_DIR", spool );

	char buf[128];
	sprintf(buf, "%d", Node);
	env.Put("CONDOR_PROCNO", buf);

	int machine_count;
	if ( JobAd->LookupInteger( ATTR_MAX_HOSTS, machine_count ) !=  1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_MAX_HOSTS);
		return 0;
	}

	sprintf(buf, "%d", machine_count);
	env.Put("CONDOR_NPROCS", buf);

        // now put the env back into the JobAd:
	env_str = env.getDelimitedString();
    dprintf ( D_FULLDEBUG, "New env: %s\n", env_str );

	bool assigned = JobAd->Assign( ATTR_JOB_ENVIRONMENT,env_str );
	if(env_str) {
		delete[] env_str;
	}
	if(!assigned) {
		dprintf( D_ALWAYS, "Unable to update env! Aborting.\n" );
		return 0;
	}

	return 1;
}

int
MPIComradeProc::JobCleanup( int pid, int status )
{ 
	return VanillaProc::JobCleanup( pid, status );
}


void 
MPIComradeProc::Suspend() { 
        /* We Comrades don't ever want to be suspended.  We 
           take it as a violation of our basic rights.  Therefore, 
           we walk off the job and notify the shadow immediately! */
	dprintf(D_FULLDEBUG,"in MPIComradeProc::Suspend()\n");
		// must do this so that we exit...
	daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
}


void 
MPIComradeProc::Continue() { 
	dprintf(D_FULLDEBUG,"in MPIComradeProc::Continue() (!)\n");    
        // really should never get here, but just in case.....
    VanillaProc::Continue();
}


bool
MPIComradeProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In MPIComradeProc::PublishUpdateAd()\n" );
	char buf[64];
	sprintf( buf, "%s = %d", ATTR_NODE, Node );
	ad->Insert( buf );

		// Now, call our parent class's version
	return VanillaProc::PublishUpdateAd( ad );
}

