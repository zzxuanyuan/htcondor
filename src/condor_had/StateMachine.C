/*
  StateMachine.cpp: implementation of the HADStateMachine class.
*/

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
#   include "Replication.h"
#endif

extern int main_shutdown_graceful();

const char* commandsHADArray[5] = {"HAD_ALIVE_CMD",
                            "HAD_SEND_ID_CMD",
                            "HAD_REPL_FILE_VERSION",
                            "HAD_REPL_JOIN",
                            "HAD_REPL_TRANSFER_FILE"};

const char* commandsMasterArray[3] = {"DAEMON_OFF",
                            "",
                            "DAEMON_ON"};                           
                            
/***********************************************************
  Function :
*/
HADStateMachine::HADStateMachine()
{
    //dprintf( D_ALWAYS,"Debug : in constructor\n");
    init();
#if USE_REPLICATION
    replicator = new HADReplication();
#endif

}

/***********************************************************
  Function :
*/
void
HADStateMachine::init()
{
    state = PRE_STATE;
    otherHADIPs = NULL;
    masterDaemon = NULL;
    replicator = NULL;
    isPrimary = false;
    debugMode = false;
    replicator = NULL;
    firstTime = true;
    callsCounter = 0;
    stateMachineTimerID = -1;
    //sendMsgTimerID = -1;
    replicaTimerID = -1;
    waitingVersionsTimerID = -1;
    hadInterval = NEGOTIATION_CYCLE;
    replicationInterval = REPLICATION_CYCLE;
    selfId = -1;
}
/***********************************************************
  Function :
*/	
void
HADStateMachine::finilize()
{
    state = PRE_STATE;

    if( otherHADIPs != NULL ){
        delete otherHADIPs;
        otherHADIPs = NULL;
    }
    if( masterDaemon != NULL ){
        // always kill leader when HAD dies - if I am leader I should kill my Negotiator,
        // If I am not a leader - my Negotiator should not be on anyway
        sendNegotiatorCmdToMaster( DAEMON_OFF );
        delete masterDaemon;
        masterDaemon = NULL;
    }
    isPrimary = false;
    debugMode = false;
    firstTime = true;
    callsCounter = 0;
    if ( stateMachineTimerID >= 0 ) {
        daemonCore->Cancel_Timer( stateMachineTimerID );
        stateMachineTimerID = -1;
    }
    //if ( sendMsgTimerID >= 0 ) {
    //    daemonCore->Cancel_Timer( sendMsgTimerID );
    //    sendMsgTimerID = -1;
    //}
    selfId = -1;

#if USE_REPLICATION
    if(replicator != NULL) {
         delete replicator;     
    }
    if ( replicaTimerID >= 0 ) {
        daemonCore->Cancel_Timer( replicaTimerID );
        replicaTimerID = -1;
    }
    if ( waitingVersionsTimerID >= 0 ) {
        daemonCore->Cancel_Timer( waitingVersionsTimerID );
        waitingVersionsTimerID = -1;
    }
#endif
    
    clearBuffers();

    
}

/***********************************************************
  Function :
*/
HADStateMachine::~HADStateMachine()
{
    finilize();
}


/***********************************************************
  Function :
*/
void
HADStateMachine::initialize()
{
    reinitialize ();
    daemonCore->Register_Command ( HAD_ALIVE_CMD, "ALIVE command",
            (CommandHandlercpp) &HADStateMachine::commandHandler,
            "commandHandler", (Service*) this, DAEMON );

    daemonCore->Register_Command ( HAD_SEND_ID_CMD, "SEND ID command",
            (CommandHandlercpp) &HADStateMachine::commandHandler,
            "commandHandler", (Service*) this, DAEMON );

#if USE_REPLICATION
    daemonCore->Register_Command (HAD_REPL_FILE_VERSION, "replication local file version",
            (CommandHandlercpp) &HADReplication::commandHandler,
            "commandHandler", (Service*) this, DAEMON);


    daemonCore->Register_Command (HAD_REPL_JOIN, "replication join command",
            (CommandHandlercpp) &HADReplication::commandHandler,
            "commandHandler", (Service*) this, DAEMON);


    daemonCore->Register_Command (HAD_REPL_TRANSFER_FILE, "replication transfer file",
            (CommandHandlercpp) &HADReplication::commandHandler,
            "commandHandler", (Service*) this, DAEMON);
    
#endif
}

void
HADStateMachine::onError(char* error_msg)
{
    dprintf( D_ALWAYS,"%s\n",error_msg );
    main_shutdown_graceful();
}

/***********************************************************
  Function : reinitialize - 
    delete all previuosly alocated memory, read all config params from
    config_file again and init all relevant parameters

    Checking configurations parameters:
    -----------------------------------

    HAD_ID is given.
    HAD_NET_LIST is given and all addresses in it are in the formats :
         (<IP:port>),(IP:port),(<name:port>),(name:port)
    HAD_CYCLE_INTERVAL is given

    Daemon command port and address matches exactly one port and address
    in HAD_NET_LIST.

    More checks?

    In case of any of this errors we should exit with error.
*/
int
HADStateMachine::reinitialize()
{
    char* tmp;
    
    finilize(); // DELETE all and start everything over from the scratch
    masterDaemon = new Daemon( DT_MASTER );

    tmp = param( "HAD_STAND_ALONE_DEBUG" );
    if( tmp ){
        debugMode = true;
        free( tmp );
    }

    tmp=param( "HAD_ID" );
    if ( tmp ){
        char* endp;
        selfId = (int)strtol( tmp, &endp, 10 );
        if (!(endp && *endp == '\0')) {
            // Entire string wasn't valid 
            free( tmp );
            onError( "HAD CONFIGURATION ERROR: HAD_ID is not valid in config file" );
        }
        free( tmp );
    } else {
        onError( "HAD CONFIGURATION ERROR: no HAD_ID in config file" );
        selfId = 0;
    }

    tmp=param( "HAD_CYCLE_INTERVAL" );
    if( tmp ) {
        char* endp;
        hadInterval = (int)strtol( tmp, &endp, 10 );
        if (!(endp && *endp == '\0')) {
               // Entire string wasn't valid 
               free( tmp );
               onError( "HAD CONFIGURATION ERROR: HAD_CYCLE_INTERVAL is not valid in config file" );
        }
        free( tmp );
    } else {
        hadInterval = NEGOTIATION_CYCLE;
        onError( "HAD CONFIGURATION ERROR: no HAD_CYCLE_INTERVAL in config file " );
    }
	
    otherHADIPs = new StringList();
    
    tmp=param( "HAD_NET_LIST" );
    if ( tmp ) {
        initializeHADList( tmp );
        free( tmp );
    } else {
        onError( "HAD CONFIGURATION ERROR: no HAD_NET_LIST in config file" );
    }
    
    // check if SEND_COMMAND_TIMEOUT*NUM_of backups > HAD_INTERVAL-safetyFactor.
    // let safetyFactor be 1 sec
    int safetyFactor = 1;
    //int time_to_send_all = (SEND_COMMAND_TIMEOUT*2)*(otherHADIPs->number() + 1);

    // new - send number of messages
    int time_to_send_all = (SEND_COMMAND_TIMEOUT*(MESSAGES_PER_INTERVAL_FACTOR)*2);
    time_to_send_all *= (otherHADIPs->number() + 1);
    if( time_to_send_all > hadInterval - safetyFactor ) {
        // configuration error - can't succeed to send all commands
        // like "I'm alive" during one hadInterval
        char error_msg[256];
            sprintf( error_msg, "%s %d","HAD CONFIGURATION ERROR: HAD_INTERVAL must be at least",
                     time_to_send_all + safetyFactor + 1);
        onError( error_msg );
    }

    //register timer : for first time after hadInterval and for each next time after hadInterval
    stateMachineTimerID = daemonCore->Register_Timer ( 0, hadInterval/(MESSAGES_PER_INTERVAL_FACTOR),
                                    (TimerHandlercpp) &HADStateMachine::cycle,
                                    "Time to check HAD", this );
                                   
    
     
    tmp=param( "HAD_PRIMARY_ID" );
    if( tmp ) {
        char* endp;
        int primaryId = (int)strtol( tmp, &endp, 10 );
        if ( !(endp && *endp == '\0') ) {
               // Entire string wasn't valid 
               free( tmp );
               onError( "HAD CONFIGURATION ERROR: HAD_PRIMARY_ID is not valid in config file" );
        } else {
               if( primaryId == selfId ) {
                    isPrimary = true;
               }
        }

        free( tmp );
    }

                           
#if USE_REPLICATION
    tmp=param( "HAD_REPLICATION_INTERVAL" );
    if( tmp ){
        char* endp;
        replicationInterval = (int)strtol( tmp, &endp, 10 );
        if (!(endp && *endp == '\0')) {
               // Entire string wasn't valid 
               free( tmp );
               onError( "HAD CONFIGURATION ERROR: HAD_CYCLE_INTERVAL is not valid in config file" );
        }
        free( tmp );
        
    } else {
        replicationInterval = REPLICATION_CYCLE;
    }
   
    // Set a timer to replication routine.
    replicaTimerID = daemonCore->Register_Timer (replicationInterval ,replicationInterval,
            (TimerHandlercpp) &HADReplication::replicate,
            "Time to replicate file", this);

    waitingVersionsTimerID = daemonCore->Register_Timer ( 2*hadInterval,0,
            (TimerHandlercpp) &HADReplication::checkReceivedVersions,
            "Time to check received versions", this);
    
    replicator->reinitialize();
    
#endif

    if( debugMode ) {
         print_params_information();
         return TRUE;
    }
 
    if( masterDaemon == NULL || sendNegotiatorCmdToMaster(DAEMON_OFF) == FALSE ) {
         onError("HAD ERROR: unable to send NEGOTIATOR_OFF command");
    }
 
    print_params_information();
    return TRUE;
}

/***********************************************************
  Function :
*/
void
HADStateMachine::cycle(){
    if(callsCounter == 0)
        step();
    sendMessages();
    callsCounter ++;
    callsCounter = callsCounter % MESSAGES_PER_INTERVAL_FACTOR;
  
}

/***********************************************************
  Function :
*/
void
HADStateMachine::step()
{

    my_debug_print_buffers();

    switch( state ) {
        case PRE_STATE:
#if USE_REPLICATION
            if( replicator->isInitializedFile() ) {
                state = PASSIVE_STATE;
                printStep("PRE_STATE","PASSIVE_STATE");
                break;
            } else {
                // return if we are still initializing negotiator file
                if(firstTime == false) {
                    break;
                }

                my_debug_print_list( otherHADIPs );
                firstTime = false;
                replicator->initializeFile( otherHADIPs,hadInterval );
                break;
            }
#else
            state = PASSIVE_STATE;
            printStep("PRE_STATE","PASSIVE_STATE");
            break;
#endif

        case PASSIVE_STATE:
            if( receivedAliveList.IsEmpty() || isPrimary ) {
                state = ELECTION_STATE;
                printStep( "PASSIVE_STATE","ELECTION_STATE" );
                //sendCommandToOthers( HAD_SEND_ID_CMD );
                // we don't want to delete elections buffers
                return;
            } else {
                printStep( "PASSIVE_STATE","PASSIVE_STATE" );
            }

            break;
        case ELECTION_STATE:
            if( !receivedAliveList.IsEmpty() && !isPrimary ) {
                state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

            // command ALIVE isn't received
            if( checkList(&receivedIdList) == FALSE ) {
                // id bigger than selfId is received
                state = PASSIVE_STATE;
                printStep("ELECTION_STATE","PASSIVE_STATE");
                break;
            }

            // if stand alone mode
            if( debugMode ) {
                state = LEADER_STATE;
                printStep("ELECTION_STATE","LEADER_STATE");
                //sendCommandToOthers(HAD_ALIVE_CMD);
                break;
            }

            // no leader in the system and this HAD has biggest id
            if( sendNegotiatorCmdToMaster(DAEMON_ON) == TRUE ) {
                state = LEADER_STATE;
                printStep( "ELECTION_STATE","LEADER_STATE") ;
                //sendCommandToOthers( HAD_ALIVE_CMD );
            } else {
                // TO DO : what with this case ? stay in election case ? return to passive ?
                // may be call sendNegotiatorCmdToMaster(DAEMON_ON) in a loop ?
                dprintf( D_FULLDEBUG,"id %d , cannot send NEGOTIATOR_ON cmd, stay in ELECTION state\n",
                    daemonCore->getpid() );
                onError("");
            }

            break;

        case LEADER_STATE:
            if( !receivedAliveList.IsEmpty() ) {
                if( checkList(&receivedAliveList) == FALSE ) {
                    // send to master "negotiator_off"
                    printStep( "LEADER_STATE","PASSIVE_STATE" );
                    state = PASSIVE_STATE;
                    if( !debugMode ) {
                        sendNegotiatorCmdToMaster( DAEMON_OFF );
                    }
                    break;
                }
            }
            printStep( "LEADER_STATE","LEADER_STATE" );
            //sendCommandToOthers( HAD_ALIVE_CMD );
            break;
    } // end switch
    clearBuffers();
}

/***********************************************************
  Function :
*/
int
HADStateMachine::sendMessages()
{
   switch( state ) {
        case ELECTION_STATE:
            return sendCommandToOthers( HAD_SEND_ID_CMD );
        case LEADER_STATE:
            return sendCommandToOthers( HAD_ALIVE_CMD ) ;
        default :
            return FALSE;
   }
  
}

/***********************************************************
  Function :
*/
int
HADStateMachine::sendCommandToOthers( int comm )
{

    char* addr;
    otherHADIPs->rewind();
    while( (addr = otherHADIPs->next()) ) {
        dprintf( D_FULLDEBUG, "send command %s(%d) to %s\n",
                        commandsHADArray[comm%HAD_ALIVE_CMD],comm,addr);
        Daemon d( DT_ANY, addr );
        ReliSock sock;
        // GABI TODO: make sock nonblocking
        sock.timeout( SEND_COMMAND_TIMEOUT );

        if(!sock.connect( addr,0,true )) {
            dprintf( D_ALWAYS,"cannot connect to addr %s\n",addr );
            sock.close();
            continue;
        }

        int cmd = comm;
        // startCommand - max timeout is SEND_COMMAND_TIMEOUT sec
        if( !d.startCommand(cmd,&sock,SEND_COMMAND_TIMEOUT ) ) {
            dprintf( D_ALWAYS,"cannot start command %s(%d) to addr %s\n",
                commandsHADArray[cmd%HAD_ALIVE_CMD],cmd,addr);
            sock.close();
            continue;
        }

        char stringId[256];
        sprintf( stringId,"%d",selfId );

        char* subsys = (char*)stringId;

        if(!sock.code( subsys ) || !sock.eom()) {
            dprintf( D_ALWAYS,"sock.code false \n");
        } else {
            dprintf( D_FULLDEBUG,"sock.code true \n");
        }
        sock.close();
    }
    return TRUE;
}

/***********************************************************
  Function :
*/
int
HADStateMachine::sendNegotiatorCmdToMaster( int comm )
{

    ReliSock sock;
    // GABI TODO: make sure sock is non blocking
    // if timeout() return value is -1, ...
    sock.timeout( SEND_COMMAND_TIMEOUT );

    if(!sock.connect( masterDaemon->addr(),0,true) ) {
        dprintf( D_ALWAYS,"cannot connect to master , addr %s\n",
                    masterDaemon->addr() );
        sock.close();
        return FALSE;
    }

    int cmd = comm;
    dprintf( D_FULLDEBUG,"send command %s(%d) to master %s\n",
                commandsMasterArray[cmd%DAEMON_OFF], cmd,masterDaemon->addr() );

    // startCommand - max timeout is SEND_COMMAND_TIMEOUT sec
    if(! (masterDaemon->startCommand(cmd,&sock,SEND_COMMAND_TIMEOUT )) ) {
        dprintf( D_ALWAYS,"cannot start command %s, addr %s\n",
                    commandsMasterArray[cmd%DAEMON_OFF], masterDaemon->addr() );
        sock.close();
        return FALSE;
    }

    char* subsys = (char*)daemonString( DT_NEGOTIATOR );

    if( !sock.code(subsys) || !sock.eom() ) {
        dprintf( D_ALWAYS,"send to master , !sock.code false \n");
        sock.close();
        return FALSE;
    } else {
        dprintf( D_FULLDEBUG,"send to master , !sock.code true \n");
    }
                      
    sock.close();     
    return TRUE;      
}

/***********************************************************
  Function :
*/
int
HADStateMachine::pushReceivedAlive( int id )
{
    int* alloc_id = new int[1];
    *alloc_id = id;
    return (receivedAliveList.Append( alloc_id ));
}

/***********************************************************
  Function :
*/
int
HADStateMachine::pushReceivedId( int id )
{
         int* alloc_id = new int[1];
         *alloc_id = id;
         return (receivedIdList.Append( alloc_id ));
}

/***********************************************************
  Function :
*/
void
HADStateMachine::initializeHADList( char* str )
{
    StringList had_net_list;

    //   initializeFromString() and rewind() return void
    had_net_list.initializeFromString( str );

    char* try_address;
    had_net_list.rewind();

    bool iAmPresent = false;
    while( (try_address = had_net_list.next()) ) {
        char* sinfull_addr = convertToSinfull( try_address );
        if(sinfull_addr == NULL) {
            char error_msg[256];
            sprintf( error_msg, "HAD CONFIGURATION ERROR: pid %d , not valid address %s",
                daemonCore->getpid(),try_address );
            onError( error_msg );
            continue;
        }
        if(strcmp( sinfull_addr,daemonCore->InfoCommandSinfulString()) == 0 ) {
            iAmPresent = true;
        } else {
            otherHADIPs->insert( sinfull_addr );
        }
        delete( sinfull_addr );
    } // end while

    if( !iAmPresent ) {
        onError( "HAD CONFIGURATION ERROR :  my address is not present in HAD_NET_LIST" );
    }
}


/***********************************************************
  Function :
*/
int
HADStateMachine::checkList( List<int>* list )
{
    int id;

    list->Rewind();
    while(list->Next( id ) ) {
        if(id > selfId)
            return FALSE;
    }
    return TRUE;
}

/***********************************************************
  Function :
*/
void HADStateMachine::removeAllFromList( List<int>* list )
{
    int* elem;
    list->Rewind();
    while((elem = list->Next()) ) {
        delete elem;
        list->DeleteCurrent();
    }
    //assert(list->IsEmpty());

}

/***********************************************************
    Function :
*/
void HADStateMachine::clearBuffers()
{
    removeAllFromList( &receivedAliveList );
    removeAllFromList( &receivedIdList );
}


/***********************************************************
  Function :
*/
char*
HADStateMachine::convertToSinfull( char* addr )
{

    int port = getPortFromAddr(addr);
    if(port == 0) {
        return NULL;
    }

    char* address = getHostFromAddr( addr ); //returns dinamically allocated buf
    if(address == 0) {
      return NULL;
    }

    struct in_addr sin;
    if(!is_ipaddr( address, &sin )) {
        struct hostent *ent = gethostbyname( address );
        if( ent == NULL ) {
            free( address );
            return NULL;
        }
        char* ip_addr = inet_ntoa(*((struct in_addr *)ent->h_addr));
        free( address );
        address = strdup( ip_addr );
    }

    char port_str[32];
    sprintf( port_str,"%d",port );
    int len = strlen(address)+strlen(port_str)+2*strlen("<")+strlen(":")+1;
    char* result = (char*)malloc( len );
    sprintf( result,"<%s:%d>",address,port );

    free( address );
    return result;
}

/***********************************************************
  Function :
*/
void
HADStateMachine::commandHandler(int cmd,Stream *strm)
{
    dprintf( D_FULLDEBUG, "commandHandler command %s(%d) is received\n",
                commandsHADArray[cmd%HAD_ALIVE_CMD],cmd);

    char* subsys = NULL;

    strm->decode();
    if( ! strm->code(subsys) ) {
        dprintf( D_ALWAYS, "commandHandler -  can't read subsystem name\n" );
        return;
    }
    if( ! strm->end_of_message() ) {
        dprintf( D_ALWAYS, "commandHandler -  can't read end_of_message\n" );
        free( subsys );
        return;
    }

    char* endp;
    int new_id = (int)strtol( subsys, &endp, 10 );
    if (!(endp && *endp == '\0')) {
        /* Entire string wasn't valid */
        free( subsys );
        dprintf( D_ALWAYS,"commandHandler received invalid id %s\n", subsys );
        return;
    }

    free( subsys );

    switch(cmd){
        case HAD_ALIVE_CMD:
            dprintf( D_FULLDEBUG, "commandHandler received ALIVE with id %d\n",
                    new_id);
            pushReceivedAlive( new_id );
            break;

        case HAD_SEND_ID_CMD:
            dprintf( D_FULLDEBUG, "commandHandler received ID_CMD with id %d\n",
                    new_id);
            pushReceivedId( new_id );
            break;
    }


}


/***********************************************************
  Function :
*/
void
HADStateMachine::printStep( char *curState,char *nextState )
{
      dprintf( D_FULLDEBUG, "State mashine step : pid <%d> port <%d> priority <%d> was <%s> go to <%s>\n",
                daemonCore->getpid(),daemonCore->InfoCommandPort(),selfId,
                curState,nextState );
}

/***********************************************************
  Function :
*/
void
HADStateMachine::my_debug_print_list( StringList* str )
{
    str->rewind();
    char* elem;
    dprintf( D_FULLDEBUG, "----> begin print list , id %d\n",
                daemonCore->getpid() );
    while( (elem = str->next()) ) {
        dprintf( D_FULLDEBUG, "----> %s\n",elem );
    }
    dprintf( D_FULLDEBUG, "----> end print list \n" );
}

/***********************************************************
  Function :
*/
void
HADStateMachine::my_debug_print_buffers()
{
    int id;
    dprintf( D_FULLDEBUG, "ALIVE IDs list : \n" );
    receivedAliveList.Rewind();
    while( receivedAliveList.Next( id ) ) {
        dprintf( D_FULLDEBUG, "<%d>\n",id );
    }

    int id2;
    dprintf( D_FULLDEBUG, "ELECTION IDs list : \n" );
    receivedIdList.Rewind();
    while( receivedIdList.Next( id2 ) ) {
        dprintf( D_FULLDEBUG, "<%d>\n",id2 );
    }
}

/***********************************************************
  Function :
*/
void
HADStateMachine::print_params_information()
{
     dprintf( D_ALWAYS,"** HAD_ID   %d\n",selfId);
     dprintf( D_ALWAYS,"** HAD_CYCLE_INTERVAL   %d\n",hadInterval);
     dprintf( D_ALWAYS,"** HAD_PRIMARY_ID(true/false)   %d\n",isPrimary);
     dprintf( D_ALWAYS,"** HAD_NET_LIST(others only)\n");
     char* addr = NULL;
     otherHADIPs->rewind();
     while( (addr = otherHADIPs->next()) ) {
           dprintf( D_ALWAYS,"**    %s\n",addr);
     }
     dprintf( D_ALWAYS,"** HAD_STAND_ALONE_DEBUG(true/false)    %d\n",debugMode);
     dprintf( D_ALWAYS,"** HAD_REPLICATION_INTERVAL     %d\n",replicationInterval);

}





