/*
  StateMachine.h: interface for the HADStateMachine class.
*/

#if !defined(HAD_StateMachine_H__)
#define HAD_StateMachine_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"

// TODO : to enter the commands to command types
// file   condor_includes/condor_commands.h
#define HAD_ALIVE_CMD           500
#define HAD_SEND_ID_CMD         501
#define HAD_REPL_FILE_VERSION   502
#define HAD_REPL_JOIN           503
#define HAD_REPL_TRANSFER_FILE  504
// end TODO

#define NEGOTIATION_CYCLE 	    	(5) //5 seconds
#define SEND_COMMAND_TIMEOUT 		(1) // 1 second
#define REPLICATION_CYCLE 300 //5 min (5*60)
#define MESSAGES_PER_INTERVAL_FACTOR (2)

#define  USE_REPLICATION    (0)

class HADReplication;

typedef enum {
    PRE_STATE = 1,
    PASSIVE_STATE = 2,
    ELECTION_STATE = 3,
    LEADER_STATE = 4
}STATES;

/*
  class HADStateMachine
*/
class HADStateMachine  :public Service
{
public:

    /*
      Const'rs
    */

    HADStateMachine();

    /*
      Destructor
    */
    ~HADStateMachine();

    void initialize();

    int reinitialize();

    /*
      step() - called each hadInterval, implements one state of the
      state machine.
    */
    void  step();
    
    /*
      cycle() - called MESSAGES_PER_INTERVAL_FACTOR times per hadInterval
    */
    void  cycle();


    /*
      sendCommandToOthers(int command) - send "ALIVE command" or
      "SEND ID command" to all HADs from HAD list.
    */
    int sendCommandToOthers( int );

    /*
      send "ALIVE command" or "SEND ID command" to all HADs from HAD list
      acconding to state
    */
    int sendMessages();
    
    /*
      sendNegotiatorCmdToMaster(int) - snd "NEGOTIATOR ON" or "NEGOTIATOR OFF"
      to master.
      return TRUE in case of success or FALSE otherwise
    */
    int sendNegotiatorCmdToMaster( int );

    /*
      pushReceivedAlive(int id) - push to buffer id of HAD that
      sent "ALIVE command".
    */
    int pushReceivedAlive( int );

    /*
      pushReceivedId(int id) -  push to buffer id of HAD that sent
      "SEND ID command".
    */
    int pushReceivedId( int );

    void commandHandler(int cmd,Stream *strm) ;
private:
    int state;
    
    int stateMachineTimerID;
    int replicaTimerID;
    int waitingVersionsTimerID;
        
    int hadInterval;
    int replicationInterval;

    // if callsCounter equals to 0 ,
    // enter state machine , otherwise send messages
    char callsCounter;
    
    int selfId;
    bool isPrimary;
    StringList* otherHADIPs;
    Daemon* masterDaemon;
    HADReplication* replicator;

    List<int> receivedAliveList;
    List<int> receivedIdList;

    bool firstTime;
    void initializeHADList(char*);
    int  checkList(List<int>*);
    void removeAllFromList(List<int>*);
    void clearBuffers();
    void printStep(char *curState,char *nextState);

    void finilize();
    void init();
    void onError(char*);

    char* convertToSinfull(char* addr);
    void print_params_information();
    // debug information
    bool debugMode;
    void my_debug_print_list(StringList* str);
    void my_debug_print_buffers();

};

#endif // !HAD_StateMachine_H__
