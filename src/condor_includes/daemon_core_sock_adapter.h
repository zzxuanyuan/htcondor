/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef DAEMON_CORE_SOCK_ADAPTER_H
#define DAEMON_CORE_SOCK_ADAPTER_H

/**
   This class is used as an indirect way to call daemonCore functions
   from cedar sock code.  Not all applications that use cedar
   are linked with DaemonCore (or use the DaemonCore event loop).
   In such applications, this daemonCore interface class will not
   be initialized and it is an error if these functions are ever
   used in such cases.  (They will EXCEPT.)
 */

#include "condor_daemon_core.h"

class DaemonCoreSockAdapterClass {
 public:
	typedef int (DaemonCore::*Register_Socket_fnptr)(Stream*,const char*,SocketHandlercpp,const char*,Service*,DCpermission);
	typedef int (DaemonCore::*Cancel_Socket_fnptr)( Stream *sock );
	typedef void (DaemonCore::*CallSocketHandler_fnptr)( Stream *sock, bool default_to_HandleCommand );
	typedef int (DaemonCore::*CallCommandHandler_fnptr)( int cmd, Stream *stream, bool delete_stream);
	typedef void (DaemonCore::*HandleReqAsync_fnptr)(Stream *stream);
    typedef int (DaemonCore::*Register_DataPtr_fnptr)( void *data );
    typedef void *(DaemonCore::*GetDataPtr_fnptr)();
	typedef int (DaemonCore::*Register_Timer_fnptr)(unsigned deltawhen,TimerHandlercpp handler,const char * event_descrip,Service* s);
	typedef int (DaemonCore::*Register_PeriodicTimer_fnptr)(unsigned deltawhen,unsigned period,TimerHandlercpp handler,const char * event_descrip,Service* s);
	typedef int (DaemonCore::*Cancel_Timer_fnptr)(int id);
	typedef bool (DaemonCore::*TooManyRegisteredSockets_fnptr)(int fd,MyString *msg,int num_fds);
	typedef void (DaemonCore::*incrementPendingSockets_fnptr)();
	typedef void (DaemonCore::*decrementPendingSockets_fnptr)();
	typedef const char* (DaemonCore::*publicNetworkIpAddr_fnptr)();
    typedef int (DaemonCore::*Register_Command_fnptr) (
		int             command,
		char const*     com_descrip,
		CommandHandler  handler, 
		char const*     handler_descrip,
		Service *       s,
		DCpermission    perm,
		int             dprintf_flag,
		bool            force_authentication);
	typedef void (DaemonCore::*daemonContactInfoChanged_fnptr)();
	typedef int (DaemonCore::*Create_Named_Pipe_fnptr) (
		int *pipe_ends,
		bool can_register_read,
		bool can_register_write,
		bool nonblocking_read,
		bool nonblocking_write,
		unsigned int psize,
		const char* pipe_name);
	typedef int (DaemonCore::*Register_Pipe_fnptr) (
		int pipe_end,
		const char* pipe_descrip,
		PipeHandlercpp handlercpp,
		const char *handler_descrip,
		Service* s,
		HandlerType handler_type,
		DCpermission perm);
#ifdef WIN32
	typedef HANDLE (DaemonCore::*Get_Inherit_Pipe_Handle_fnptr) (int pipe_end);
	typedef int (DaemonCore::*Inherit_Pipe_Handle_fnptr) (
		HANDLE pipe_handle,
		bool write,
		bool overlapping,
		bool nonblocking,
		int psize);
#endif
	typedef int (DaemonCore::*Read_Pipe_fnptr) (int pipe_end, void* buffer, int len);
	typedef int (DaemonCore::*Write_Pipe_fnptr) (int pipe_end, const void* buffer, int len);
	typedef int (DaemonCore::*Close_Pipe_fnptr) (int pipe_end);


	DaemonCoreSockAdapterClass(): m_daemonCore(0) {}

	void EnableDaemonCore(
		DaemonCore *dC,
		Register_Socket_fnptr in_Register_Socket_fnptr,
		Cancel_Socket_fnptr in_Cancel_Socket_fnptr,
		CallSocketHandler_fnptr in_CallSocketHandler_fnptr,
		CallCommandHandler_fnptr in_CallCommandHandler_fnptr,
		HandleReqAsync_fnptr in_HandleReqAsync_fnptr,
		Register_DataPtr_fnptr in_Register_DataPtr_fnptr,
		GetDataPtr_fnptr in_GetDataPtrFun_fnptr,
		Register_Timer_fnptr in_Register_Timer_fnptr,
		Register_PeriodicTimer_fnptr in_Register_PeriodicTimer_fnptr,
		Cancel_Timer_fnptr in_Cancel_Timer_fnptr,
		TooManyRegisteredSockets_fnptr in_TooManyRegisteredSockets_fnptr,
		incrementPendingSockets_fnptr in_incrementPendingSockets_fnptr,
		decrementPendingSockets_fnptr in_decrementPendingSockets_fnptr,
		publicNetworkIpAddr_fnptr in_publicNetworkIpAddr_fnptr,
		Register_Command_fnptr in_Register_Command_fnptr,
		daemonContactInfoChanged_fnptr in_daemonContactInfoChanged_fnptr,
		Create_Named_Pipe_fnptr in_Create_Named_Pipe_fnptr,
		Register_Pipe_fnptr in_Register_Pipe_fnptr,
#ifdef WIN32
		Get_Inherit_Pipe_Handle_fnptr in_Get_Inherit_Pipe_Handle_fnptr,
		Inherit_Pipe_Handle_fnptr in_Inherit_Pipe_Handle_fnptr,
#endif
		Read_Pipe_fnptr in_Read_Pipe_fnptr,
		Write_Pipe_fnptr in_Write_Pipe_fnptr,
		Close_Pipe_fnptr in_Close_Pipe_fnptr)
	{
		m_daemonCore = dC;
		m_Register_Socket_fnptr = in_Register_Socket_fnptr;
		m_Cancel_Socket_fnptr = in_Cancel_Socket_fnptr;
		m_CallSocketHandler_fnptr = in_CallSocketHandler_fnptr;
		m_CallCommandHandler_fnptr = in_CallCommandHandler_fnptr;
		m_HandleReqAsync_fnptr = in_HandleReqAsync_fnptr;
		m_Register_DataPtr_fnptr = in_Register_DataPtr_fnptr;
		m_GetDataPtr_fnptr = in_GetDataPtrFun_fnptr;
		m_Register_Timer_fnptr = in_Register_Timer_fnptr;
		m_Register_PeriodicTimer_fnptr = in_Register_PeriodicTimer_fnptr;
		m_Cancel_Timer_fnptr = in_Cancel_Timer_fnptr;
		m_TooManyRegisteredSockets_fnptr = in_TooManyRegisteredSockets_fnptr;
		m_incrementPendingSockets_fnptr = in_incrementPendingSockets_fnptr;
		m_decrementPendingSockets_fnptr = in_decrementPendingSockets_fnptr;
		m_publicNetworkIpAddr_fnptr = in_publicNetworkIpAddr_fnptr;
		m_Register_Command_fnptr = in_Register_Command_fnptr;
		m_daemonContactInfoChanged_fnptr = in_daemonContactInfoChanged_fnptr;
		m_Create_Named_Pipe_fnptr = in_Create_Named_Pipe_fnptr;
		m_Register_Pipe_fnptr = in_Register_Pipe_fnptr;
#ifdef WIN32
		m_Get_Inherit_Pipe_Handle_fnptr = in_Get_Inherit_Pipe_Handle_fnptr;
		m_Inherit_Pipe_Handle_fnptr = in_Inherit_Pipe_Handle_fnptr;
#endif
		m_Read_Pipe_fnptr = in_Read_Pipe_fnptr;
		m_Write_Pipe_fnptr = in_Write_Pipe_fnptr;
		m_Close_Pipe_fnptr = in_Close_Pipe_fnptr;
	}

		// These functions all have the same interface as the corresponding
		// daemonCore functions.

	DaemonCore *m_daemonCore;
	Register_Socket_fnptr m_Register_Socket_fnptr;
	Cancel_Socket_fnptr m_Cancel_Socket_fnptr;
	CallSocketHandler_fnptr m_CallSocketHandler_fnptr;
	CallCommandHandler_fnptr m_CallCommandHandler_fnptr;
	HandleReqAsync_fnptr m_HandleReqAsync_fnptr;
	Register_DataPtr_fnptr m_Register_DataPtr_fnptr;
	GetDataPtr_fnptr m_GetDataPtr_fnptr;
	Register_Timer_fnptr m_Register_Timer_fnptr;
	Register_PeriodicTimer_fnptr m_Register_PeriodicTimer_fnptr;
	Cancel_Timer_fnptr m_Cancel_Timer_fnptr;
	TooManyRegisteredSockets_fnptr m_TooManyRegisteredSockets_fnptr;
	incrementPendingSockets_fnptr m_incrementPendingSockets_fnptr;
	decrementPendingSockets_fnptr m_decrementPendingSockets_fnptr;
	publicNetworkIpAddr_fnptr m_publicNetworkIpAddr_fnptr;
	Register_Command_fnptr m_Register_Command_fnptr;
	daemonContactInfoChanged_fnptr m_daemonContactInfoChanged_fnptr;
	Create_Named_Pipe_fnptr m_Create_Named_Pipe_fnptr;
	Register_Pipe_fnptr m_Register_Pipe_fnptr;
#ifdef WIN32
	Get_Inherit_Pipe_Handle_fnptr m_Get_Inherit_Pipe_Handle_fnptr;
	Inherit_Pipe_Handle_fnptr m_Inherit_Pipe_Handle_fnptr;
#endif
	Read_Pipe_fnptr m_Read_Pipe_fnptr;
	Write_Pipe_fnptr m_Write_Pipe_fnptr;
	Close_Pipe_fnptr m_Close_Pipe_fnptr;

    int Register_Socket (Stream*              iosock,
                         const char *         iosock_descrip,
                         SocketHandlercpp     handlercpp,
                         const char *         handler_descrip,
                         Service*             s,
                         DCpermission         perm = ALLOW)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Socket_fnptr)(iosock,iosock_descrip,handlercpp,handler_descrip,s,perm);
	}

	int Cancel_Socket( Stream *stream )
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Cancel_Socket_fnptr)(stream);
	}

	void CallSocketHandler( Stream *stream, bool default_to_HandleCommand=false )
	{
		ASSERT(m_daemonCore);
		(m_daemonCore->*m_CallSocketHandler_fnptr)(stream,default_to_HandleCommand);
	}

	int CallCommandHandler( int cmd, Stream *stream, bool delete_stream=true )
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_CallCommandHandler_fnptr)(cmd,stream,delete_stream);
	}

	void HandleReqAsync(Stream *stream)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_HandleReqAsync_fnptr)(stream);
	}


    int Register_DataPtr( void *data )
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_DataPtr_fnptr)(data);
	}
    void *GetDataPtr()
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_GetDataPtr_fnptr)();
	}
    int Register_Timer (unsigned     deltawhen,
                        TimerHandlercpp handler,
                        const char * event_descrip, 
                        Service*     s = NULL)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Timer_fnptr)(
			deltawhen,
			handler,
			event_descrip,
			s);
	}
    int Register_Timer (unsigned     deltawhen,
						unsigned     period,
                        TimerHandlercpp handler,
                        const char * event_descrip, 
                        Service*     s = NULL)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_PeriodicTimer_fnptr)(
			deltawhen,
			period,
			handler,
			event_descrip,
			s);
	}
    int Cancel_Timer (int id)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Cancel_Timer_fnptr)( id );
	}
	bool TooManyRegisteredSockets(int fd=-1,MyString *msg=NULL,int num_fds=1)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_TooManyRegisteredSockets_fnptr)(fd,msg,num_fds);
	}

	bool isEnabled()
	{
		return m_daemonCore != NULL;
	}

	void incrementPendingSockets() {
		ASSERT(m_daemonCore);
		(m_daemonCore->*m_incrementPendingSockets_fnptr)();
	}

	void decrementPendingSockets() {
		ASSERT(m_daemonCore);
		(m_daemonCore->*m_decrementPendingSockets_fnptr)();
	}

	const char* publicNetworkIpAddr(void) {
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_publicNetworkIpAddr_fnptr)();
	}

    int Register_Command (int             command,
                          char const*     com_descrip,
                          CommandHandler  handler, 
                          char const*     handler_descrip,
                          Service *       s                = NULL,
                          DCpermission    perm             = ALLOW,
                          int             dprintf_flag     = D_COMMAND,
						  bool            force_authentication = false)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Command_fnptr)(command,com_descrip,handler,handler_descrip,s,perm,dprintf_flag,force_authentication);
	}

	void daemonContactInfoChanged() {
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_daemonContactInfoChanged_fnptr)();
	}

	int Create_Named_Pipe(
		int *pipe_ends,
		bool can_register_read,
		bool can_register_write,
		bool nonblocking_read,
		bool nonblocking_write,
		unsigned int psize,
		const char* pipe_name)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Create_Named_Pipe_fnptr)(pipe_ends, can_register_read, can_register_write, nonblocking_read, nonblocking_write, psize, pipe_name);
	}

	int Register_Pipe(
		int pipe_end,
		const char* pipe_descrip,
		PipeHandlercpp handlercpp,
		const char *handler_descrip,
		Service* s)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Register_Pipe_fnptr)(pipe_end, pipe_descrip, handlercpp, handler_descrip, s, HANDLE_READ, ALLOW);
	}

	int Read_Pipe(int pipe_end, void* buffer, int len)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Read_Pipe_fnptr)(pipe_end, buffer, len);
	}

	int Write_Pipe(int pipe_end, const void* buffer, int len)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Write_Pipe_fnptr)(pipe_end, buffer, len);
	}

	int Close_Pipe(int pipe_end)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Close_Pipe_fnptr)(pipe_end);
	}

#ifdef WIN32
	HANDLE Get_Inherit_Pipe_Handle(int pipe_end)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Get_Inherit_Pipe_Handle_fnptr)(pipe_end);
	}

	int Inherit_Pipe_Handle(HANDLE pipe_handle, bool write, bool overlapping, bool nonblocking, int psize)
	{
		ASSERT(m_daemonCore);
		return (m_daemonCore->*m_Inherit_Pipe_Handle_fnptr)(pipe_handle, write, overlapping, nonblocking, psize);
	}
#endif
};

extern DaemonCoreSockAdapterClass daemonCoreSockAdapter;

#endif
