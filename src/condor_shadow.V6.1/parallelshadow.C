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
#include "parallelshadow.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"         // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.
#include "list.h"                // List class
#include "internet.h"            // sinful->hostname stuff
#include "daemon.h"
#include "env.h"
#include "condor_config.h"       // for 'param()'
#include "condor_uid.h"          // for PRIV_UNKNOWN




void start_gdb(){
  static int gdbflag = TRUE;
  if ( !gdbflag ){
	return;
  }
  gdbflag = FALSE;

  /******** for debug *******/
  char * progname = "/usr/local/condor/sbin/condor_shadow";
  
  int mypid;
  char buffer[100];
  mypid = getpid();
  
  sprintf(buffer, "xterm -display localhost:0.0 -e gdb %s %d ", progname, mypid);
  if (fork() == 0)
	system(buffer);
  else
	sleep(5);
}


ParallelShadow::ParallelShadow() {
    nextResourceToStart = 0;
	numNodes = 0;
    shutDownMode = FALSE;
    ResourceList.fill(NULL);
    ResourceList.truncate(-1);
	actualExitReason = -1;
	info_tid = -1;

	machineFileName[0] = '\0';
	machineFileNameBase[0] = '\0';
}

ParallelShadow::~ParallelShadow() {
        // Walk through list of Remote Resources.  Delete all.
    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
        delete ResourceList[i];
    }
}

void 
ParallelShadow::init( ClassAd* job_ad, const char* schedd_addr )
{
    dprintf ( D_FULLDEBUG, "In ParallelShadow::init()\n" );
	char buf[256];

    if( ! job_ad ) {
        EXCEPT( "No job_ad defined!" );
    }

        // BaseShadow::baseInit - basic init stuff...
    baseInit( job_ad, schedd_addr );

	
        // make first remote resource the "master".  Put it first in list.
    MpiResource *rr = new MpiResource( this );

    ClassAd *temp = new ClassAd( *(getJobAd() ) );

    sprintf( buf, "%s = %s", ATTR_MPI_IS_MASTER, "TRUE" );
    if( !temp->Insert(buf) ) {
        dprintf( D_ALWAYS, "Failed to insert %s into jobAd.\n", buf );
        shutDown( JOB_NOT_STARTED );
    }

	replaceNode( temp, 0 );
	rr->setNode( 0 );
	sprintf( buf, "%s = 0", ATTR_NODE );
	temp->InsertOrUpdate( buf );
    rr->setJobAd( temp );

	rr->setStartdInfo( temp );

    ResourceList[ResourceList.getlast()+1] = rr;
}


void
ParallelShadow::reconnect( void )
{
	EXCEPT( "reconnect is not supported for MPI universe!" );
}


bool 
ParallelShadow::supportsReconnect( void )
{
	return false;
}


void
ParallelShadow::spawn( void )
{
		/*
		  This is parallele.  We should really do a better job of dealing
		  with the multiple ClassAds for MPI universe via the classad
		  file mechanism (pipe to STDIN, usually), instead of this
		  whole mess, and spawn() should really just call
		  "startMaster()".  however, in the race to get disconnected
		  operation working for vanilla, we cut a few corners and
		  leave this as it is.  whenever we're seriously looking at
		  MPI support again, we should fix this, too.
		*/
		/*
		  Finally, register a timer to call getResources(), which
		  sends a command to the schedd to get all the job classads,
		  startd sinful strings, and ClaimIds for all the matches
		  for our computation.  
		  In the future this will just be a backup way to get the
		  info, since the schedd will start to push all this info to
		  our UDP command port.  For now, this is the only way we get
		  the info.
		*/
	info_tid = daemonCore->
		Register_Timer( 1, 0,
						(TimerHandlercpp)&ParallelShadow::getResources,
						"getResources", this );
	if( info_tid < 0 ) {
		EXCEPT( "Can't register DC timer!" );
	}
}


int 
ParallelShadow::getResources( void )
{
    dprintf ( D_FULLDEBUG, "Getting machines from schedd now...\n" );

    char *host = NULL;
    char *claim_id = NULL;
    MpiResource *rr;
	int cluster;
	char buf[128];

    int numProcs=0;    // the # of procs to come
    int numInProc=0;   // the # in a particular proc.
	ClassAd *job_ad = NULL;
	ClassAd *tmp_ad = NULL;
	int nodenum = 1;
	ReliSock* sock;

	cluster = getCluster();
    rr = ResourceList[0];
	rr->getClaimId( claim_id );

		// First, contact the schedd and send the command, the
		// cluster, and the ClaimId
	Daemon my_schedd (DT_SCHEDD, NULL, NULL);

	if(!(sock = (ReliSock*)my_schedd.startCommand(GIVE_MATCHES))) {
		EXCEPT( "Can't connect to schedd at %s", getScheddAddr() );
	}
		
	sock->encode();
	if( ! sock->code(cluster) ) {
		EXCEPT( "Can't send cluster (%d) to schedd\n", cluster );
	}
	if( ! sock->code(claim_id) ) {
		EXCEPT( "Can't send ClaimId to schedd\n" );
	}

		// Now that we sent this, free the memory that was allocated
		// with getClaimId() above
	delete [] claim_id;
	claim_id = NULL;

	if( ! sock->end_of_message() ) {
		EXCEPT( "Can't send EOM to schedd\n" );
	}
	
		// Ok, that's all we need to send, now we can read the data
		// back off the wire
	sock->decode();

        // We first get the number of proc classes we'll be getting.
    if ( !sock->code( numProcs ) ) {
        EXCEPT( "Failed to get number of procs" );
    }

        /* Now, do stuff for each proc: */
    for ( int i=0 ; i<numProcs ; i++ ) {
		job_ad = new ClassAd();
		if( !job_ad->initFromStream(*sock)  ) {
            EXCEPT( "Failed to get job classad for proc %d", i );
		}
        if( !sock->code( numInProc ) ) {
            EXCEPT( "Failed to get number of matches in proc %d", i );
        }

        dprintf ( D_FULLDEBUG, "Got %d matches for proc # %d\n",
				  numInProc, i );

        for ( int j=0 ; j<numInProc ; j++ ) {
            if ( !sock->code( host ) ||
                 !sock->code( claim_id ) ) {
                EXCEPT( "Problem getting resource %d, %d", i, j );
            }
            dprintf( D_FULLDEBUG, "Got host: %s id: %s\n", host, claim_id );
            
            if ( i==0 && j==0 ) {
					/* 
					   TODO: once this is just backup for if the
					   schedd doesn't push it, we need to NOT ignore
					   the first match, since we don't already have
					   it, really.
					*/
                    /* first host passed on command line...we already 
                       have it!  We ignore it here.... */

                free( host );
                free( claim_id );
                host = NULL;
                claim_id = NULL;
                continue;
            }

            rr = new MpiResource( this );
            rr->setStartdInfo( host, claim_id );
 				// for now, set this to the sinful string.  when the
 				// starter spawns, it'll do an RSC to register a real
				// hostname... 
			rr->setMachineName( host );

			tmp_ad = new ClassAd ( *job_ad );
			replaceNode ( tmp_ad, nodenum );
			rr->setNode( nodenum );
			sprintf( buf, "%s = %d", ATTR_NODE, nodenum );
			tmp_ad->InsertOrUpdate( buf );
			sprintf( buf, "%s = \"%s\"", ATTR_MY_ADDRESS,
					 daemonCore->InfoCommandSinfulString() );
			tmp_ad->InsertOrUpdate( buf );
			rr->setJobAd( tmp_ad );
			nodenum++;

            ResourceList[ResourceList.getlast()+1] = rr;

                /* free stuff so next code() works correctly */
            free( host );
            free( claim_id );
            host = NULL;
            claim_id = NULL;

        } // end of for loop for this proc
        
		delete job_ad;
		job_ad = NULL;

    } // end of for loop on all procs...

    sock->end_of_message();

	numNodes = nodenum;  // for later use...
    dprintf ( D_PROTOCOL, "#1 - Shadow started; %d machines gotten.\n", 
			  numNodes );

	start();
    return TRUE;
}

void
ParallelShadow::start(){
    dprintf ( D_FULLDEBUG, "In ParallelShadow::start()\n" );
	// create machine file
	createMachineFile();

	dprintf( D_FULLDEBUG, "spawnning 1-%d comrades\n", numNodes - 1);

	// hack comrade ads
	for (int i = 1; i < numNodes; i++){
	  dprintf ( D_FULLDEBUG, "hack comrade %d:.\n", i );
	  if (!hackComradeAd(ResourceList[i]->getJobAd())){
		dprintf ( D_ALWAYS, "Failed to hack comrade Ad:.\n" );
		shutDown( JOB_NOT_STARTED );
	  }
	}
	// spawn all comrades
	// 
	for (int i = 1; i < numNodes; i++){
	  dprintf( D_FULLDEBUG, "spawnning comrade %d\n", i);
	  spawnNode( ResourceList[i] );
	}
	dprintf( D_FULLDEBUG, "spawnning comrade .. done\n");
	dprintf( D_FULLDEBUG, "now, start master \n");

	startMaster();
}

void
ParallelShadow::startMaster()
{
  // add something to the enviroment
  dprintf ( D_FULLDEBUG, "startMaster. \n" );

  if (!hackMasterAd(ResourceList[0]->getJobAd())){
	dprintf ( D_ALWAYS, "Failed to hack master Ad:.\n" );
	shutDown( JOB_NOT_STARTED );
  }

  // spawn the job 
  spawnNode( ResourceList[0] );

  dprintf ( D_PROTOCOL, "- Just requested Master resource.\n" );
}

#define ATTR_MACHINE_COUNT "machine_count"

int
ParallelShadow::hackComradeAd( ClassAd *JobAd )
{
/*
  add followings as enviroment

  CONDOR_PARALLEL_MACHINE_FILE=machineFileName
  CONDOR_PARALLEL_NO_MACHINES=numNodes
  CONDOR_PARALLEL_SHADOW_CONTACT= ATTR_MY_ADDRESS

  add followings to transfer files
  machineFileName

  add following to the attribute
  
  machine_count = numNodes;
  */



	char *env_str = NULL;
	Env envobject;
	if ( !JobAd->LookupString( ATTR_JOB_ENVIRONMENT, &env_str )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ENVIRONMENT );
		return 0;
	}

	envobject.Merge(env_str);
	free(env_str);
    
    char shad[128];
    shad[0] = 0;
	if ( JobAd->LookupString( ATTR_MY_ADDRESS, shad ) < 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_MY_ADDRESS );
		return 0;
	}
    envobject.Put( "CONDOR_PARALLEL_SHADOW_CONTACT", shad );


	/* set the number of processor as env */
	char npstring[128];
	sprintf(npstring, "%d", numNodes);
	envobject.Put( "CONDOR_PARALLEL_NO_MACHINES",  npstring);

   	/* set machinefile name as env */
	envobject.Put( "CONDOR_PARALLEL_MACHINE_FILE", machineFileNameBase );

        // now put the env back into the JobAd:
	env_str = envobject.getDelimitedString();
    dprintf ( D_FULLDEBUG, "New env: %s\n", env_str );

	bool assigned = JobAd->Assign( ATTR_JOB_ENVIRONMENT,env_str );
	if(env_str) {
		delete[] env_str;
	}
	if(!assigned) {
		dprintf( D_ALWAYS, "Unable to update env! Aborting.\n" );
		return 0;
	}


    char args[2048];
    char tmp[2048];

	dprintf( D_FULLDEBUG, "About to add wantssh attr\n");
	sprintf(tmp, "%s = True", ATTR_WANT_SSHD);
	if ( !JobAd->Insert( tmp )) {
		dprintf( D_ALWAYS, "Unable to insert %s! Aborting.\n",
				 ATTR_WANT_SSHD);
		shutDown( JOB_NOT_STARTED );
	}

	dprintf( D_FULLDEBUG, "About to add machine_count attr\n");
	sprintf(tmp, "%s = %d", ATTR_MACHINE_COUNT, numNodes);
	if ( !JobAd->Insert( tmp )) {
		dprintf( D_ALWAYS, "Unable to insert %s! Aborting.\n",
				 ATTR_MACHINE_COUNT);
		shutDown( JOB_NOT_STARTED );
	}

		// While we're at it, if the job wants files transfered,
		// include the procgroup file in the list of input files.
		// This is only needed on the master.
	if( !JobAd->LookupString(ATTR_TRANSFER_FILES, args) ) {
			// Nothing, we're done.
		return 1;
	}
		// Ok, we found it.  If it's set to anything other than
		// "Never", we need to do our work.
	if( args[0] == 'n' || args[0] == 'N' ) {
			// It's "never", we're done.
		return 1;
	}

		// Now, see if they already gave us a list.
	if( !JobAd->LookupString(ATTR_TRANSFER_INPUT_FILES, args) ) {
			// Nothing here, so we can safely add it ourselves. 
		sprintf( tmp, "%s = \"%s\"",
				 ATTR_TRANSFER_INPUT_FILES,
				 machineFileName
				 );
	} else {
			// There's a list already.  We've got to append to it. 
		sprintf( tmp, "%s = \"%s, %s\"",
				 ATTR_TRANSFER_INPUT_FILES, args,
				 machineFileName
				 );

	}
	dprintf( D_FULLDEBUG, "About to append to job ad: %s\n", tmp );
	if ( !JobAd->Insert( tmp )) {
		dprintf( D_ALWAYS, "Unable to update %s! Aborting.\n",
				 ATTR_TRANSFER_INPUT_FILES );
		shutDown( JOB_NOT_STARTED );
	}



	return 1;
}


int 
ParallelShadow::hackMasterAd( ClassAd *JobAd)
{

	return hackComradeAd(JobAd);
}



void 
ParallelShadow::spawnNode( MpiResource* rr )
{
  dprintf(D_FULLDEBUG, "---  spawning resource jobad    ---\n");
  rr->getJobAd()->dPrint(D_FULLDEBUG);

		// First, contact the startd to spawn the job
  if( rr->activateClaim() != OK ) {
	shutDown( JOB_NOT_STARTED );
  }

  dprintf ( D_PROTOCOL, "Just requested resource for node %d\n",
			nextResourceToStart );

	//	nextResourceToStart++;
}


void 
ParallelShadow::cleanUp( void )
{
  dprintf ( D_FULLDEBUG, "ParallelShadow::cleanUp\n" );

	deleteMachineFile();

	MpiResource *r;
	int i;
    for( i=0 ; i<=ResourceList.getlast() ; i++ ) {
		r = ResourceList[i];
		r->killStarter();
	}		
	BaseShadow::cleanUp();
}


void 
ParallelShadow::gracefulShutDown( void )
{
	cleanUp();
}


void
ParallelShadow::emailTerminateEvent( int exitReason )
{
	int i;
	FILE* mailer;
	mailer = shutDownEmail( exitReason );
	if( ! mailer ) {
			// nothing to do
		return;
	}

	fprintf( mailer, "Your Condor-MPI job %d.%d has completed.\n", 
			 getCluster(), getProc() );

	fprintf( mailer, "\nHere are the machines that ran your MPI job.\n");
	fprintf( mailer, "They are listed in the order they were started\n" );
	fprintf( mailer, "in, which is the same as MPI_Comm_rank.\n\n" );
	
	fprintf( mailer, "    Machine Name               Result\n" );
	fprintf( mailer, " ------------------------    -----------\n" );

	int allexitsone = TRUE;
	int exit_code;
	for ( i=0 ; i<=ResourceList.getlast() ; i++ ) {
		(ResourceList[i])->printExit( mailer );
		exit_code = (ResourceList[i])->exitCode();
		if( exit_code != 1 ) {
			allexitsone = FALSE;
		}
	}

	if ( allexitsone ) {
		fprintf ( mailer, "\nCondor has noticed that all of the " );
		fprintf ( mailer, "processes in this job \nhave an exit status " );
		fprintf ( mailer, "of 1.  This *might* be the result of a core\n");
		fprintf ( mailer, "dump.  Condor can\'t tell for sure - the " );
		fprintf ( mailer, "MPICH library catches\nSIGSEGV and exits" );
		fprintf ( mailer, "with a status of one.\n" );
	}

	fprintf( mailer, "\nHave a nice day.\n" );
	
	email_close(mailer);
}


void 
ParallelShadow::shutDown( int exitReason )
{
		/* With many resources, we have to figure out if all of
		   them are done, and we have to figure out if we need
		   to kill others.... */
	if( !shutDownLogic( exitReason ) ) {
		return;  // leave if we're not *really* ready to shut down.
	}

		/* if we're still here, we can call BaseShadow::shutDown() to
		   do the real work, which is shared among all kinds of
		   shadows.  the shutDown process will call other virtual
		   functions to get universe-specific goodness right. */
	BaseShadow::shutDown( exitReason );
}


int 
ParallelShadow::shutDownLogic( int& exitReason ) {

		/* What sucks for us here is that we know we want to shut 
		   down, but we don't know *why* we are shutting down.
		   We have to look through the set of MpiResources
		   and figure out which have exited, how they exited, 
		   and if we should kill them all... Basically, the only
		   time we *don't* remove everything is when all the 
		   resources have exited normally.  */

  /* in the parallel mpi universe, we will wait for the rank 0 
     node to die. If any other (sshd) node dies, the 
     rank 0 script will die, sooner or later.  Hidemoto*/

	static int realReason = -1;

	//	dprintf( D_FULLDEBUG, "invoke gdb\n");
	// start_gdb();

	dprintf( D_FULLDEBUG, "Entering shutDownLogic(r=%d)\n", 
			 exitReason );

		/* if we have a 'pre-startup' exit reason, we can just
		   dupe that to all resources and exit right away. */
	if ( exitReason == JOB_NOT_STARTED  ||
		 exitReason == JOB_SHADOW_USAGE ) {
		for ( int i=0 ; i<ResourceList.getlast() ; i++ ) {
			(ResourceList[i])->setExitReason( exitReason );
		}
		return TRUE;
	}

		/* Now we know that *something* started... */
	
	int normal_exit = FALSE;

		/* If the job on the resource has exited normally, then
		   we don't want to remove everyone else... */
	if( (exitReason == JOB_EXITED) && !(exitedBySignal()) ) {
		dprintf( D_FULLDEBUG, "Normal exit\n" );
		normal_exit = TRUE;
	}

	/** check the status of rank 0; 
	  if its done, we have to kill all other sshds */
	int status = ResourceList[0]->getResourceState();
	if ( normal_exit ) {
	  realReason = exitReason;	  
	}


	if ( !shutDownMode ) {
	  for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
		ResourceState s = ResourceList[i]->getResourceState();
		if( s == RR_EXECUTING || s == RR_STARTUP ) {
		  char * machineName = NULL;
		  ResourceList[i]->getMachineName( machineName );
		  dprintf( D_FULLDEBUG, "killStarter on %s\n", machineName);
		  ResourceList[i]->killStarter(true);
		}
	  }
	  shutDownMode = TRUE;
	}

	/* We now have to figure out if everyone has finished... */
	int alldone = TRUE;
	MpiResource *r;

    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
		r = ResourceList[i];
		char *res = NULL;
		r->getMachineName( res );
		dprintf( D_FULLDEBUG, "Resource %s...%13s %d\n", res,
				 rrStateToString(r->getResourceState()), 
				 r->getExitReason() );
		delete [] res;
		switch ( r->getResourceState() )
		{
			case RR_PENDING_DEATH:
				alldone = FALSE;  // wait for results to come in, and
			case RR_FINISHED:
				break;            // move on...
			case RR_PRE: {
					// what the heck is going on? - shouldn't happen.
				r->setExitReason( JOB_NOT_STARTED );
				break;
			}
		    case RR_STARTUP:
			case RR_EXECUTING: {
				if ( !normal_exit ) {
					r->killStarter();
				}
				alldone = FALSE;
				break;
			}
			default: {
				dprintf ( D_ALWAYS, "ERROR: Don't know state %d\n", 
						  r->getResourceState() );
			}
		} // switch()
	} // for()



	if ( (!normal_exit) && shutDownMode ) {
		/* We want the exit reason  to be set to the exit
		   reason of the job that caused us to shut down.
		   Therefore, we set this here: */
		exitReason = actualExitReason;
	}

	if ( alldone ) {
			// everyone has reported in their exit status...
		dprintf( D_FULLDEBUG, "All nodes have finished, ready to exit\n" );

		/** set the exit code and exit signal to the job ad.
		    This is not needed for the original mpi universe.
			I donot know why, but this one does not work without 
			this code.  I'm sure that there is something I donot
			understand 			-- Hide
		 */
		ClassAd * masterAd = ResourceList[0]->getJobAd();
		ClassAd * ad = getJobAd();
		char buf [1000];
		int val, res;
		
		res =  masterAd->LookupInteger(ATTR_ON_EXIT_CODE, val);
		if (res){
		  sprintf(buf, "%s=%d", ATTR_ON_EXIT_CODE, val);
		  ad->InsertOrUpdate( buf );
		}

		res = masterAd->LookupInteger(ATTR_ON_EXIT_SIGNAL, val);
		if (res){
		  sprintf(buf, "%s=%d", ATTR_ON_EXIT_SIGNAL, val);
		  ad->InsertOrUpdate( buf );
		}

		if (realReason != -1)
		  exitReason = realReason;
		return TRUE;
	}
	return FALSE;
}

int 
ParallelShadow::handleJobRemoval( int sig ) {
    dprintf ( D_FULLDEBUG, "In handleJobRemoval, sig %d\n", sig );
	remove_requested = true;
 
	ResourceState s;

    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
		s = ResourceList[i]->getResourceState();
		if( s == RR_EXECUTING || s == RR_STARTUP ) {
			ResourceList[i]->setExitReason( JOB_KILLED );
			ResourceList[i]->killStarter();
		}
    }

	return 0;
}

/* This is basically a search-and-replace "#MpInOdE#" with a number 
   for that node...so we can address each mpi node in the submit file. */
void
ParallelShadow::replaceNode ( ClassAd *ad, int nodenum ) {

	ExprTree *tree = NULL, *rhs = NULL, *lhs = NULL;
	char rhstr[ATTRLIST_MAX_EXPRESSION];
	char lhstr[128];
	char final[ATTRLIST_MAX_EXPRESSION];
	char node[9];

	sprintf( node, "%d", nodenum );

	ad->ResetExpr();
	while( (tree = ad->NextExpr()) ) {
		rhstr[0] = '\0';
		lhstr[0] = '\0';
		if( (lhs = tree->LArg()) ) {
			lhs->PrintToStr (lhstr);
		}
		if( (rhs = tree->RArg()) ) {
			rhs->PrintToStr (rhstr);
		}
		if( !lhs || !rhs ) {
			dprintf( D_ALWAYS, "Could not replace $(NODE) in ad!\n" );
			return;
		}

		MyString strRh(rhstr);
		if (strRh.replaceString("#MpInOdE#", node))
		{
			sprintf( final, "%s = %s", lhstr, strRh.Value());
			ad->InsertOrUpdate( final );
			dprintf( D_FULLDEBUG, "Replaced $(NODE), now using: %s\n", 
					 final );
		}
	}	
}


int
ParallelShadow::updateFromStarter(int command, Stream *s)
{
	ClassAd update_ad;
	MpiResource* mpi_res = NULL;
	int mpi_node = -1;
	
	// get info from the starter encapsulated in a ClassAd
	s->decode();
	update_ad.initFromStream(*s);
	s->end_of_message();

		// First, figure out what remote resource this info belongs
		// to. 

	dprintf( D_FULLDEBUG, "in ParallelShadow::updateFromStarter \n");
	update_ad.dPrint(D_FULLDEBUG);


	if( ! update_ad.LookupInteger(ATTR_NODE, mpi_node) ) {
			// No ATTR_NODE in the update ad!
		dprintf( D_ALWAYS, "ERROR in ParallelShadow::updateFromStarter: "
				 "no %s defined in update ad, can't process!\n",
				 ATTR_NODE );
		return FALSE;
	}
	if( ! (mpi_res = findResource(mpi_node)) ) {
		dprintf( D_ALWAYS, "ERROR in ParallelShadow::updateFromStarter: "
				 "can't find remote resource for node %d, "
				 "can't process!\n", mpi_node );
		return FALSE;
	}

		// Now, we're in good shape.  Grab all the info we care about
		// and put it in the appropriate place.
	mpi_res->updateFromStarter( &update_ad );

		// XXX TODO: Do we want to update our local job ad?  Do we
		// want to store the maximum in there?  Seperate stuff for
		// each node?  


	dprintf( D_FULLDEBUG, "Resource %s...%13s\n", mpi_res,
			 rrStateToString(mpi_res->getResourceState()));


	return TRUE;
}


MpiResource*
ParallelShadow::findResource( int node )
{
	MpiResource* mpi_res;
	int i;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		if( node == mpi_res->node() ) {
			return mpi_res;
		}
	}
	return NULL;
}


struct rusage
ParallelShadow::getRUsage( void ) 
{
	MpiResource* mpi_res;
	struct rusage total;
	struct rusage cur;
	int i;
	memset( &total, 0, sizeof(struct rusage) );
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		cur = mpi_res->getRUsage();
		total.ru_stime.tv_sec += cur.ru_stime.tv_sec;
		total.ru_utime.tv_sec += cur.ru_utime.tv_sec;
	}
	return total;
}


int
ParallelShadow::getImageSize( void )
{
	MpiResource* mpi_res;
	int i, max = 0, val;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		val = mpi_res->getImageSize();
		if( val > max ) {
			max = val;
		}
	}
	return max;
}


int
ParallelShadow::getDiskUsage( void )
{
	MpiResource* mpi_res;
	int i, max = 0, val;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		val = mpi_res->getDiskUsage();
		if( val > max ) {
			max = val;
		}
	}
	return max;
}


float
ParallelShadow::bytesSent( void )
{
	MpiResource* mpi_res;
	int i;
	float total = 0;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		total += mpi_res->bytesSent();
	}
	return total;
}


float
ParallelShadow::bytesReceived( void )
{
	MpiResource* mpi_res;
	int i;
	float total = 0;
	for( i=0; i<=ResourceList.getlast() ; i++ ) {
		mpi_res = ResourceList[i];
		total += mpi_res->bytesReceived();
	}
	return total;
}

int
ParallelShadow::getExitReason( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->getExitReason();
	}
	return -1;
}


bool
ParallelShadow::setMpiMasterInfo( char* str )
{
	return false;
}


bool
ParallelShadow::exitedBySignal( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->exitedBySignal();
	}
	return false;
}


int
ParallelShadow::exitSignal( void )
{
  
	if( ResourceList[0] ) {
		return ResourceList[0]->exitSignal();
	}
	return -1;
}


int
ParallelShadow::exitCode( void )
{
	if( ResourceList[0] ) {
		return ResourceList[0]->exitCode();
	}
	return -1;
}


void
ParallelShadow::resourceBeganExecution( RemoteResource* rr )
{
	bool all_executing = true;

	int i;
	for( i=0; i<=ResourceList.getlast() && all_executing ; i++ ) {
		if( ResourceList[i]->getResourceState() != RR_EXECUTING ) {
			all_executing = false;
		}
	}

	if( all_executing ) {
			// All nodes in this computation are now running, so we 
			// can finally log the execute event.
		ExecuteEvent event;
		strcpy( event.executeHost, "MPI_job" );
		if ( !uLog.writeEvent( &event )) {
			dprintf ( D_ALWAYS, "Unable to log EXECUTE event." );
		}
		
			// Now that the whole job is running, start up a few
			// timers we need.
		shadow_user_policy.startTimer();
		
	}
}


void
ParallelShadow::resourceReconnected( RemoteResource* rr )
{
	EXCEPT( "impossible: ParallelShadow doesn't support reconnect" );
}


void
ParallelShadow::logDisconnectedEvent( const char* reason )
{
	EXCEPT( "impossible: ParallelShadow doesn't support reconnect" );
}


void
ParallelShadow::logReconnectedEvent( void )
{
	EXCEPT( "impossible: ParallelShadow doesn't support reconnect" );
}


void
ParallelShadow::logReconnectFailedEvent( const char* reason )
{
	EXCEPT( "impossible: ParallelShadow doesn't support reconnect" );
}




/**********************************************************************/
/**********************************************************************/


void
ParallelShadow::createMachineFile(){
    MpiResource *rr;
    char mach[128];
    char *sinful = new char[128];
    struct sockaddr_in sin;
    FILE *pg;

    sprintf( machineFileNameBase, "machines.%d.%d", getCluster(), getProc() );
    sprintf( machineFileName, "%s/%s", getIwd(), machineFileNameBase );


    if( (pg=fopen( machineFileName, "w" )) == NULL ) {
        dprintf( D_ALWAYS, "Failure to open %s for writing, errno %d\n", 
                 machineFileName, errno );
        shutDown( JOB_NOT_STARTED );
    }
        
        // get the machine name (using string_to_sin and sin_to_hostname)
    rr = ResourceList[0];
    rr->getStartdAddress( sinful );
    string_to_sin( sinful, &sin );
    // rank 0 machine should not be on the list
    //    sprintf( mach, "%s", sin_to_hostname( &sin, NULL ));
    //    fprintf( pg, "%s 0 condor_exec %s\n", mach, getOwner() );

    dprintf ( D_FULLDEBUG, "Machines file:\n" );

        // for each resource, get machine name, make machinefile entry
    for ( int i=0 ; i<=ResourceList.getlast() ; i++ ) {
        rr = ResourceList[i];
        rr->getStartdAddress( sinful );
        string_to_sin( sinful, &sin );
        sprintf( mach, "%s", sin_to_hostname( &sin, NULL ) );
        fprintf( pg, "%s\n", mach);
        dprintf( D_FULLDEBUG, "%s\n", mach);
    }
    delete [] sinful;

        // set permissions on the machines file:
#ifndef WIN32
    if ( fchmod( fileno( pg ), 0666 ) < 0 ) {
        dprintf ( D_ALWAYS, "fchmod failed! errno %d\n", errno );
        fclose( pg );
        shutDown( JOB_NOT_STARTED );
    }
#endif

    if ( fclose( pg ) == EOF ) {
        dprintf ( D_ALWAYS, "fclose failed!  errno = %d\n", errno );
        shutDown( JOB_NOT_STARTED );
    }
}


void 
ParallelShadow::deleteMachineFile(){
  deleteFile(machineFileName);
}

void 
ParallelShadow::deleteFile(char * fileName){
  if (fileName && fileName[0]){
    if( unlink( fileName ) == -1 ) {
	  if( errno != ENOENT ) {
		dprintf( D_ALWAYS, "Problem removing %s: errno %d.\n", 
				 fileName, errno );
	  }
    }
  }
}


