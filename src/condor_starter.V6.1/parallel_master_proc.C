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
#include "parallel_master_proc.h"
#include "condor_attributes.h"
#include "env.h"
#include "condor_string.h"  // for strnewp
#include "my_hostname.h"
#include "starter.h"
#include "sshd_proc.h"

extern CStarter *Starter;

ParallelMasterProc::ParallelMasterProc( ClassAd * jobAd ) : VanillaProc( jobAd )
{
    dprintf ( D_FULLDEBUG, "Constructor of ParallelMasterProc::ParallelMasterProc\n" );
	SshdCheckInit();
}

ParallelMasterProc::~ParallelMasterProc()  {}


int 
ParallelMasterProc::StartJob()
{ 
	dprintf(D_FULLDEBUG,"in ParallelMasterProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in ParallelMasterProc::StartJob()!\n" ); 
		return 0;
	}

	if (!alterEnv() ){
		dprintf ( D_ALWAYS, "Failed to tweak ad!\n" ); 
		return 0;
	}

    dprintf(D_PROTOCOL, "#11 - Comrade starting up....\n" );

        // special args already in ad; simply start it up
    return VanillaProc::StartJob();
}

void 
ParallelMasterProc::Suspend() { 
        /* We Comrades don't ever want to be suspended.  We 
           take it as a violation of our basic rights.  Therefore, 
           we walk off the job and notify the shadow immediately! */
	dprintf(D_FULLDEBUG,"in ParallelMasterProc::Suspend()\n");
		// must do this so that we exit...
	daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
}


bool 
ParallelMasterProc::ShutdownFast() {
  return VanillaProc::ShutdownFast();
}


void 
ParallelMasterProc::Continue() { 
	dprintf(D_FULLDEBUG,"in ParallelMasterProc::Continue() (!)\n");    
        // really should never get here, but just in case.....
    VanillaProc::Continue();
}



int
ParallelMasterProc::alterEnv()
{

  dprintf ( D_FULLDEBUG, "ParallelMasterProc::alterPath()\n" );

  char *rsh_dir = param( "CONDOR_PARALLEL_RSH_DIR" );
  if (rsh_dir == NULL){
	dprintf(D_ALWAYS,"Connot find CONDOR_PARALLEL_RSH_DIR in config\n");
	return FALSE;
  }
  char * openssh_dir  = param( "CONDOR_PARALLEL_OPENSSH_DIR" );
  if (openssh_dir == NULL){
	free (rsh_dir);
	dprintf(D_ALWAYS,"Connot find CONDOR_OPENSSH_DIR in config\n");
	return FALSE;
  }

  const char * work_dir     = Starter->GetWorkingDir();

  // get key name from sshd_proc 

  if ( !Starter->getSshdProc() ){
	dprintf(D_ALWAYS,"no sshdProc. abort\n");
	return FALSE;
  }
  char * baseFileName = ((SshdProc *)(Starter->getSshdProc()))->getKeyBaseName();


/* task:  First, see if there's a PATH var. in the JobAd->env.  
   If there is, alter it.  If there isn't, insert one. */
    
    char *tmp;
	char *env_str = NULL;
	Env envobject;
	if ( !JobAd->LookupString( ATTR_JOB_ENVIRONMENT, &env_str )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ENVIRONMENT );
		return 0;
	}

	envobject.Merge(env_str);
	free(env_str);

	MyString path;
	MyString new_path;

	new_path = rsh_dir;
	new_path += ":";

	if(envobject.getenv("PATH",path)) {
        // The user gave us a path in env.  Find & alter:
        dprintf ( D_FULLDEBUG, "$PATH in ad:%s\n", path.Value() );

		new_path += path;
	}
	else {
        // User did not specify any env, or there is no 'PATH'
        // in env sent along.  We get $PATH and alter it.

        tmp = getenv( "PATH" );
        if ( tmp ) {
            dprintf ( D_FULLDEBUG, "No Path in ad, $PATH in env\n" );
            dprintf ( D_FULLDEBUG, "before: %s\n", tmp );
			new_path += tmp;
        }
        else {   // no PATH in env.  Make one.
            dprintf ( D_FULLDEBUG, "No Path in ad, no $PATH in env\n" );
			new_path = rsh_dir;
        }
    }
	envobject.Put("PATH",new_path.Value());
    
    envobject.Put( "CONDOR_PARALLEL_OPENSSH_DIR",  openssh_dir);
    envobject.Put( "CONDOR_PARALLEL_RSH_DIR",      rsh_dir);
    envobject.Put( "CONDOR_PARALLEL_WORK_DIR",     work_dir);
    envobject.Put( "CONDOR_PARALLEL_USER_KEY",     baseFileName);

        // now put the env back into the JobAd:
	env_str = envobject.getDelimitedString();
    dprintf ( D_FULLDEBUG, "New env: %s\n", env_str );

	free( openssh_dir );

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


#define ATTR_MACHINE_COUNT "machine_count"

/*
  connect to the shadow and ask for the number of sshds that 
  already reported
 */

int 
ParallelMasterProc::CheckSshdCount(ClassAd * jobAd, int & haveEnough) {
    // get proc count
    int count;
    if (jobAd->LookupInteger( ATTR_MACHINE_COUNT, count) < 1) {
	    dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_MACHINE_COUNT );
	    return FALSE;
	}

    // get shadow Address

    char shadow_contact[128];
	shadow_contact[0] = 0;
	if ( jobAd->LookupString( ATTR_MY_ADDRESS, shadow_contact ) < 1 ) {
	    dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_MY_ADDRESS );
  	    return FALSE;
	}

	// connect

	ReliSock * s = new ReliSock;
  
	if ( !s->connect( shadow_contact ) ) {
	   dprintf( D_ALWAYS, "failed to connect %s.\n", shadow_contact);
	   delete s;
	   return FALSE;
    }
  
	s->encode();
  
	int cmd = SSHD_GETNUM;
	if ( !s->code ( cmd ) ||
		 !s->end_of_message()) {
	    dprintf( D_ALWAYS, "failed to send command to get info.\n");
	    delete s;
		return FALSE;
	}

	s->decode();
	int number;
	if ( !s->code(number) ||
		 !s->end_of_message()) { 
	    dprintf( D_ALWAYS, "failed to get number from %s.\n", shadow_contact);
	    delete s;
		return FALSE;
	}


	haveEnough = (number >= count);

	if (haveEnough) 
	    dprintf( D_ALWAYS, "got enough sshds spawn the script .\n");
	else
	    dprintf( D_ALWAYS, "could not have enough sshds, may be retry .\n");
	return TRUE;
}

void
ParallelMasterProc::SshdCheckInit(){
    // get the interval
    sshdCheckInterval = 20;
	char * sshdCheckIntervalString = param("PARALLEL_SSHD_CHECK_INTERVAL");
	if (sshdCheckIntervalString != NULL){
	    sshdCheckInterval = atoi(sshdCheckIntervalString);
	    free(sshdCheckIntervalString);
	}
	sshdCheckCounter = 0;

    // get the interval
    sshdCheckMax = 20;
	char * sshdCheckMaxString = param("PARALLEL_SSHD_CHECK_MAX");
	if (sshdCheckMaxString!= NULL){
	    sshdCheckMax = atoi(sshdCheckMaxString);
	    free(sshdCheckMaxString);
	}
	sshd_check_tid = -1;
}

int
ParallelMasterProc::SpawnParallelMaster(){

	// check if we have enough sshd
	int haveEnough;
	extern int main_shutdown_fast();

	if (! CheckSshdCount(JobAd, haveEnough)){
	    dprintf( D_ALWAYS, "failed to connct and get info from shadow, give up\n");
		main_shutdown_fast();
	}
	if (!haveEnough){
	    // we donot have enough, register this routine as a timer proc
        // and return True;

	    if ( sshdCheckCounter++ > sshdCheckMax ) {
		    // 
		    //  cancel the timer, if needed
  		    if (sshd_check_tid != -1){
			    daemonCore->Cancel_Timer(sshd_check_tid);
				sshd_check_tid = -1;
			}
			dprintf( D_ALWAYS, "sshd servers are not propery spawned, give up\n");
			main_shutdown_fast();
		}

	    // register the timer, if not registered
		if (sshd_check_tid == -1){
		    sshd_check_tid = 
			  daemonCore->Register_Timer(10, sshdCheckInterval,
										 (TimerHandlercpp)&ParallelMasterProc::SpawnParallelMaster,
										 "ParallelMasterProc::SpawnParallelMaster",
										 this);
 	    }
		return TRUE;
	}
	if (sshd_check_tid != -1){
	  	daemonCore->Cancel_Timer(sshd_check_tid);
		sshd_check_tid = -1;
	}

	// OK, invoke the job
	UserProc *job = new ParallelMasterProc( JobAd );

	if (job->StartJob()) {
	    Starter->appendJobList(job);	
		Starter->SpawnToolDaemon(job, JobAd);	

		Starter->jic->allJobsSpawned();
	} else {
	    dprintf( D_ALWAYS, "Failed to start job, exiting\n" );
		main_shutdown_fast();
		return FALSE;
	}
	return TRUE;
		  
}
