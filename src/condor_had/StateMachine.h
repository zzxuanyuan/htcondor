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
// for 'HadState'
#include "HadState.h"

//#undef IS_REPLICATION_USED
//#define IS_REPLICATION_USED

//#define MESSAGES_PER_INTERVAL_FACTOR (2)
#define SEND_COMMAND_TIMEOUT (5) // 5 seconds

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

    virtual bool reinitialize();
   /* Function    : reconfigure
    * Return value: bool - success value
	* Description : reconfigures all inner structures, depending on the 
    *               changes, performed in the configuration file: it might be
    *               a full restart or just a reloading of parameters
	*/
	bool reconfigure();
protected:
   /* Function    : isHardConfigurationNeeded
 	* Return value: bool - whether we have to reconfigure all the parameters of
	*					   the HAD or only those that do not affect the
	*					   negotiator's location
	* Description : checks, what type of reconfiguration we have to perform: the
	*				hard reconfiguration, i.e. reloading the configuration file
	*				once again, or soft one, i.e. reloading only the parameters,
	*				which do not affect the negotiator's location
 	*/
	bool isHardConfigurationNeeded();
   /* Function    : softReconfigure 
    * Return value: int - success value 
    * Description : reconfigures only the parameters, which do not affects the
	*				location of the negotiator 
    */
	int softReconfigure();
    /*
      step() - called each m_hadInterval, implements one state of the
      state machine.
    */
    void  step();
    
    /*
      cycle() - called MESSAGES_PER_INTERVAL_FACTOR times per m_hadInterval
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

    HadState m_state;   
    int      m_stateMachineTimerID;
        
    int      m_hadInterval;
    int      m_connectionTimeout;
    
    // if m_callsCounter equals to 0 ,
    // enter state machine , otherwise send messages
    char     m_callsCounter;
    
    int      m_selfId;
    bool     m_isPrimary;
    bool     m_usePrimary;
    StringList* m_otherHADIPs;
    Daemon*     m_masterDaemon;

    List<int> m_receivedAliveList;
    List<int> m_receivedIdList;

    static bool initializeHADList(char* , bool , StringList*, int* );
    int  checkList(List<int>*);
    static void removeAllFromList(List<int>*);
    void clearBuffers();
    //void printStep(char *curState,char *nextState);
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
    bool m_standAloneMode;
    static void my_debug_print_list(StringList* str);
    void my_debug_print_buffers();

	void registerCommand(int command);

// replication-specific data members and functions
	// usage of replication, controlled by configuration parameter 
	// USE_REPLICATION
	bool m_useReplication;

	int sendReplicationCommand( int );
	void setReplicationDaemonSinfulString( );

	char* m_replicationDaemonSinfulString;
	// finished replication's timer handler
	void replicationFinished();
	// timer, controlling the time allowed for finishing the replication, while
	// joining the pool
	int   m_replicationFinishedTimerId;
// End of replication-specific data members and functions

// classad-specific data members and functions
    void initializeClassAd();
    // collector updates' timer handler
    void updateCollectors();
    // updates collectors upon changing from/to leader state
    void updateCollectorsClassAd( const MyString& isHadActive );
	// invalidates HAD classad in collectors
	void invalidateClassAd();

    ClassAd*       m_classAd;
    // info about our central manager
    CollectorList* m_collectorsList;
    int            m_updateCollectorTimerId;
    int            m_updateCollectorInterval;
// End of classad-specific data members and functions
};

#endif // !HAD_StateMachine_H__
