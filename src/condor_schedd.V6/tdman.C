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

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_daemon_client.h"
#include "dc_transferd.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "scheduler.h"
#include "basename.h"

TransferDaemon::TransferDaemon(MyString fquser, MyString id, TDMode status) :
	m_treqs_in_progress(200, hashFuncMyString)
{
	m_fquser = fquser;
	m_id = id;
	m_status = status;

	m_sinful = "";
	m_update_sock = NULL;
	m_treq_sock = NULL;
}

TransferDaemon::~TransferDaemon()
{
	// XXX TODO
}

void
TransferDaemon::clear(void)
{
	m_status = TD_PRE_INVOKED;
	m_sinful = "";
	m_update_sock = NULL;
	m_treq_sock = NULL;
}

void
TransferDaemon::set_fquser(MyString fquser)
{
	m_fquser = fquser;
}

MyString
TransferDaemon::get_fquser(void)
{
	return m_fquser;
}

void
TransferDaemon::set_id(MyString id)
{
	m_id = id;
}

MyString
TransferDaemon::get_id(void)
{
	return m_id;
}

void
TransferDaemon::set_status(TDMode tds)
{
	m_status = tds;
}

TDMode
TransferDaemon::get_status()
{
	return m_status;
}

void
TransferDaemon::set_schedd_sinful(MyString sinful)
{
	m_schedd_sinful = sinful;
}

MyString
TransferDaemon::get_schedd_sinful()
{
	return m_schedd_sinful;
}

void
TransferDaemon::set_sinful(MyString sinful)
{
	m_sinful = sinful;
}

void
TransferDaemon::set_sinful(char *sinful)
{
	MyString sin;
	sin = sinful;

	set_sinful(sin);
}

MyString
TransferDaemon::get_sinful()
{
	return m_sinful;
}

void
TransferDaemon::set_update_sock(ReliSock *update_sock)
{
	m_update_sock = update_sock;
}

ReliSock*
TransferDaemon::get_update_sock(void)
{
	return m_update_sock;
}

void
TransferDaemon::set_treq_sock(ReliSock *treq_sock)
{
	m_treq_sock = treq_sock;
}

ReliSock*
TransferDaemon::get_treq_sock(void)
{
	return m_treq_sock;
}

bool
TransferDaemon::add_transfer_request(TransferRequest *treq)
{
	return m_treqs.Append(treq);
}

bool
TransferDaemon::push_transfer_requests(void)
{
	TransferRequest *treq = NULL;
	int ret;
	MyString capability;
	MyString rej_reason;
	char *encap = "ENCAPSULATION_METHOD_OLD_CLASSADS\n";
	ClassAd respad;
	int invalid;

	dprintf(D_ALWAYS, 
		"Entering TransferDaemon::push_transfer_requests()\n");

	if (m_treq_sock == NULL) {
		EXCEPT("TransferDaemon::push_transfer_requests(): TD object was not "
				"up to have a real daemon backend prior to pushing requests");
	}

	// process all pending requests at once.
	m_treqs.Rewind();
	while(m_treqs.Next(treq))
	{
		/////////////////////////////////////////////////
		// Have the schedd do any last minute setup for this request 
		// before giving it to the transferd.
		/////////////////////////////////////////////////

		// Here the schedd must dig around in the treq for the procids
		// or whatever and then convert them to jobads which it puts
		// back into the treq.
		ret = treq->call_pre_push_callback(treq, this);

		/////////////////////////////////////////////////
		// Depending what the schedd said in the return code, do we continue
		// processing this request normally, or do we forget about it.
		/////////////////////////////////////////////////
		switch(ret) {
			case TREQ_ACTION_CONTINUE:
				// the pre callback did whatever it wanted to, and now says to
				// continue the process of handling this request.
				break;

			case TREQ_ACTION_TERMINATE:
				// Remove this request from consideration by this transfer 
				// daemon.
				// It is assumed the pre callback took control of the memory
				// and had already deleted/saved it for later requeue/etc
				// and said something to the client about it.
				m_treqs.DeleteCurrent();

				// don't contact the transferd about this request, just go to
				// the next request instead.
				continue;

				break;

			default:
				EXCEPT("TransferDaemon::push_transfer_requests(): Programmer "
					"Error. Unknown pre TreqAction\n");
				break;
		}

		/////////////////////////////////////////////////
		// Send the request to the transferd
		/////////////////////////////////////////////////

		// Let's use the only encapsulation protocol we have at the moment.
		m_treq_sock->encode();
		m_treq_sock->code(encap);
		m_treq_sock->eom();

		// This only puts a small amount of the treq onto the channel. The
		// transferd doesn't need a lot of the info in the schedd's view of
		// this information.
		treq->put(m_treq_sock);
		m_treq_sock->eom();

		/////////////////////////////////////////////////
		// Now the transferd will do work on the request, assigning it a 
		// capability, etc, etc, etc
		/////////////////////////////////////////////////

		m_treq_sock->decode();

		/////////////////////////////////////////////////
		// Get the classad back form the transferd which represents the
		// capability assigned to this classad and the file transfer protocols
		// with which the transferd is willing to speak to the submitter.
		// Also, there could be errors and what not in the ad, so the
		// post_push callback better inspect it and see if it likes what it
		// say.
		/////////////////////////////////////////////////

		// This response ad from the transferd about the request just give
		// to it will contain:
		//
		//	ATTR_TREQ_INVALID_REQUEST (set to true)
		//	ATTR_TREQ_INVALID_REASON
		//
		//	OR
		//
		//	ATTR_TREQ_INVALID_REQUEST (set to false)
		//	ATTR_TREQ_CAPABILITY
		//
		respad.initFromStream(*m_treq_sock);
		m_treq_sock->eom();

		/////////////////////////////////////////////////
		// Fix up the treq with what the transferd said.
		/////////////////////////////////////////////////

		respad.LookupInteger(ATTR_TREQ_INVALID_REQUEST, invalid);
		if (invalid == FALSE) {
			dprintf(D_ALWAYS, "Transferd said request was valid.\n");

			respad.LookupString(ATTR_TREQ_CAPABILITY, capability);
			treq->set_capability(capability);

			// move it from the original enqueue list to the in progress hash
			// keyed by its capability
			m_treqs.DeleteCurrent();
			m_treqs_in_progress.insert(treq->get_capability(), treq);

		} else {
			dprintf(D_ALWAYS, "Transferd said request was invalid.\n");

			// record into the treq that the transferd rejected it.
			treq->set_rejected(true);
			respad.LookupString(ATTR_TREQ_INVALID_REASON, rej_reason);
			treq->set_rejected_reason(rej_reason);
		}

		/////////////////////////////////////////////////
		// Here the schedd will take the capability and tell the waiting
		// clients the answer and whatever else it wants to do.
		// WARNING: The schedd may decide to get rid of this request if the
		// transferd rejected it.
		/////////////////////////////////////////////////

		ret = treq->call_post_push_callback(treq, this);

		/////////////////////////////////////////////////
		// Depending what the schedd said in the return code, do we continue
		// processing this request normally, or do we forget about it.
		/////////////////////////////////////////////////
		switch(ret) {
			case TREQ_ACTION_CONTINUE:
				// the post callback did whatever it wanted to, and now says to
				// continue the process of handling this request.
				break;

			case TREQ_ACTION_TERMINATE:
				// It is assumed the post callback took control of the memory
				// and had already deleted/saved it for later requeue/etc
				// and said something to the client about it.
				m_treqs.DeleteCurrent();

				// XXX This is a complicated action to implement, since it
				// means we have to contact the transferd and tell it to
				// abort the request.

				EXCEPT("TransferDaemon::push_transfer_requests(): NOT "
					"IMPLEMENTED: aborting an treq after the push to the "
					"transferd. To implement this functionality, you must "
					"contact the transferd here and have it also abort "
					"the request.");

				break;

			default:
				EXCEPT("TransferDaemon::push_transfer_requests(): Programmer "
					"Error. Unknown post TreqAction\n");
				break;
		}
	}

	dprintf(D_ALWAYS,
		"Leaving TransferDaemon::push_transfer_requests()\n");

	return true;
}

bool
TransferDaemon::update_transfer_request(ClassAd *update)
{
	MyString cap;
	TransferRequest *treq = NULL;
	int ret;
	TransferDaemon *td = NULL;

	dprintf(D_ALWAYS,
		"Entering TransferDaemon::update_transfer_request()\n");
	
	////////////////////////////////////////////////////////////////////////
	// The update ad contains the capability for a treq, so let's find the
	// treq.
	////////////////////////////////////////////////////////////////////////
	update->LookupString(ATTR_TREQ_CAPABILITY, cap);

	// If I'm getting an update for it, it better be in progress.
	if (m_treqs_in_progress.lookup(cap, treq) != 0) {
		EXCEPT("TransferDaemon::update_transfer_request(): Programmer error. "
			"Updating treq not found in progress table hash!");
	}

	// let the schedd look at the update and figure out if it wants to do
	// anything with it.
	dprintf(D_ALWAYS, "TransferDaemon::update_transfer_request(): "
		"Calling schedd update callback\n");
	ret = treq->call_update_callback(treq, td, update);

	/////////////////////////////////////////////////
	// Depending what the schedd said in the return code, do we continue
	// processing this request normally, or do we forget about it.
	/////////////////////////////////////////////////
	switch(ret) {
		case TREQ_ACTION_CONTINUE:
			// Don't delete the request from our in progress table, and assume
			// there will be more updates for the schedd's update handler
			// to process. In effect, do nothing and wait.
			break;

		case TREQ_ACTION_TERMINATE:
			// It is assumed the update callback took control of the memory
			// and had already deleted/saved it for later requeue/etc.
			// For our purposes, we remove it from consideration from our table
			// which in effect means it is removed from the transfer daemons
			// responsibility.
			ASSERT(m_treqs_in_progress.remove(cap) == 0);

			break;

		default:
			EXCEPT("TransferDaemon::update_transfer_request(): Programmer "
				"Error. Unknown update TreqAction\n");
			break;
	}

	dprintf(D_ALWAYS,
		"Leaving TransferDaemon::update_transfer_request()\n");
	
	return TRUE;
}



////////////////////////////////////////////////////////////////////////////
// The transfer daemon manager class
////////////////////////////////////////////////////////////////////////////

TDMan::TDMan()
{
	m_td_table = 
		new HashTable<MyString, TransferDaemon*>(200, hashFuncMyString);
	m_id_table = 
		new HashTable<MyString, MyString>(200, hashFuncMyString);
	m_td_pid_table = 
		new HashTable<long, TransferDaemon*>(200, hashFuncLong);
}

TDMan::~TDMan()
{
	/* XXX fix up to go through them and delete everything inside of them. */
	delete m_td_table;
	delete m_id_table;
	delete m_td_pid_table;
}

void TDMan::register_handlers(void)
{
	 daemonCore->Register_Command(TRANSFERD_REGISTER,
			"TRANSFERD_REGISTER",
			(CommandHandlercpp)&TDMan::transferd_registration,
			"transferd_registration", this, WRITE);
}

TransferDaemon*
TDMan::find_td_by_user(MyString fquser)
{
	int ret;
	TransferDaemon *td = NULL;

	ret = m_td_table->lookup(fquser, td);

	if (ret != 0) {
		// not found
		return NULL;
	}

	return td;
}

TransferDaemon*
TDMan::find_td_by_user(char *fquser)
{
	MyString str;

	str = fquser;

	return find_td_by_user(str);
}

TransferDaemon*
TDMan::find_td_by_ident(MyString id)
{
	int ret;
	MyString fquser;
	TransferDaemon *td = NULL;

	// look up the fquser associated with this id
	ret = m_id_table->lookup(id, fquser);
	if (ret != 0) {
		// not found
		return NULL;
	}

	// look up the transferd for that user.
	ret = m_td_table->lookup(fquser, td);
	if (ret != 0) {
		// not found
		return NULL;
	}

	return td;
}

TransferDaemon*
TDMan::find_td_by_ident(char *ident)
{
	MyString id;

	id = ident;

	return find_td_by_ident(id);
}


bool
TDMan::invoke_a_td(TransferDaemon *td)
{
	TransferDaemon *found_td = NULL;
	MyString found_id;
	ArgList args;
	char *td_path = NULL;
	int rid;
	pid_t pid;
	MyString args_display;

	ASSERT(td != NULL);

	// I might be asked to reinvoke something that may have died
	// previously, but I better not invoke one already registered.

	switch(td->get_status()) {
		case TD_PRE_INVOKED:
			// all good
			break;
		case TD_INVOKED:
			// daemon died after invocation, but before registering.
			break;
		case TD_REGISTERED:
			// disallow reinvocation due to daemon being considered alive.
			return false;
			break;
		case TD_MIA:
			// XXX do I need this?
			break;
			
	}

	//////////////////////////////////////////////////////////////////////
	// Store this object into the internal tables

	// I might be calling this again with the same td, so don't insert it
	// twice into the tables...

	// store it into the general activity table
	if (m_td_table->lookup(td->get_fquser(), found_td) != 0) {
		m_td_table->insert(td->get_fquser(), td);
	}

	// build the association with the id
	if (m_id_table->lookup(td->get_id(), found_id) != 0) {
		m_id_table->insert(td->get_id(), td->get_fquser());
	}


	//////////////////////////////////////////////////////////////////////
	// What executable am I going to be running?

	td_path = param("TRANSFERD");
	if (td_path == NULL) {
		EXCEPT("TRANSFERD must be defined in the config file!");
	}

	//////////////////////////////////////////////////////////////////////
	// What is the argument list for the transferd?

	// what should my argv[0] be?
	args.AppendArg(condor_basename(td_path));

	// This is a daemoncore process
	args.AppendArg("-f");

	// report back to this schedd
	args.AppendArg("--schedd");
	args.AppendArg(daemonCore->InfoCommandSinfulString());

	// what id does it have?
	args.AppendArg("--id");
	args.AppendArg(td->get_id());

	//////////////////////////////////////////////////////////////////////
	// Which reaper should handle the exiting of this program

	// set up a reaper to handle the exiting of this daemon
	rid = daemonCore->Register_Reaper("transferd_reaper",
		(ReaperHandlercpp)&TDMan::transferd_reaper,
		"transferd_reaper",
		this);

	//////////////////////////////////////////////////////////////////////
	// Create the process

	args.GetArgsStringForDisplay(&args_display);
	dprintf(D_ALWAYS, "Invoking for user '%s' a transferd: %s\n", 
		td->get_fquser().Value(),
		args_display.Value());

	// XXX needs to be the user, not root!
	pid = daemonCore->Create_Process( td_path, args, PRIV_ROOT, rid );
	dprintf(D_ALWAYS, "Created transferd with pid %d\n", pid);

	if (pid == FALSE) {
		// XXX TODO
		EXCEPT("failed to create transferd process.");
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////
	// Perform associations

	// consistancy check. Pid must not be in the table.
	ASSERT(m_td_pid_table->lookup((long)pid, found_td) != 0);

	// insert into the pid table for TDMann reaper to use...
	m_td_pid_table->insert((long)pid, td);
	
	free(td_path);

	td->set_status(TD_INVOKED);

	return TRUE;
}

void
TDMan::refuse(Stream *s)
{
	s->encode();
	s->put( NOT_OK );
	s->eom();
}

// the reaper for when a transferd goes away or dies.
int
TDMan::transferd_reaper(int pid, int status) 
{
	dprintf(D_ALWAYS, "TDMan: Reaped transferd %d with status %d\n", 
		pid, status);
	
	// XXX find the td object associated with this td and call the reaper
	// for it the schedd supplied.

	return TRUE;
}


// The handler for TRANSFERD_REGISTER.
// This handler checks to make sure this is a valid transferd. If valid,
// the schedd may then shove over a transfer request to the transferd on the
// open relisock. The relisock is kept open the life of the transferd, and
// the schedd sends new transfer requests whenever it wants to.
int 
TDMan::transferd_registration(int cmd, Stream *sock)
{
	ReliSock *rsock = (ReliSock*)sock;
	int reply;
	char *td_sinful = NULL;
	char *td_id = NULL;
	const char *fquser = NULL;
	TDUpdateContinuation *tdup = NULL;
	TransferDaemon *td = NULL;

	cmd = cmd; // quiet the compiler

	dprintf(D_ALWAYS, "Entering TDMan::transferd_registration()\n");

	dprintf(D_ALWAYS, "Got TRANSFERD_REGISTER message!\n");

	rsock->decode();

	///////////////////////////////////////////////////////////////
	// make sure we are authenticated
	///////////////////////////////////////////////////////////////
	if( ! rsock->isAuthenticated() ) {
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! rsock->authenticate(methods.Value(), &errstack) ) {
				// we failed to authenticate, we should bail out now
				// since we don't know what user is trying to perform
				// this action.
				// TODO: it'd be nice to print out what failed, but we
				// need better error propagation for that...
			errstack.push( "SCHEDD::TDMan", 42,
					"Failure to register transferd - Authentication failed" );
			dprintf( D_ALWAYS, "transferd_registration() aborting: %s\n",
					 errstack.getFullText() );
			refuse( rsock );
			dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
			return CLOSE_STREAM;
		}
	}	

	///////////////////////////////////////////////////////////////
	// Figure out who this user is.
	///////////////////////////////////////////////////////////////

	fquser = rsock->getFullyQualifiedUser();
	if (fquser == NULL) {
		dprintf(D_ALWAYS, "Transferd identity is unverifiable. Denied.\n");
		refuse(rsock);
		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	///////////////////////////////////////////////////////////////
	// Decode the registration message from the transferd
	///////////////////////////////////////////////////////////////

	rsock->decode();

	// Get the sinful string of the transferd
	rsock->code(td_sinful);
	rsock->eom();

	// Get the id string I requested the transferd to have so I can figure out
	// which request I made matches it.
	rsock->code(td_id);
	rsock->eom();

	dprintf(D_ALWAYS, "Transferd %s, id: %s, owned by '%s' "
		"attempting to register!\n",
		td_sinful, td_id, fquser);

	rsock->encode();

	// send back a good reply
	reply = 1;
	rsock->code(reply);
	rsock->eom();

	///////////////////////////////////////////////////////////////
	// Determine if I requested a transferd for this identity. Close if not.
	///////////////////////////////////////////////////////////////

	// See if I have a transferd by that id
	td = find_td_by_ident(td_id);
	if (td == NULL) {
		// guess not, refuse it
		dprintf(D_ALWAYS, 
			"Did not request a transferd with that id for any user. "
			"Refuse.\n");
		refuse(rsock);
		free(td_sinful);
		free(td_id);
		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	// then see if the user of that td jives with how it authenticated
	if (td->get_fquser() != fquser) {
		// guess not, refuse it
		dprintf(D_ALWAYS, 
			"Did not request a transferd with that id for this specific user. "
			"Refuse.\n");
		refuse(rsock);
		free(td_sinful);
		free(td_id);
		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	// is it in the TD_INVOKED state?
	if (td->get_status() != TD_INVOKED) {
		// guess not, refuse it
		dprintf(D_ALWAYS, 
			"Transferd for user not in TD_PRE_INVOKED state. Refuse.\n");
		refuse(rsock);
		free(td_sinful);
		free(td_id);
		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	///////////////////////////////////////////////////////////////
	// Set up some parameters in the TD object which represent
	// the backend of this object (which is a true daemon)
	///////////////////////////////////////////////////////////////

	td->set_status(TD_REGISTERED);
	td->set_sinful(td_sinful);
	td->set_update_sock(rsock);

	///////////////////////////////////////////////////////////////
	// Call back to the transferd so we have two asynch sockets, one for
	// new transfer requests to go down, the other for updates to come back.
	///////////////////////////////////////////////////////////////

	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //

	// At this point, we connect back to the transferd, so this means that in 
	// the protocol, the registration function in the transferd must now have
	// ended and the transferd has gone back to daemonCore. Otherwise, 
	// deadlock. We stash the socket which we will be sending transfer
	// requests down into the td object.

	dprintf(D_ALWAYS, "Registration is valid...\n");
	dprintf(D_ALWAYS, "Creating TransferRequest channel to transferd.\n");
	CondorError errstack;
	ReliSock *td_req_sock = NULL;
	DCTransferD dctd(td_sinful);

	// XXX This connect in here should be non-blocking....
	if (dctd.setup_treq_channel(&td_req_sock, 20, &errstack) == false) {
		dprintf(D_ALWAYS, "Refusing errant transferd.\n");
		refuse(rsock);
		dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
		return CLOSE_STREAM;
	}

	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// WARNING WARNING WARNING //
	// Transferd must have gone back to daemoncore at this point in the
	// protocol.

	td->set_treq_sock(td_req_sock);
	dprintf(D_ALWAYS, 
		"TransferRequest channel created. Transferd appears stable.\n");

	///////////////////////////////////////////////////////////////
	// Register a call back socket for future updates from this transferd
	///////////////////////////////////////////////////////////////

	// Now, let's give a good name for the update socket.
	MyString sock_id;
	sock_id += "<Update-Socket-For-TD-";
	sock_id += td_sinful;
	sock_id += "-";
	sock_id += fquser;
	sock_id += "-";
	sock_id += td_id;
	sock_id += ">";

	daemonCore->Register_Socket(sock, (char*)sock_id.Value(),
		(SocketHandlercpp)&TDMan::transferd_update,
		"TDMan::transferd_update", this, ALLOW);
	
	// stash an identifier with the registered socket so I can find this
	// transferd later when this socket gets an update. I can't just shove
	// the transferd pointer here since it might have been removed by other
	// handlers if they determined the daemon went away. Instead I'll push an 
	// identifier so I can see if it still exists in the pool before messing
	// with it.
	tdup = new TDUpdateContinuation(td_sinful, fquser, td_id, sock_id.Value());
	ASSERT(tdup);

	// set up the continuation for TDMan::transferd_update()
	daemonCore->Register_DataPtr(tdup);

	free(td_sinful);
	free(td_id);

	///////////////////////////////////////////////////////////////
	// Push any pending requests for this transfer daemon.
	// This has the potential to call callback into the schedd.
	///////////////////////////////////////////////////////////////

	dprintf(D_ALWAYS, 
		"TDMan::transferd_registration() pushing queued requests\n");

	td->push_transfer_requests();

	///////////////////////////////////////////////////////////////
	// If any registration callback exists, then call it so the user of
	// the factory object knows that the registration happened
	///////////////////////////////////////////////////////////////
	// XXX TODO

	dprintf(D_ALWAYS, "Leaving TDMan::transferd_registration()\n");
	return KEEP_STREAM;
}


// When a transferd finishes sending some files, it informs the schedd when the
// transfer was correctly sent. NOTE: Maybe add when the transferd thinks there
// are problems, like files not found and stuff like that.
int 
TDMan::transferd_update(Stream *sock)
{
	ReliSock *rsock = (ReliSock*)sock;
	TDUpdateContinuation *tdup = NULL;
	ClassAd update;
	MyString cap;
	MyString status;
	MyString reason;
	TransferDaemon *td = NULL;

	/////////////////////////////////////////////////////////////////////////
	// Grab the continuation from the registration handler
	/////////////////////////////////////////////////////////////////////////
	
	// We don't delete this pointer until we know this socket is closed...
	// This allows us to recycle it across many updates from the same td.
	tdup = (TDUpdateContinuation*)daemonCore->GetDataPtr();
	ASSERT(tdup != NULL);

	dprintf(D_ALWAYS, "Transferd update from: addr(%s) fquser(%s) id(%s)\n", 
		tdup->sinful.Value(), tdup->fquser.Value(), tdup->id.Value());

	/////////////////////////////////////////////////////////////////////////
	// Get the resultant status classad from the transferd
	/////////////////////////////////////////////////////////////////////////

	// grab the classad from the transferd
	if (update.initFromStream(*rsock) == 0) {
		// Hmm, couldn't get the update, clean up shop.
		dprintf(D_ALWAYS, "Update socket was closed. Transferd has died!\n");
		delete tdup;
		daemonCore->SetDataPtr(NULL);
		return CLOSE_STREAM;
	}
	rsock->eom();

	update.LookupString(ATTR_TREQ_CAPABILITY, cap);
	update.LookupString(ATTR_TREQ_UPDATE_STATUS, status);
	update.LookupString(ATTR_TREQ_UPDATE_REASON, reason);

	dprintf(D_ALWAYS, "Update was: cap = %s, status = %s,  reason = %s\n",
		cap.Value(), status.Value(), reason.Value());

	/////////////////////////////////////////////////////////////////////////
	// Find the matching transfer daemon with the id in question.
	/////////////////////////////////////////////////////////////////////////

	td = find_td_by_ident(tdup->id);
	// XXX This is a little strange. It would mean that somehow the 
	// td object got deleted by someone, but we didn't deal with the fact
	// we could still get updates for it. I'd consider this a consistancy
	// check that we have a valid td object when a daemon is in existence
	// and it better be true.
	ASSERT(td != NULL);

	/////////////////////////////////////////////////////////////////////////
	// Pass the update ad directly to the transfer daemon and let it deal
	// with it locally by calling the right schedd callbacks and such.
	/////////////////////////////////////////////////////////////////////////

	td->update_transfer_request(&update);

	/////////////////////////////////////////////////////////////////////////
	// Keep the stream for the next update
	/////////////////////////////////////////////////////////////////////////

	return KEEP_STREAM;
}
