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


/***********************************************************
  Function :
*/
HADStateMachine::HADStateMachine()
{
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
    firstTime = true;
    callsCounter = 0;
    stateMachineTimerID = -1;
    //sendMsgTimerID = -1;
    replicaTimerID = -1;
    waitingVersionsTimerID = -1;
    hadInterval = -1;
    // set valid value to send NEGOTIATOR_OFF
    connectionTimeout = SEND_COMMAND_TIMEOUT;
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
    connectionTimeout = SEND_COMMAND_TIMEOUT;
    
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
    
    selfId = -1;

#if USE_REPLICATION
    
    //if(replicator != NULL) {  
    //     delete replicator;
    //     replicator = NULL;    
    //}
        
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
#if USE_REPLICATION
    if(replicator != NULL) {
         delete replicator;
         replicator = NULL;
    }
#endif
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
            (CommandHandlercpp) &HADStateMachine::commandHandler,
            "commandHandler", (Service*) this, DAEMON );

    daemonCore->Register_Command (HAD_REPL_JOIN, "replication join command",
            (CommandHandlercpp) &HADStateMachine::commandHandler,
            "commandHandler", (Service*) this, DAEMON );


    daemonCore->Register_Command (HAD_REPL_TRANSFER_FILE, "replication transfer file",
            (CommandHandlercpp) &HADStateMachine::commandHandler,
            "commandHandler", (Service*) this, DAEMON );
    replicator->initialize();
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
        bool res = false;
        selfId = myatoi(tmp,&res);
        if(!res){
            free( tmp );
            onError( "HAD CONFIGURATION ERROR: HAD_ID is not valid in config file" );
        }
        free( tmp );
    } else {
        onError( "HAD CONFIGURATION ERROR: no HAD_ID in config file" );
        selfId = 0;
    }
    
    otherHADIPs = new StringList();
    tmp=param( "HAD_NET_LIST" );
    if ( tmp ) {
        initializeHADList( tmp );
        free( tmp );
    } else {
        onError( "HAD CONFIGURATION ERROR: no HAD_NET_LIST in config file" );
    }
    
    tmp=param( "HAD_CONNECTION_TIMEOUT" );
    if( tmp ) {
        bool res = false;
        connectionTimeout = myatoi(tmp,&res);
        if(!res){
            free( tmp );
            onError( "HAD CONFIGURATION ERROR: HAD_CONNECTION_TIMEOUT is not valid in config file" );
        }
        
        if(connectionTimeout <= 0){
               free( tmp );
               onError( "HAD CONFIGURATION ERROR: HAD_CONNECTION_TIMEOUT is not valid in config file" );
        }
        // calculate hadInterval
        int safetyFactor = 1;
        int timeoutNumber = 2; // connect + startCommand (sock.code() and sock.eom() aren't blocking)
        
        int time_to_send_all = (connectionTimeout*timeoutNumber);
        time_to_send_all *= (otherHADIPs->number() + 1); // otherHads + master
        
        hadInterval =  (time_to_send_all + safetyFactor)*(MESSAGES_PER_INTERVAL_FACTOR);
        free( tmp );
    } else {
        onError( "HAD CONFIGURATION ERROR: no HAD_CONNECTION_TIMEOUT in config file " );
    }
	    
    tmp=param( "HAD_PRIMARY_ID" );
    if( tmp ) {
        bool res = false;
        int primaryId = myatoi(tmp,&res);
        if(!res){
            free( tmp );
            onError( "HAD CONFIGURATION ERROR: HAD_PRIMARY_ID is not valid in config file" );
        } else{
            if( primaryId == selfId ) {
                    isPrimary = true;
               }
        }

        free( tmp );
    }

    stateMachineTimerID = daemonCore->Register_Timer ( 0,
                                    (TimerHandlercpp) &HADStateMachine::cycle,
                                    "Time to check HAD", this);

    dprintf( D_ALWAYS,"** Register on stateMachineTimerID , interval = %d\n",
            hadInterval/(MESSAGES_PER_INTERVAL_FACTOR));
                                       
#if USE_REPLICATION
    //replicator = new HADReplication();
    replicator->reinitialize();

    tmp=param( "HAD_REPLICATION_INTERVAL" );
    if( tmp ){
        bool res = false;
        replicationInterval = myatoi(tmp,&res);
        if(!res){
            free( tmp );
            onError( "HAD CONFIGURATION ERROR: HAD_REPLICATION_INTERVAL is not valid in config file" );
        }
        free( tmp );
        
    } else {
        onError( "HAD CONFIGURATION ERROR: HAD_CYCLE_INTERVAL is not valid in config file" );
    }

    // Set a timer to replication routine.
    replicaTimerID = daemonCore->Register_Timer (replicationInterval ,replicationInterval,
            (TimerHandlercpp) &HADStateMachine::replicaTimerHandler,
            "Time to replicate file", this);


    waitingVersionsTimerID = daemonCore->Register_Timer ( 2*hadInterval,
            (TimerHandlercpp) &HADStateMachine::waitingVersionsTimerHandler,
            "Time to check received versions", this);


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
    dprintf( D_FULLDEBUG, "-------------- > Timer stateMachineTimerID is called\n");
    
    if(stateMachineTimerID >= 0){
      daemonCore->Cancel_Timer( stateMachineTimerID );
    }
    stateMachineTimerID = daemonCore->Register_Timer ( hadInterval/(MESSAGES_PER_INTERVAL_FACTOR),
                                    (TimerHandlercpp) &HADStateMachine::cycle,
                                    "Time to check HAD", this);
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

                //my_debug_print_list( otherHADIPs );
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
                break;
            }

            // no leader in the system and this HAD has biggest id
            if( sendNegotiatorCmdToMaster(DAEMON_ON) == TRUE ) {
                state = LEADER_STATE;
                printStep( "ELECTION_STATE","LEADER_STATE") ;
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
        char str_command[256];
        commandToString(comm,str_command);
        dprintf( D_FULLDEBUG, "send command %s(%d) to %s\n",
                        str_command,comm,addr);

        Daemon d( DT_ANY, addr );
        ReliSock sock;

        sock.timeout( connectionTimeout );
        sock.doNotEnforceMinimalCONNECT_TIMEOUT();

        // blocking with timeout connectionTimeout
        if(!sock.connect( addr,0,false )) {
            dprintf( D_ALWAYS,"cannot connect to addr %s\n",addr );
            sock.close();
            continue;
        }

        int cmd = comm;
        // startCommand - max timeout is connectionTimeout sec
        if( !d.startCommand(cmd,&sock,connectionTimeout ) ) {
            dprintf( D_ALWAYS,"cannot start command %s(%d) to addr %s\n",
                str_command,cmd,addr);
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

    sock.timeout( connectionTimeout );
    sock.doNotEnforceMinimalCONNECT_TIMEOUT();

    // blocking with timeout connectionTimeout
    if(!sock.connect( masterDaemon->addr(),0,false) ) {
        dprintf( D_ALWAYS,"cannot connect to master , addr %s\n",
                    masterDaemon->addr() );
        sock.close();
        return FALSE;
    }

    int cmd = comm;
    char str_command[256];
    commandToString(comm,str_command);
    dprintf( D_FULLDEBUG,"send command %s(%d) to master %s\n",
                str_command, cmd,masterDaemon->addr() );

    // startCommand - max timeout is connectionTimeout sec
    if(! (masterDaemon->startCommand(cmd,&sock,connectionTimeout )) ) {
        dprintf( D_ALWAYS,"cannot start command %s, addr %s\n",
                    str_command, masterDaemon->addr() );
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
void
HADStateMachine::replicaTimerHandler(){
#if USE_REPLICATION
    replicator->replicate();
#endif
}

/***********************************************************
  Function :
*/
void
HADStateMachine::waitingVersionsTimerHandler(){
#if USE_REPLICATION
    if ( waitingVersionsTimerID >= 0 ) {
        daemonCore->Cancel_Timer( waitingVersionsTimerID );
        waitingVersionsTimerID = -1;
    }
    replicator->checkReceivedVersions();
#endif
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
    char str_command[256];
    commandToString(cmd,str_command);
    dprintf( D_FULLDEBUG, "commandHandler command (%d) is received\n",cmd);
  
    switch(cmd){
        case HAD_ALIVE_CMD:
        case HAD_SEND_ID_CMD:
            commandHandlerStateMachine(cmd,strm);
            break;
#if USE_REPLICATION
        case HAD_REPL_FILE_VERSION:
        case HAD_REPL_JOIN:
        case HAD_REPL_TRANSFER_FILE:
            replicator->commandHandler(cmd,strm);
            break;
#endif
    }
}
/***********************************************************
  Function :
*/
void
HADStateMachine::commandHandlerStateMachine(int cmd,Stream *strm)
{
    
    char* subsys = NULL;
    strm->timeout( connectionTimeout );
    
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


    bool res = false;
    int new_id = myatoi(subsys,&res);
    if(!res){
          dprintf( D_ALWAYS,"commandHandler received invalid id %s\n", subsys );
        
    }
    free( subsys );
    

    switch(cmd){
        case HAD_ALIVE_CMD:
            dprintf( D_FULLDEBUG, "commandHandler received HAD_ALIVE_CMD with id %d\n",
                    new_id);
            pushReceivedAlive( new_id );
            break;

        case HAD_SEND_ID_CMD:
            dprintf( D_FULLDEBUG, "commandHandler received HAD_SEND_ID_CMD with id %d\n",
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
     dprintf( D_ALWAYS,"** HAD_CONNECTION_TIMEOUT   %d\n",connectionTimeout);
     dprintf( D_ALWAYS,"** HAD_PRIMARY_ID(true/false)   %d\n",isPrimary);
     dprintf( D_ALWAYS,"** HAD_NET_LIST(others only)\n");
     char* addr = NULL;
     otherHADIPs->rewind();
     while( (addr = otherHADIPs->next()) ) {
           dprintf( D_ALWAYS,"**    %s\n",addr);
     }
     dprintf( D_ALWAYS,"** HAD_STAND_ALONE_DEBUG(true/false)    %d\n",debugMode);
#if USE_REPLICATION
     dprintf( D_ALWAYS,"** HAD_REPLICATION_INTERVAL     %d\n",replicationInterval);
#endif
}

/***********************************************************
  Function :
*/
void
HADStateMachine::commandToString(int command, char* comm_string)
{
    switch(command){
        case HAD_ALIVE_CMD:
            sprintf(comm_string,"HAD_ALIVE_CMD");
            break;
        case HAD_SEND_ID_CMD:
            sprintf(comm_string,"HAD_SEND_ID_CMD");
            break;
        case DAEMON_ON:
            sprintf(comm_string,"DAEMON_ON");
            break;        
        case DAEMON_OFF:
            sprintf(comm_string,"DAEMON_OFF");
            break;        
#if USE_REPLICATION
        case HAD_REPL_FILE_VERSION:
            sprintf(comm_string,"HAD_REPL_FILE_VERSION");
            break;
        case HAD_REPL_JOIN:
            sprintf(comm_string,"HAD_REPL_JOIN");
            break;
        case HAD_REPL_TRANSFER_FILE:
            sprintf(comm_string,"HAD_REPL_TRANSFER_FILE");
            break;
#endif
        default :
            sprintf(comm_string,"\"unknown command\"");

    }
}

/***********************************************************
  Function :
*/
int
HADStateMachine::myatoi(const char* str, bool* res)
{
    if ( *str=='\0' ){
        *res = false;
        return 0;
    } else{
        char* endp;
        int ret_val = (int)strtol( str, &endp, 10 );
        if (endp!=NULL && *endp != '\0'){
            // Any reminder is considered as an error
            // Entire string wasn't valid
            *res = false;
            return 0;
        }
        *res = true;
        return ret_val;
    }
}

