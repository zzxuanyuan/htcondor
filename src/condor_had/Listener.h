// Listener.h: interface for the HADListener class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(HAD_LISTENER_H__)
#define HAD_LISTENER_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"

class HADStateMachine;

    /**
    * class   HADListener - listen for incoming messages and
    *       put them to  HADStateMachine buffers.
    */
class HADListener  :public Service
{
public:

    /**
    *   Const'r
    */
    HADListener(HADStateMachine*);

    /**
    *   Destructor
    */
	~HADListener();

    /**
    *   initialize() - register to messages
    */
	void initialize();

	int reinitialize();


    /**
    *    commandHandler - handler for "ALIVE command" and "SEND ID command"
    */
    void commandHandler(int cmd,Stream *strm) ;


private:

    HADStateMachine* StateMachine;
	
};

#endif // !HAD_LISTENER_H__
