// StateMachine.cpp: implementation of the HADStateMachine class.
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
#include "list.h"

#include "StateMachine.h"

#if USE_REPLICATION
    #include "Replication.h"
#endif

extern int main_shutdown_graceful();

/***********************************************************
*  Function :
*/
HADStateMachine::HADStateMachine(){
    init();
}

#if USE_REPLICATION
HADStateMachine::HADStateMachine(HADReplication* replic){
    init();
    state = PRE_STATE;
    replicator = replic;
}
#endif

void HADStateMachine::init(){
    state = PASSIVE_STATE;
    otherHADIPs = NULL;
    masterDaemon = NULL;
    isPrimary = false;
    debugMode = false;
    replicator = NULL;
    firstTime = true;
    hadTimerID = -1;
    hadInterval = NEGOTIATION_CYCLE;
    selfId = -1;
}
/***********************************************************
*  Function :
*/	
void HADStateMachine::finilize()
{

    state = PASSIVE_STATE;

    if(otherHADIPs != NULL){
        delete otherHADIPs;
		otherHADIPs = NULL;
	}
    if(masterDaemon != NULL){
        // always kill leader when HAD dies - if I am leader I should kill my Negotiator,
        // If I am not a leader - my Negotiator should not be on anyway
        sendNegotiatorCmdToMaster(DAEMON_OFF);

        delete masterDaemon;
		masterDaemon = NULL;
	}
    isPrimary = false;
    debugMode = false;
    firstTime = true;
	if ( hadTimerID >= 0 ) {
		daemonCore->Cancel_Timer(hadTimerID);
		hadTimerID = -1;
	}
    selfId = -1;

	clearBuffers();


	// I Do not delete replicator, cause StateMachine did not allocated it.
	// I only delete what I have allocated in this class.
}

HADStateMachine::~HADStateMachine()
{
	finilize();
}


/***********************************************************
*  Function :
*/
void HADStateMachine::initialize(){
   reinitialize ();

    // not sure that we need it
   //daemonCore->Register_Command (RESCHEDULE, "Reschedule",
    //        (CommandHandlercpp) &HADStateMachine::RESCHEDULE_commandHandler,
	//		"RESCHEDULE_commandHandler", (Service*) this, DAEMON);

#ifdef WANT_NETMAN
   // TODO - we don't know what it does
   //daemonCore->Register_Command (REQUEST_NETWORK, "RequestNetwork",
   //                               (CommandHandlercpp) &HADStateMachine::REQUEST_NETWORK_commandHandler,
   //                              "REQUEST_NETWORK_commandHandler", this, WRITE);
#endif

   // Set a timer to check HAD cycle.
   //hadTimerID = daemonCore->Register_Timer (0 ,hadInterval,(TimerHandlercpp) &HADStateMachine::step,
   //                                 "Time to check HAD", this);

}

void HADStateMachine::onError(char* error_msg)
{
    dprintf(D_ALWAYS,"%s\n",error_msg);
    main_shutdown_graceful();
}

/***********************************************************
*  Function : reinitialize - 
*	delete all previuosly alocated memory, read all config params from config_file again
*	and init all relevant parameters
*
*	Checking configurations parameters:
*	-----------------------------------
*
*	HAD_ID is given.
*	HAD_NET_LIST is given and all addresses in it are Condor sinfull adresses (<IP:port>)
*	HAD_CYCLE_INTERVAL is given
*
*	Daemon command port and address matches exactly one port and address in HAD_NET_LIST 
*	More checks?
*
*	In case of any of this errors we should exit with error.
*/
int HADStateMachine::reinitialize(){
	char* tmp;

	
	// DELETE all and start everything over from the scratch
	finilize();
	
    masterDaemon = new Daemon( DT_MASTER );

	tmp = param("HAD_STAND_ALONE_DEBUG");	
	if(tmp){
		debugMode = true;
        free( tmp );
	}

	tmp=param("HAD_ID");
	if (tmp)
	{
        // selfId = atoi(tmp);
        char* endp;
        selfId = (int)strtol(tmp, &endp, 10);
        if (!(endp && *endp == '\0')) {
               /* Entire string wasn't valid */
               free(tmp);
               onError("HAD CONFIGURATION ERROR: HAD_ID is not valid in config file");
        }
		free(tmp);
	}
	else
	{
        onError("HAD CONFIGURATION ERROR: no HAD_ID in config file");
        selfId = 0;
	}

	tmp=param("HAD_CYCLE_INTERVAL");
	if( tmp )
	{

        // hadInterval = atoi(tmp);
        char* endp;
        hadInterval = (int)strtol(tmp, &endp, 10);
        if (!(endp && *endp == '\0')) {
               /* Entire string wasn't valid */
               free(tmp);
               onError("HAD CONFIGURATION ERROR: HAD_CYCLE_INTERVAL is not valid in config file");
        }
		free( tmp );

	} else {
		hadInterval = NEGOTIATION_CYCLE;
        onError("HAD CONFIGURATION ERROR: no HAD_CYCLE_INTERVAL in config file ");
    }

    hadTimerID = daemonCore->Register_Timer (0 ,hadInterval,(TimerHandlercpp) &HADStateMachine::step,
                                    "Time to check HAD", this);

    otherHADIPs = new StringList();
	tmp=param("HAD_NET_LIST");
	if (tmp)
	{
		initializeHADList(tmp);
		free(tmp);
	} else {
        onError("HAD CONFIGURATION ERROR: no HAD_NET_LIST in config file");
    }

    tmp=param("HAD_PRIMARY_ID");
	if( tmp )
	{
		// int primaryId = atoi(tmp);
        char* endp;
        int primaryId = (int)strtol(tmp, &endp, 10);
        if (!(endp && *endp == '\0')) {
               /* Entire string wasn't valid */
               free(tmp);
               onError("HAD CONFIGURATION ERROR: HAD_PRIMARY_ID is not valid in config file");
        }else{
               if(primaryId == selfId)
                    isPrimary = true;
        }

		free( tmp );
	}

    // check if SEND_COMMAND_TIMEOUT*NUM_of backups > HAD_INTERVAL-safetyFactor.
    // let safetyFactor be 1 sec
    int safetyFactor = 1;
    if((SEND_COMMAND_TIMEOUT*2+1)*otherHADIPs->number() > hadInterval-safetyFactor){
        // configuration error - can't succeed to send all commands
        // like "I'm alive" during one hadInterval
        onError("HAD CONFIGURATION ERROR: SEND_COMMAND_TIMEOUT*NUM_of backups > HAD_INTERVAL-SafetyFactor");
    }

    if(debugMode) {
         return TRUE;
    }

    if(masterDaemon == NULL || sendNegotiatorCmdToMaster(DAEMON_OFF) == FALSE) {
         onError("HAD ERROR: unable to send NEGOTIATOR_OFF command");
    }
	return TRUE;
}

/***********************************************************
*  Function :
*/	
void  HADStateMachine::step(){
	dprintf( D_FULLDEBUG, "\n--->id %d - step function, state %d\n",daemonCore->getpid(),state);
    my_debug_print_buffers();

	switch(state){
        case PRE_STATE:
#if USE_REPLICATION
            if(replicator->isInitializedFile()){
                  state = PASSIVE_STATE;
                  printStep("PRE_STATE","PASSIVE_STATE");
                  break;
            }else{
                // return if we are still initializing negotiator file
                if(firstTime == false){
                    break;
                }

                my_debug_print_list(otherHADIPs);

                firstTime = false;
                replicator->initializeFile(otherHADIPs,hadInterval);
                break;
            }
#endif

		case PASSIVE_STATE:
            if(receivedAliveList.IsEmpty() || isPrimary){
                state = ELECTION_STATE;
                printStep("PASSIVE_STATE","ELECTION_STATE");
                sendCommandToOthers(HAD_SEND_ID_CMD);
                // we don't want to delete elections buffers
                return;
            } else{
                printStep("PASSIVE_STATE","PASSIVE_STATE");
            }

			break;
		case ELECTION_STATE:
            if(!receivedAliveList.IsEmpty() && !isPrimary){
                state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

            // command ALIVE isn't received
            if(checkList(&receivedIdList) == FALSE){
                // id bigger than selfId is received
                state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

            // if stand alone mode
            if(debugMode){
                state = LEADER_STATE;
                printStep("ELECTION_STATE","LEADER_STATE");
                sendCommandToOthers(HAD_ALIVE_CMD);
                break;
            }

            // no leader in the system and this HAD has biggest id
            if(sendNegotiatorCmdToMaster(DAEMON_ON) == TRUE){
                state = LEADER_STATE;
                printStep("ELECTION_STATE","LEADER_STATE");
                sendCommandToOthers(HAD_ALIVE_CMD);
            }else{
                // TO DO : what with this case ? stay in election case ? return to passive ?
                // may be call sendNegotiatorCmdToMaster(DAEMON_ON) in a loop ?
                dprintf(D_FULLDEBUG,"id %d , cannot send NEGOTIATOR_ON cmd, stay in ELECTION state\n",daemonCore->getpid());
                onError("");
            }

            break;

		case LEADER_STATE:

            if(!receivedAliveList.IsEmpty()){
                if(checkList(&receivedAliveList) == FALSE){
                    // send to master "negotiator_off"
                    printStep("LEADER_STATE","PASSIVE_STATE");
                    state = PASSIVE_STATE;
                    if(!debugMode){
                        sendNegotiatorCmdToMaster(DAEMON_OFF);
                    }
                    break;
                }
            }
            printStep("LEADER_STATE","LEADER_STATE");
            sendCommandToOthers(HAD_ALIVE_CMD);
			break;
	}
    clearBuffers();
}


/***********************************************************
*  Function :
*/
// TODO: Why do we need that function ?
/*
int HADStateMachine::RESCHEDULE_commandHandler(int, Stream *strm){
    dprintf( D_ALWAYS, "id %d , RESCHEDULE_commandHandler   \n",daemonCore->getpid());
	return TRUE;
}                               */

/***********************************************************
*  Function :
*/
int HADStateMachine::sendCommandToOthers(int comm){

	char* addr;
    //my_debug_print_list(otherHADIPs);

    otherHADIPs->rewind();
	while( (addr = otherHADIPs->next()) ) {
        dprintf( D_FULLDEBUG, "id %d , send %d command to %s\n",daemonCore->getpid(),comm,addr);
	
	    Daemon d( DT_ANY, addr );
        ReliSock sock;
        // GABI TODO: make sock nonblocking
        sock.timeout(SEND_COMMAND_TIMEOUT);
	
	    if(!sock.connect(addr,0,true)){
		    dprintf(D_ALWAYS,"id %d , cannot connect to addr\n",daemonCore->getpid(),addr);
            sock.close();
		    continue;
	    }

	    int cmd = comm;
        // startCommand - max timeout is 1 sec
        if(!d.startCommand(cmd,&sock,SEND_COMMAND_TIMEOUT )) {
            dprintf(D_ALWAYS,"id %d , cannot start command\n",daemonCore->getpid());
            sock.close();
		    continue;
        }

	    char stringId[256];
		sprintf(stringId,"%d",selfId);

		char* subsys = (char*)stringId;

	    if(!sock.code(subsys) || !sock.eom()){
		    dprintf(D_ALWAYS,"id %d , sock.code false \n",daemonCore->getpid());
	    }else{
            dprintf(D_FULLDEBUG,"id %d , sock.code true \n",daemonCore->getpid());
	    }

        sock.close();
	}

	return TRUE;
}

/***********************************************************
*  Function :
*/
int HADStateMachine::sendNegotiatorCmdToMaster(int comm){

	//Daemon d( DT_MASTER );
	ReliSock sock;
	// GABI TODO: make sure sock is non blocking
    // if timeout() return value is -1, ...
    sock.timeout(SEND_COMMAND_TIMEOUT);
	

	if(!sock.connect(masterDaemon->addr(),0,true)){
		dprintf(D_ALWAYS,"id %d , cannot connect to master , addr %s\n",daemonCore->getpid(),masterDaemon->addr());
        sock.close();
		return FALSE;
	}

	int cmd = comm;
	dprintf(D_FULLDEBUG,"id %d ,send command %d (DAEMON_OFF = 467,DAEMON_ON = 469) to master\n",daemonCore->getpid(),cmd);
    dprintf(D_FULLDEBUG,"id %d ,master address is %s\n",daemonCore->getpid(),masterDaemon->addr());

    // startCommand - max timeout is 1 sec
    if(!(masterDaemon->startCommand(cmd,&sock,SEND_COMMAND_TIMEOUT ))){
        dprintf(D_ALWAYS,"id %d , cannot start command (master) , addr %s\n",daemonCore->getpid());
        sock.close();
		return FALSE;
    }

	char* subsys = (char*)daemonString(DT_NEGOTIATOR);	

	if(!sock.code(subsys) || !sock.eom()){
		dprintf(D_ALWAYS,"id %d send to master , !sock.code false \n",daemonCore->getpid());
        sock.close();
		return FALSE;
	}else{
        dprintf(D_FULLDEBUG,"id %d send to master , !sock.code true \n",daemonCore->getpid());
	}

  sock.close();
  return TRUE;
}

/***********************************************************
*  Function :
*/
int HADStateMachine::pushReceivedAlive(int id){

         int* alloc_id = new int[1];
         *alloc_id = id;
         return (receivedAliveList.Append(alloc_id));
}

/***********************************************************
*  Function :
*/
int HADStateMachine::pushReceivedId(int id){

         int* alloc_id = new int[1];
         *alloc_id = id;
         return (receivedIdList.Append(alloc_id));
}


/***********************************************************
*  Function :
*/
void HADStateMachine::initializeHADList(char* str){
	StringList had_net_list;

    //   initializeFromString() and rewind() return void
	had_net_list.initializeFromString(str);

	char* try_address;
	had_net_list.rewind();

    bool iAmPresent = false;
    while( (try_address = had_net_list.next()) ) {
      /*
        struct sockaddr_in sin;
        if(!isValidAddress(try_address,&sin)){
            char error_msg[256];
            sprintf( error_msg, "HAD CONFIGURATION ERROR: %d , not valid sinful address %s",daemonCore->getpid(),try_address);
            onError(error_msg);
            continue;
        }

        dprintf(D_FULLDEBUG," initializeHADList try_address %s port %d\n",inet_ntoa(sin.sin_addr),sin.sin_port);
        // check if not myself
        if(! isMyAddress(sin) ){
            char* ip_addr = strdup(inet_ntoa(sin.sin_addr));
            char full_addr[256];
            sprintf(full_addr,"<%s:%d>",ip_addr,sin.sin_port);

            otherHADIPs->insert(full_addr);
            dprintf(D_FULLDEBUG," initializeHADList insert %s\n",full_addr);
            delete ip_addr;
        }else{
            iAmPresent = true;
        }
	}*/

      char* sinfull_addr = convertToSinfull(try_address);
      if(sinfull_addr == NULL){
	    char error_msg[256];
            sprintf( error_msg, "HAD CONFIGURATION ERROR: %d , not valid address %s",daemonCore->getpid(),try_address);
            onError(error_msg);
            continue;
      }
      if(strcmp(sinfull_addr,daemonCore->InfoCommandSinfulString()) == 0){
	iAmPresent = true;
      }else{
	otherHADIPs->insert(sinfull_addr);
	dprintf(D_FULLDEBUG," initializeHADList insert %s\n",sinfull_addr);
      }
      delete( sinfull_addr );
    }

    if(!iAmPresent){
        onError("HAD CONFIGURATION ERROR : my address is not present in HAD_NET_LIST");
    }
}


/***********************************************************
*  Function :
*/
int HADStateMachine::checkList(List<int>* list){
    int id;

	list->Rewind();

    while(list->Next(id) ) {
		if(id > selfId)
            return FALSE;
	}
    return TRUE;
}

/***********************************************************
*  Function :
*/
void HADStateMachine::removeAllFromList(List<int>* list){
    int* elem;
	list->Rewind();
	while((elem = list->Next()) ) {
        delete elem;
		list->DeleteCurrent();
	}
    assert(list->IsEmpty());

}

/***********************************************************
*  Function :
*/
void HADStateMachine::clearBuffers(){
   removeAllFromList(&receivedAliveList);
   removeAllFromList(&receivedIdList);
}

/***********************************************************
*  Function :
*//*
bool HADStateMachine::isValidAddress(char* addr,struct sockaddr_in* sockad){
    char* str;
    struct sockaddr_in sin;

    if(!(addr[0] == '<') ){
         str = (char*)malloc(strlen(addr)+3);
         sprintf(str,"<%s>",addr);
         if(string_to_sin( str, &sin ) == 0){
            free( str );
            return FALSE;
         }

         dprintf(D_FULLDEBUG," isValidAddress address %s sin_address %s port %d\n",addr,inet_ntoa(sin.sin_addr),sin.sin_port);

         free( str );
         *sockad = sin;
	 sockad->sin_port = getPortFromAddr(addr);
         return TRUE;
    }else{
      
        if(string_to_sin( addr, &sin ) == 0)
            return FALSE;
        *sockad = sin;
	sockad->sin_port = getPortFromAddr(addr);
	dprintf(D_FULLDEBUG," isValidAddress address2 %s sin_address %s port %d\n",addr,inet_ntoa(sockad->sin_addr),sockad->sin_port);
        return TRUE;
    }


    }*/

/***********************************************************
*  Function :
*/
char* HADStateMachine::convertToSinfull(char* addr){

    int port = getPortFromAddr(addr);
    if(port == 0)
      return NULL;

    char* address = getHostFromAddr(addr);
    if(address == 0)
      return NULL;

    struct in_addr sin;
    if(!is_ipaddr(address, &sin)){
      struct hostent *ent = gethostbyname(address);
      if(ent == NULL){
	free( address );
	return NULL;
      }
      char* ip_addr = inet_ntoa(*((struct in_addr *)ent->h_addr));
      free( address );
      address = strdup(ip_addr);
    }

    char port_str[10];
    sprintf(port_str,"%d",port);
    char* result = (char*)malloc(strlen(address) + strlen(port_str) + 2*strlen("<") + strlen(":") + 1);
    sprintf(result,"<%s:%d>",address,port);

    free( address );
    return result;

}
/***********************************************************
*  Function :
*//*
bool HADStateMachine::isMyAddress(struct sockaddr_in other_sin){

    struct sockaddr_in my_sin;
    string_to_sin( daemonCore->InfoCommandSinfulString(), &my_sin );

    // compare addresses
    // if( (my_sin.sin_addr.s_addr == other_sin.sin_addr.s_addr) && (my_sin.sin_port == other_sin.sin_port)){
    //    return TRUE;
    // }
    // compare addresses
    if( (my_sin.sin_addr.s_addr == other_sin.sin_addr.s_addr) && (daemonCore->InfoCommandPort() == other_sin.sin_port)){
        return TRUE;
    }
    return FALSE;

}
  */
/***********************************************************
*  Function :
*/
void HADStateMachine::printStep(char *curState,char *nextState){
      dprintf( D_FULLDEBUG, "State mashine step :id <%d> port <%d> priority <%d> was <%s> go to <%s>\n",daemonCore->getpid(),daemonCore->InfoCommandPort(),selfId,curState,nextState);
}

/***********************************************************
*  Function :
*/
void HADStateMachine::my_debug_print_list(StringList* str){
	str->rewind();
	char* elem;
	dprintf( D_FULLDEBUG, "SV ----> begin print list , id %d\n",daemonCore->getpid());
	while( (elem = str->next()) ) {
		dprintf( D_FULLDEBUG, "SV ----> %s\n",elem);
	}
	dprintf( D_FULLDEBUG, "SV ----> end print list \n");
}

/***********************************************************
*  Function :
*/
void HADStateMachine::my_debug_print_buffers(){
    int id;
    dprintf( D_FULLDEBUG, "ALIVE IDs list : \n");
    receivedAliveList.Rewind();
    while( receivedAliveList.Next(id) ) {
		      dprintf( D_FULLDEBUG, "<%d>\n",id);
	}

    int id2;
    dprintf( D_FULLDEBUG, "ELECTION IDs list : \n");
    receivedIdList.Rewind();
    while( receivedIdList.Next(id2) ) {
		      dprintf( D_FULLDEBUG, "<%d>\n",id2);
	}

}



