/*
  HAD.cpp : Defines the entry point for the console application.
*/

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_io.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "StateMachine.h"

#if USE_REPLICATION
#   include "Replication.h"
#endif

extern "C" int SetSyscalls(int val){return val;}
extern char* myName;

char *mySubSystem = "HAD";  // for daemon core

#if USE_REPLICATION
    HADReplication* replicator = NULL;
#endif

HADStateMachine* stateMachine = NULL;

int
main_init (int, char *[])
{
    dprintf(D_ALWAYS,"Starting Arbiter ....\n");	
    try {
#if USE_REPLICATION
        replicator = new HADReplication();
        stateMachine = new HADStateMachine(replicator);
#else
        stateMachine = new HADStateMachine();
#endif	


#if USE_REPLICATION
        replicator->initialize();	
#endif

        stateMachine->initialize();
        return TRUE;
    } catch (char* rr) {
        cout << rr << endl;
        dprintf(D_ALWAYS, "Exception in main_init %s \n", rr);
        return FALSE;
    }
}

int
main_shutdown_graceful()
{

    dprintf(D_ALWAYS, "main_shutdown_graceful \n");
    if(stateMachine!=NULL){
        delete  stateMachine;
    }
#if USE_REPLICATION
    if(replicator!=NULL){
        delete replicator;	
    }
#endif
    DC_Exit(0);
    return 0;
}


int
main_shutdown_fast()
{
    if(stateMachine!=NULL){
        delete  stateMachine;
    }
#if USE_REPLICATION
    if(replicator!=NULL){
        delete replicator;	
    }
#endif

    DC_Exit(0);
    return 0;
}

int
main_config( bool is_full )
{
    //Why not reinilize everything even if one of then can't reinilialize?
    bool ret = stateMachine->reinitialize();
#if USE_REPLICATION
    ret = ret && replicator->reinitialize();
#endif
    return ret;
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}
