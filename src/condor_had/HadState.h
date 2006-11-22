#ifndef HAD_STATE_H
#define HAD_STATE_H

// the state of HAD
typedef enum { PRE_STATE = 1     , PASSIVE_STATE = 2, 
			   ELECTION_STATE = 3, LEADER_STATE  = 4 } HadState;
				
#endif // HAD_STATE_H			   
