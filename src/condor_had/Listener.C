// Listener.cpp: implementation of the HADListener class.
//
//////////////////////////////////////////////////////////////////////

#include "condor_common.h"
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_api.h"
#include "condor_classad_lookup.h"
#include "condor_query.h"
#include "daemon.h"
#include "daemon_types.h"
#include "internet.h"

#include "Listener.h"
#include "StateMachine.h"



/***********************************************************
*  Function :
*/
HADListener::HADListener(HADStateMachine* sm){

    StateMachine = sm;

}

/***********************************************************
*  Function :
*/	
HADListener::~HADListener(){

}

/***********************************************************
*  Function :
*/
void HADListener::initialize(){
   reinitialize();

   daemonCore->Register_Command (HAD_ALIVE_CMD, "ALIVE command",
            		(CommandHandlercpp) &HADListener::commandHandler,
			          "commandHandler", (Service*) this, DAEMON);

   daemonCore->Register_Command (HAD_SEND_ID_CMD, "SEND ID command",
            		(CommandHandlercpp) &HADListener::commandHandler,
			          "commandHandler", (Service*) this, DAEMON);
}

/***********************************************************
*  Function :
*/
int HADListener::reinitialize(){
    return TRUE;
}

/***********************************************************
*  Function :
*/
void HADListener::commandHandler(int cmd,Stream *strm){
    dprintf( D_FULLDEBUG, "id %d Listener - commandHandler cmd : %d\n",daemonCore->getpid(),cmd);

    char* subsys = NULL;

	strm->decode();
	if( ! strm->code(subsys) ) {
		dprintf( D_ALWAYS, "id %d Listener - commandHandler - Can't read subsystem name\n" );
        return;
	}
	if( ! strm->end_of_message() ) {
		dprintf( D_ALWAYS, "id %d Listener - commandHandler - Can't read end_of_message\n" );
		free( subsys );
        return;
	}

    //int new_id = atoi(subsys);
    char* endp;
    int new_id = (int)strtol(subsys, &endp, 10);
    if (!(endp && *endp == '\0')) {
               /* Entire string wasn't valid */
               free(subsys);
               dprintf(D_ALWAYS,"id %d Listener - commandHandler received invalid id %s\n",daemonCore->getpid(),subsys);
               return;
    }

	free( subsys );
    dprintf( D_FULLDEBUG, "id %d Listener - commandHandler received id : %d\n",daemonCore->getpid(),new_id);

    switch(cmd){
        case HAD_ALIVE_CMD:
            dprintf( D_FULLDEBUG, "id %d Listener - commandHandler received ALIVE\n",daemonCore->getpid());
            StateMachine->pushReceivedAlive(new_id);
            break;

        case HAD_SEND_ID_CMD:
            dprintf( D_FULLDEBUG, "id %d Listener - commandHandler received ID_CMD\n",daemonCore->getpid());
            StateMachine->pushReceivedId(new_id);
            break;
    }


}













