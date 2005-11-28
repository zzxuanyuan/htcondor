/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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

/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 *
 * The Condor High Availability Daemon (HAD) software was developed by
 * by the Distributed Systems Laboratory at the Technion Israel
 * Institute of Technology, and contributed to to the Condor Project.
 *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/*
  StateMachine.h: interface for the HADStateMachine class.
*/

#if !defined(HAD_StateMachine_H__)
#define HAD_StateMachine_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"

//#undef IS_REPLICATION_USED
//#define IS_REPLICATION_USED

#define MESSAGES_PER_INTERVAL_FACTOR (2)
#define SEND_COMMAND_TIMEOUT (5) // 5 seconds

typedef enum {
    PRE_STATE = 1,
    PASSIVE_STATE = 2,
    ELECTION_STATE = 3,
    LEADER_STATE = 4
}STATES;

class CollectorList;
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
    virtual ~HADStateMachine();

    virtual void initialize();

    virtual int reinitialize();

protected:
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

    int state;   
    int stateMachineTimerID;
        
    int hadInterval;
    int connectionTimeout;
    
    // if callsCounter equals to 0 ,
    // enter state machine , otherwise send messages
    char callsCounter;
    
    int selfId;
    bool isPrimary;
    bool usePrimary;
    StringList* otherHADIPs;
    Daemon* masterDaemon;

    List<int> receivedAliveList;
    List<int> receivedIdList;

    void initializeHADList(char*);
    int  checkList(List<int>*);
    void removeAllFromList(List<int>*);
    void clearBuffers();
    void printStep(char *curState,char *nextState);
    //char* commandToString(int command);

    void finalize();
    void init();
    
    /*
        convertToSinfull(char* addr)
        addr - address in one of the formats :
            <IP:port>,IP:port,<name:port>,name:port
        return address in the format <IP:port>
    */
    //char* convertToSinfull(char* addr);
    void printParamsInformation();

    //int myatoi(const char* str, bool* res);

    // debug information
    bool standAloneMode;
    void my_debug_print_list(StringList* str);
    void my_debug_print_buffers();

	// usage of replication, controlled by configuration parameter 
	// USE_REPLICATION
	bool useReplication;

	int sendReplicationCommand( int );
	void setReplicationDaemonSinfulString( );

	char* replicationDaemonSinfulString;
// classad-specific data members and functions
    void initializeClassAd();
    // timer handler
    void updateCollectors();
    // updates collectors upon changing from/to leader state
    void updateCollectorsClassAd( const MyString& isHadActive );

    ClassAd*       classAd;
    // info about our central manager
    CollectorList* collectorsList;
    int            updateCollectorTimerId;
    int            updateCollectorInterval;
};

#endif // !HAD_StateMachine_H__
