////////////////////////////////////////////////////////////////////////////////
//
// This file contains the definition for class DaemonCore. This is the
// central structure for every daemon in condor. The daemon core triggers
// preregistered handlers for corresponding events. class Service is the base
// class of the classes that daemon_core can serve. In order to use a class
// with the DaemonCore, it has to be a derived class of Service.
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _CONDOR_DAEMON_CORE_H_
#define _CONDOR_DAEMON_CORE_H_

#include "condor_common.h"
#include "condor_timer_manager.h"
#include "condor_io.h"
#include "condor_uid.h"

#ifndef WIN32
#if defined(Solaris)
#define __EXTENSIONS__
#endif
#include <sys/types.h>
#include <sys/time.h>
#include "condor_fdset.h"
#if defined(Solaris)
#undef __EXTENSIONS__
#endif
#endif  /* ifndef WIN32 */

#include "condor_io.h"
#include "condor_timer_manager.h"
#include "condor_commands.h"

// enum for Daemon Core socket/command/signal permissions
enum DCpermission { ALLOW, READ, WRITE };

static const int KEEP_STREAM = 100;
static char* EMPTY_DESCRIP = "<NULL>";

// typedefs for callback procedures
typedef int		(*CommandHandler)(Service*,int,Stream*);
typedef int		(Service::*CommandHandlercpp)(int,Stream*);

typedef int		(*SignalHandler)(Service*,int);
typedef int		(Service::*SignalHandlercpp)(int);

typedef int		(*SocketHandler)(Service*,Stream*);
typedef int		(Service::*SocketHandlercpp)(Stream*);

typedef int		(*ReaperHandler)(Service*,int pid,int exit_status);
typedef int		(Service::*ReaperHandlercpp)(int pid,int exit_status);

class DaemonCore : public Service
{
	public:
		
		DaemonCore(int ComSize = 0, int SigSize = 0, int SocSize = 0);
		~DaemonCore();

		void	Driver();
		

		
		int		Register_Command(int command, char *com_descrip, CommandHandler handler, 
					char *handler_descrip, Service* s = NULL, DCpermission perm = ALLOW);
		int		Register_Command(int command, char *com_descript, CommandHandlercpp handlercpp, 
					char *handler_descrip, Service* s, DCpermission perm = ALLOW);
		int		Cancel_Command( int command );
		int		InfoCommandPort();

		int		Register_Signal(int sig, char *sig_descrip, SignalHandler handler, 
					char *handler_descrip, Service* s = NULL, DCpermission perm = ALLOW);
		int		Register_Signal(int sig, char *sig_descript, SignalHandlercpp handlercpp, 
					char *handler_descrip, Service* s, DCpermission perm = ALLOW);
		int		Cancel_Signal( int sig );
			
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandler handler,
					char *handler_descrip, Service* s = NULL, DCpermission perm = ALLOW);
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandlercpp handlercpp,
					char *handler_descrip, Service* s, DCpermission perm = ALLOW);
		int		Register_Command_Socket( Stream* iosock, char *descrip = NULL ) {
					return(Register_Socket(iosock,descrip,NULL,NULL,"DC Command Handler",NULL,ALLOW,0)); 
				}
		int		Cancel_Socket( Stream* );

		int		Register_Timer(unsigned deltawhen, Event event, char *event_descrip, 
					Service* s = NULL, int id = -1);
		int		Register_Timer(unsigned deltawhen, unsigned period, Event event, 
					char *event_descrip, Service* s = NULL, int id = -1);
		int		Register_Timer(unsigned deltawhen, Eventcpp event, char *event_descrip, 
					Service* s, int id = -1);
		int		Register_Timer(unsigned deltawhen, unsigned period, Eventcpp event, 
					char *event_descrip, Service* s, int id = -1);
		int		Cancel_Timer( int id );

		void	Dump(int, char* = NULL );

		inline int getpid() { return 0; };

		int		Send_Signal(int pid, int sig);

#ifdef FUTURE		
		int		Block_Signal()
		int		Unblock_Signal()
		int		Register_Reaper(Service* s, ReaperHandler handler);
		int		Create_Process()
		int		Create_Thread()
		int		Kill_Process()
		int		Kill_Thread()
#endif

		int		HandleSigCommand(int command, Stream* stream);
		
	private:

		void	HandleReq(int socki);

		int		HandleSig(int command, int sig);

		int		Register_Command(int command, char *com_descip, CommandHandler handler, 
					CommandHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);
		int		Register_Signal(int sig, char *sig_descip, SignalHandler handler, 
					SignalHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);
		int		Register_Socket(Stream* iosock, char *iosock_descrip, SocketHandler handler, 
					SocketHandlercpp handlercpp, char *handler_descrip, Service* s, 
					DCpermission perm, int is_cpp);

		struct CommandEnt
		{
		    int				num;
		    CommandHandler	handler;
			CommandHandlercpp	handlercpp;
			int				is_cpp;
			DCpermission	perm;
			Service*		service; 
			char*			command_descrip;
			char*			handler_descrip;
		};
		void				DumpCommandTable(int, const char* = NULL);
		int					maxCommand;		// max number of command handlers
		int					nCommand;		// number of command handlers used
		CommandEnt*			comTable;		// command table

		struct SignalEnt 
		{
			int				num;
		    SignalHandler	handler;
			SignalHandlercpp	handlercpp;
			int				is_cpp;
			DCpermission	perm;
			Service*		service; 
			int				is_blocked;
			int				is_pending;
			char*			sig_descrip;
			char*			handler_descrip;
		};
		void				DumpSigTable(int, const char* = NULL);
		int					maxSig;		// max number of signal handlers
		int					nSig;		// number of signal handlers used
		SignalEnt*			sigTable;		// signal table
		int					sent_signal;	// TRUE if a signal handler sends a signal

		struct SockEnt
		{
		    Stream*			iosock;
			SOCKET			sockd;
		    SocketHandler	handler;
			SocketHandlercpp	handlercpp;
			int				is_cpp;
			DCpermission	perm;
			Service*		service; 
			char*			iosock_descrip;
			char*			handler_descrip;
		};
		void				DumpSocketTable(int, const char* = NULL);
		int					maxSocket;		// max number of socket handlers
		int					nSock;		// number of socket handlers used
		SockEnt*			sockTable;		// socket table
		int					initial_command_sock;  

		static				TimerManager t;

		void				DumpTimerList(int, char* = NULL);

		void				free_descrip(char *p) { if(p &&  p != EMPTY_DESCRIP) free(p); }
	
		fd_set				readfds; 
};

#endif
