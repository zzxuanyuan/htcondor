/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#ifndef _CONDOR_TDMAN_H_
#define _CONDOR_TDMAN_H_

#include "../condor_transferd/condor_td.h"
#include "HashTable.h"

// This holds the status for a particular transferd
enum TDMode {
	// The representative object of the transfer daemon has been made, but 
	// the work to invoke it hasn't been completed yet.
	TD_PRE_INVOKED,
	// the transferd has been invoked, but hasn't registered
	TD_INVOKED,
	// the transferd has been registered and is avialable for use
	TD_REGISTERED,
	// someone has come back to the schedd and said the registered transferd
	// is not connectable or wasn't there, or whatever.
	TD_MIA
};

// identification of a transferd for continuation purposes across registered
// callback funtions.
class TDUpdateContinuation
{
	public:
		TDUpdateContinuation(MyString s, MyString f, MyString i, MyString n)
		{
			sinful = s;
			fquser = f;
			id = i;
			name = n;
		}

		TDUpdateContinuation(char *s, char *f, char *i, char *n)
		{
			sinful = s;
			fquser = f;
			id = i;
			name = n;
		}

		// sinful string of the td
		MyString sinful;
		// fully qualified user the td is running as
		MyString fquser;
		// the id string the schedd gave to the ransferd
		MyString id;
		// the registration name for the socket handler
		MyString name;
};

// This represents the invocation, and current status, of a transfer daemon
class TransferDaemon
{
	public:
		TransferDaemon(MyString fquser, MyString id, TDMode status);
		~TransferDaemon();

		// This transferd had been started on behalf of a fully qualified user
		// This records who that user is.
		void set_fquser(MyString fquser);
		MyString get_fquser(void);

		// Set the specific ID string associated with this td.
		void set_id(MyString id);
		MyString get_id(void);

		// The schedd changes the state of this object according to what it
		// knows about the status of the daemon itself.
		void set_status(TDMode s);
		TDMode get_status(void);

		// To whom should this td report?
		void set_schedd_sinful(MyString sinful);
		MyString get_schedd_sinful(void);

		// Who is this transferd (after it registers)
		void set_sinful(MyString sinful);
		MyString get_sinful(void);

		// The socket the schedd uses to listen to updates from the td.
		// This is also what was the original registration socket.
		void set_update_sock(ReliSock *update_sock);
		ReliSock* get_update_sock(void);

		// The socket one must use to send to a new transfer request to
		// this daemon.
		void set_treq_sock(ReliSock *treq_sock);
		ReliSock* get_treq_sock(void);

		// Cache the ReliSock to the client, so the user of this object can 
		// wait until the td starts up and registers.
		void set_client_sock(ReliSock *client_sock);
		ReliSock* get_client_sock(void);

		// If I happen to have a transfer request when making this object,
		// store them here until the transferd registers and I can deal 
		// with it then. This object assumes ownership of the memory unless
		// false is returned.
		bool add_transfer_request(TransferRequest *treq);

		// write all of the pending requests to the treq socket.
		// It returns true if all of the requests had been pushed, and
		// false otherwise. If false, then the requets not successfully
		// pushed are still ready to be pushed later. You don't know why
		// it was false though, it could be due to no treq connection or
		// a failure of writing to the socket. Returns true if nothing to be
		// pushed.
		// NOTE: This may block.
		bool push_transfer_requests(void);

	private:
		// The sinful string of this transferd after registration
		MyString m_sinful;

		// the owner of the transferd
		MyString m_fquser;

		// The id string associated with this td
		MyString m_id;

		// The schedd to which this td must report
		MyString m_schedd_sinful;

		// current status about this transferd I requested
		TDMode m_status;

		// Storage of Transfer Requests until I can pass them to the transfer
		// daemon itself.
		SimpleList<TransferRequest*> m_treqs;

		// The registration socket that the schedd also receives updates on.
		ReliSock *m_update_sock; 

		// The socket the schedd initiated to send treqs to the td
		ReliSock *m_treq_sock;

		// The submit client socket, stashed for when this td registers itself
		ReliSock *m_client_sock;
};

class TDMan
{
	public:
		TDMan();
		~TDMan();

		// returns NULL if no td is currently available. Returns the
		// transfer daemon object invoked for this user. If no such transferd
		// exists, then return NULL;
		TransferDaemon* find_td_by_user(MyString fquser);

		// when the td registers itself, figure out to which of my objects its
		// identity string pairs.
		TransferDaemon* find_td_by_ident(MyString id);

		// I've determined that I have to create a transfer daemon, so have the
		// caller set up a TransferDaemon object, and then I'll be responsible
		// for starting it.
		// The caller has specified the fquser and id in the object.
		// This function will dig around in the td object and fire up a 
		// td according to what it finds.
		void invoke_a_td(TransferDaemon *td);

	private:
		// This is where I store the table of transferd objects, each
		// representing the status and connection to a transferd.
		// Key: fquser, Value: transferd
		HashTable<MyString, TransferDaemon*> *m_td_table;

		// Store an association between an id given to a specific transferd
		// and the user that id ultimately identifies.
		// Key: id, Value: fquser
		HashTable<MyString, MyString> *m_id_table;

		// a table of pids associated with running transferds so reapers
		// can do their work, among other things.
		HashTable<long, TransferDaemon*> *m_td_pid_table;
	
	// NOTE: When we get around to implementing multiple tds per user with
	// different id strings, then find_any_td must return a list of tds and
	// the hash tables value must be that corresponding list.
};

#endif /* _CONDOR_TDMAN_H_ */








