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
#include "condor_config.h"
#include "condor_debug.h"
#include "qmgmt.h"
#include "condor_qmgr.h"
#include "scheduler.h"

TransferDaemon::TransferDaemon(MyString fquser, MyString id, TDMode status)
{
	m_fquser = fquser;
	m_id = id;
	m_status = status;
}

TransferDaemon::~TransferDaemon()
{
	// XXX TODO
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

void
TransferDaemon::set_client_sock(ReliSock *client_sock)
{
	m_client_sock = client_sock;
}

ReliSock*
TransferDaemon::get_client_sock(void)
{
	return m_client_sock;
}

bool
TransferDaemon::add_transfer_request(TransferRequest *treq)
{
	return m_treqs.Append(treq);
}

bool
TransferDaemon::push_transfer_requests(void)
{
	// XXX TODO
	return false;
}

////////////////////////////////////////////////////////////////////////////
// The transfer daemon manager class
////////////////////////////////////////////////////////////////////////////

TDMan::TDMan()
{
	m_td_table = 
		new HashTable<MyString, TransferDaemon*>(20, hashFuncMyString);
	m_id_table = 
		new HashTable<MyString, MyString>(20, hashFuncMyString);
	m_td_pid_table = 
		new HashTable<long, TransferDaemon*>(20, hashFuncLong);
}

TDMan::~TDMan()
{
	/* XXX fix up to go through them and delete everything inside of them. */
	delete m_td_table;
	delete m_id_table;
	delete m_td_pid_table;
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

void
TDMan::invoke_a_td(TransferDaemon *td)
{
	
}




// The handler for TRANSFERD_REGISTER.
// This handler checks to make sure this is a valid transferd. If valid,
// the schedd may then shove over a transfer request to the transferd on the
// open relisock. The relisock is kept open the life of the transferd, and
// the schedd sends new transfer requests whenever it wants to.
int 
Scheduler::transferd_registration(int cmd, Stream *sock)
{
	ReliSock *rsock = (ReliSock*)sock;
	int reply;
	char *td_sinful = NULL;
	char *td_id = NULL;
	const char *fquser = NULL;
	TDUpdateContinuation *tdup = NULL;

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
			errstack.push( "SCHEDD", 42,
					"Failure to register transferd - Authentication failed" );
			dprintf( D_ALWAYS, "transferd_registration() aborting: %s\n",
					 errstack.getFullText() );
			refuse( rsock );
			return FALSE;
		}
	}	

	///////////////////////////////////////////////////////////////
	// Verify that this user is actually someone who can write to the job queue.
	///////////////////////////////////////////////////////////////

	fquser = rsock->getFullyQualifiedUser();
	if (fquser == NULL) {
		dprintf(D_ALWAYS, "Transferd identity is unverifiable. Denied.\n");
		refuse(rsock);
	}

	///////////////////////////////////////////////////////////////
	// Determine if I requested a transferd for this identity. Close if not.
	///////////////////////////////////////////////////////////////
	
	// TODO

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

	dprintf(D_ALWAYS, "Transferd %s, id: %s, owned by '%s' is registered!\n",
		td_sinful, td_id, rsock->getFullyQualifiedUser());

	rsock->encode();

	// send back a good reply
	reply = 1;
	rsock->code(reply);
	rsock->eom();

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
		(SocketHandlercpp)&Scheduler::transferd_update,
		"Scheduler::transferd_update", this, ALLOW);
	
	// stash an identifier with the registered socket so I can find this
	// transferd later when this socket gets an update. I can't just shove
	// the transferd pointer here since it might have been removed by other
	// handlers if they determined the daemon went away. Instead I'll push an 
	// identifier so I can see if it still exists in the pool before messing
	// with it.
	tdup = new TDUpdateContinuation(td_sinful, fquser, td_id, sock_id.Value());
	ASSERT(tdup);

	// set up the continuation for Scheduler::transferd_update()
	daemonCore->Register_DataPtr(tdup);

	free(td_sinful);
	free(td_id);

	return KEEP_STREAM;
}


// When a transferd finishes sending some files, it informs the schedd when the
// transfer was correctly sent. NOTE: Maybe add when the transferd thinks there
// are problems, like files not found and stuff like that.
int 
Scheduler::transferd_update(Stream *sock)
{
	ReliSock *rsock = (ReliSock*)sock;
	TDUpdateContinuation *tdup = NULL;
	ClassAd update;

	// continue the continuation
	tdup = (TDUpdateContinuation*)daemonCore->GetDataPtr();
	ASSERT(tdup);

	dprintf(D_ALWAYS, "Transferd update from: addr(%s) fquser(%s) id(%s)\n", 
		tdup->sinful.Value(), tdup->fquser.Value(), tdup->id.Value());

	// grab the classad from the transferd
	if (update.initFromStream(*rsock) == 0) {
		// Hmm, couldn't get the update, clean up shop.
		dprintf(D_ALWAYS, "Update socket was closed.\n");
		delete tdup;
		daemonCore->SetDataPtr(NULL);
		return CLOSE_STREAM;
	}
	rsock->eom();

	// TODO
	// decode the classad and determine what to do about it
	// jobs waiting for transfer should be removed from being on hold when
	// their transfer is complete.

	return KEEP_STREAM;
}























// XXX propogate the fact that I'm splitting the entire code path if uploading
// and downloading into two completely separate codepaths. I've finished
// just the first functions, and need to propogate it.

// This function is used BOTH for uploading and downloading files to the
// schedd. Which path selected is determined by the command passed to this
// function. This function should really be split into two different handlers,
// one for uploading the spool, and one for downloading it. 

/* In this service function, the client tells the schedd a bunch of jobs
	it would like to upload into a sandbox. The schedd will hold
	open the connection back to the client (potentially across to another
	callback) until it gets a sinful string of a transferd that the
	client may upload its sandbox to.
*/

int
Scheduler::requestSandboxLocation(int mode, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	ExtArray<PROC_ID> *jobs = NULL;
	int i;
	PROC_ID a_job;
	char *peer_version = NULL;
	TransferDaemon *td = NULL;
	MyString rand_id;
	MyString fquser;
	char *tmp = NULL;
	int cluster, proc;

	dprintf(D_ALWAYS, "Entering requestSandboxLocation()\n");

		// make sure this connection is authenticated, and we know who
		// the user is.  also, set a timeout, since we don't want to
		// block long trying to read from our client.   
	rsock->timeout( 10 );  

	////////////////////////////////////////////////////////////////////////
	// Authenticate the socket

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
			errstack.push( "SCHEDD", SCHEDD_ERR_SPOOL_FILES_FAILED,
					"Failure to spool job files - Authentication failed" );
			dprintf( D_ALWAYS, "requestSandBoxLocation() aborting: %s\n",
					 errstack.getFullText() );
			refuse( s );
			return FALSE;
		}
	}	

	rsock->decode();

	////////////////////////////////////////////////////////////////////////
	// read the message from the client about what it wants to transfer

	// The protocol requires the perm handling command to be used and
	// the peer version to be present.

	peer_version = NULL;
	if ( !rsock->code(peer_version) ) {
		dprintf( D_ALWAYS,
			 	"requestSandBoxLocation(): failed to read peer_version\n" );
		refuse(s);
		return FALSE;
	}

	// At this point, we are responsible for deallocating
	// peer_version with free()

	// read the number of jobs involved
	if ( !rsock->code(JobAdsArrayLen) ) {
			dprintf( D_ALWAYS, "requestSandBoxLocation(): "
				 	"failed to read JobAdsArrayLen (%d)\n",
					JobAdsArrayLen );
			refuse(s);
			return FALSE;
	}
	rsock->eom();

	if ( JobAdsArrayLen <= 0 ) {
		dprintf( D_ALWAYS, "requestSandBoxLocation(): "
			 	"read bad JobAdsArrayLen value %d\n", JobAdsArrayLen );
		refuse(s);
		return FALSE;
	}

	dprintf(D_FULLDEBUG,"requestSandBoxLocation(): read JobAdsArrayLen - %d\n",
			JobAdsArrayLen);
	
	jobs = new ExtArray<PROC_ID>;
	ASSERT(jobs);

	// load the procid structures that the client wishes to do transfers for.
	// this will also be given to the transferd by the client in a seperate
	// connection to the transferd to identify the set of transferring files.
	for (i=0; i<JobAdsArrayLen; i++) {
		rsock->code(a_job);
		(*jobs)[i] = a_job;
	}
	rsock->eom();

	////////////////////////////////////////////////////////////////////////
	// Analyze what the client told us to see if it may do such a thing

	// Now analyze what the client told me and validate if they can modify the
	// jobs they told me about.
	setQSock(rsock);	// so OwnerCheck() will work
	for (i=0; i<JobAdsArrayLen; i++) {
		if (!(OwnerCheck((*jobs)[i].cluster,(*jobs)[i].proc))) {
			// Need to tell client about this failure 
		}
	}
	// XXX send back a packet explaining the work is ok, or can't modify jobs?
	unsetQSock();

	////////////////////////////////////////////////////////////////////////
	// construct the transfer request for this job's ads

	TransferRequest *treq = new TransferRequest();

	// set up metadata about the request.
	// set the various things for this request
	treq->set_peer_version(peer_version);
	treq->set_protocol_version(0);
	treq->set_transfer_service("Passive"); // XXX fixme to use enum
	treq->set_num_transfers(JobAdsArrayLen);

	// Get the actual job ads and shove them into the request. The request
	// push for the transfer daemon will fix up the attributes inside of the
	// job ad which explain the transfer times and such.
	for (i=0; i<JobAdsArrayLen; i++) {
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;
		ClassAd * nad = GetJobAd( cluster, proc );
		treq->append_task(nad);
	}

	////////////////////////////////////////////////////////////////////////
	// locate a transferd

	// Ok, figure out if I have a transferd already setup for this user.
	td = m_tdman.find_td_by_user(fquser);
	if (td == NULL) {
		/* nope, so create a TransferDaemon object, and hand it to the td
			manager for it to start. Stash the client socket into the object
			so when it comes online, we can continue our discussion with the
			client. */
		// the id of the transferd for this particular user 
		rand_id.randomlyGenerateHex(64);
		td = new TransferDaemon(fquser, rand_id, TD_PRE_INVOKED);

		// store the client socket in here for later reply.
		td->set_client_sock(rsock);

		// store the request which will get pushed when the daemon wakes up
		td->add_transfer_request(treq);

		// let the manager object start it up for us....
		m_tdman.invoke_a_td(td);

		// set up the continuation for this td and return. When the transferd
		// wakes up and calls home, the schedd will figure out what to do in
		// another handler.

		return KEEP_STREAM;
	}

	////////////////////////////////////////////////////////////////////////
	// Found an already alive one, so just deal with it

	td->add_transfer_request(treq);

	rsock->encode();

	// now push the request(s) to the transferd, this returns when the transferd
	// has gotten the transfer request manifest.
	td->push_transfer_requests();

	// send back the sinful string of the td to the submitting client
	tmp = (char*)td->get_sinful().Value();
	rsock->code(tmp);
	rsock->eom();

	// all done. 

	return CLOSE_STREAM;
}


#if 0


// a client is uploading files to the schedd
int
Scheduler::spoolJobFilesWorkerThread(void *arg, Stream* s)
{
	int ret_val;

	// a client is uploading files to the schedd
	ret_val = uploadGeneralJobFilesWorkerThread(arg,s);

		// Now we sleep here for one second.  Why?  So we are certain
		// to transfer back output files even if the job ran for less 
		// than one second. This is because:
		// stat() can't tell the difference between:
		//   1) A job starts up, touches a file, and exits all in one second
		//   2) A job starts up, doesn't touch the file, and exits all in one 
		//      second
		// So if we force the start time of the job to be one second later than
		// the time we know the files were written, stat() should be able
		// to perceive what happened, if anything.
		dprintf(D_ALWAYS,"Scheduler::spoolJobFilesWorkerThread(void *arg, Stream* s) NAP TIME\n");
	sleep(1);
	return ret_val;
}

// upload files to the schedd
int
Scheduler::uploadGeneralJobFilesWorkerThread(void *arg, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	int i;
	ExtArray<PROC_ID> *jobs = ((job_data_transfer_t *)arg)->jobs;
	char *peer_version = ((job_data_transfer_t *)arg)->peer_version;
	int mode = ((job_data_transfer_t *)arg)->mode;
	int result;
	int old_timeout;
	int cluster, proc;
	
	/* Setup a large timeout; when lots of jobs are being submitted w/ 
	 * large sandboxes, the default is WAY to small...
	 */
	old_timeout = s->timeout(60 * 60 * 8);  

	JobAdsArrayLen = jobs->getlast() + 1;

	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;
		ClassAd * ad = GetJobAd( cluster, proc );
		if ( !ad ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "job ad %d.%d not found\n",cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		} else {
			dprintf(D_FULLDEBUG,"generalJobFilesWorkerThread(): "
					"transfer files for job %d.%d\n",cluster,proc);
		}

		dprintf(D_ALWAYS, "The submitting job ad as the FileTransferObject sees it\n");
		ad->dPrint(D_ALWAYS);

			// Create a file transfer object, with schedd as the server
		result = ftrans.SimpleInit(ad, true, true, rsock);
		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to init filetransfer for job %d.%d \n",
					 cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
		if ( peer_version != NULL ) {
			ftrans.setPeerVersion( peer_version );
		}

		// receive sandbox into the schedd The ftrans object is downloading,
		// but the client is uploading to the schedd.
		result = ftrans.DownloadFiles();

		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to transfer files for job %d.%d \n",
					 cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
	}	
		
		
	rsock->eom();

	int answer;

	rsock->encode();

	answer = OK;
	rsock->code(answer);
	rsock->eom();

	s->timeout(old_timeout);

	if ( peer_version ) {
		free( peer_version );
	}

	if (answer == OK ) {
		return TRUE;
	} else {
		return FALSE;
	}
}



// uploading files to schedd reaper
int
Scheduler::spoolJobFilesReaper(int tid,int exit_status)
{
	ExtArray<PROC_ID> *jobs;
	const char *AttrsToModify[] = { 
		ATTR_JOB_CMD,
		ATTR_JOB_INPUT,
		ATTR_JOB_OUTPUT,
		ATTR_JOB_ERROR,
		ATTR_TRANSFER_INPUT_FILES,
		ATTR_TRANSFER_OUTPUT_FILES,
		ATTR_ULOG_FILE,
		ATTR_X509_USER_PROXY,
		NULL };		// list must end with a NULL


	dprintf(D_FULLDEBUG,"spoolJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

	time_t now = time(NULL);

		// find the list of jobs which we just finished receiving the files
	spoolJobFileWorkers->lookup(tid,jobs);

	if (!jobs) {
		dprintf(D_ALWAYS,"ERROR - JobFilesReaper no entry for tid %d\n",tid);
		return FALSE;
	}

	if (exit_status == FALSE) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		spoolJobFileWorkers->remove(tid);
		delete jobs;
		return FALSE;
	}


	int i,cluster,proc,index;
	char new_attr_value[500];
	char *buf = NULL;
	ExprTree *expr = NULL;
	char *SpoolSpace = NULL;
		// figure out how many jobs we're dealing with
	int len = (*jobs).getlast() + 1;


		// For each job, modify its ClassAd
	for (i=0; i < len; i++) {
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;

		ClassAd *ad = GetJobAd(cluster,proc);
		if (!ad) {
			// didn't find this job ad, must've been removed?
			// just go to the next one
			continue;
		}
		if ( SpoolSpace ) free(SpoolSpace);
		SpoolSpace = strdup( gen_ckpt_name(Spool,cluster,proc,0) );
		ASSERT(SpoolSpace);

		BeginTransaction();

			// Backup the original IWD at submit time
		if (buf) free(buf);
		buf = NULL;
		ad->LookupString(ATTR_JOB_IWD,&buf);
		if ( buf ) {
			sprintf(new_attr_value,"SUBMIT_%s",ATTR_JOB_IWD);
			SetAttributeString(cluster,proc,new_attr_value,buf);
			free(buf);
			buf = NULL;
		}
			// Modify the IWD to point to the spool space			
		SetAttributeString(cluster,proc,ATTR_JOB_IWD,SpoolSpace);

			// Backup the original TRANSFER_OUTPUT_REMAPS at submit time
		expr = ad->Lookup(ATTR_TRANSFER_OUTPUT_REMAPS);
		sprintf(new_attr_value,"SUBMIT_%s",ATTR_TRANSFER_OUTPUT_REMAPS);
		if ( expr ) {
			char *remap_buf = NULL;
			ASSERT( expr->RArg() );
			expr->RArg()->PrintToNewStr(&remap_buf);
			ASSERT(remap_buf);
			SetAttribute(cluster,proc,new_attr_value,remap_buf);
			free(remap_buf);
		}
		else if(ad->Lookup(new_attr_value)) {
				// SUBMIT_TransferOutputRemaps is defined, but
				// TransferOutputRemaps is not; disable the former,
				// so that when somebody fetches the sandbox, nothing
				// gets remapped.
			SetAttribute(cluster,proc,new_attr_value,"Undefined");
		}
			// Set TRANSFER_OUTPUT_REMAPS to Undefined so that we don't
			// do remaps when the job's output files come back into the
			// spool space. We only want to remap when the submitter
			// retrieves the files.
		SetAttribute(cluster,proc,ATTR_TRANSFER_OUTPUT_REMAPS,"Undefined");

			// Now, for all the attributes listed in 
			// AttrsToModify, change them to be relative to new IWD
			// by taking the basename of all file paths.
		index = -1;
		while ( AttrsToModify[++index] ) {
				// Lookup original value
			if (buf) free(buf);
			buf = NULL;
			ad->LookupString(AttrsToModify[index],&buf);
			if (!buf) {
				// attribute not found, so no need to modify it
				continue;
			}
			if ( nullFile(buf) ) {
				// null file -- no need to modify it
				continue;
			}
				// Create new value - deal with the fact that
				// some of these attributes contain a list of pathnames
			StringList old_paths(buf,",");
			StringList new_paths(NULL,",");
			old_paths.rewind();
			char *old_path_buf;
			bool changed = false;
			const char *base = NULL;
			char new_path_buf[_POSIX_PATH_MAX];
			while ( (old_path_buf=old_paths.next()) ) {
				base = condor_basename(old_path_buf);
				if ( strcmp(base,old_path_buf)!=0 ) {
					snprintf(new_path_buf,_POSIX_PATH_MAX,
						"%s%c%s",SpoolSpace,DIR_DELIM_CHAR,base);
					base = new_path_buf;
					changed = true;
				}
				new_paths.append(base);
			}
			if ( changed ) {
					// Backup original value
				sprintf(new_attr_value,"SUBMIT_%s",AttrsToModify[index]);
				SetAttributeString(cluster,proc,new_attr_value,buf);
					// Store new value
				char *new_value = new_paths.print_to_string();
				ASSERT(new_value);
				SetAttributeString(cluster,proc,AttrsToModify[index],new_value);
				free(new_value);
			}
		}

			// Set ATTR_STAGE_IN_FINISH if not already set.
		int spool_completion_time = 0;
		ad->LookupInteger(ATTR_STAGE_IN_FINISH,spool_completion_time);
		if ( !spool_completion_time ) {
			// The transfer thread specifically slept for 1 second
			// to ensure that the job can't possibly start (and finish)
			// prior to the timestamps on the file.  Unfortunately,
			// we note the transfer finish time _here_.  So we've got 
			// to back off 1 second.
			SetAttributeInt(cluster,proc,ATTR_STAGE_IN_FINISH,now - 1);
		}

			// And now release the job.
		releaseJob(cluster,proc,"Data files spooled",false,false,false,false);
		CommitTransaction();
	}

	daemonCore->Register_Timer( 0, 
						(TimerHandlercpp)&Scheduler::reschedule_negotiator_timer,
						"Scheduler::reschedule_negotiator", this );

	spoolJobFileWorkers->remove(tid);
	delete jobs;
	if (SpoolSpace) free(SpoolSpace);
	if (buf) free(buf);
	return TRUE;
}















// download files from schedd
int
Scheduler::downloadJobFiles(int mode, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	ExtArray<PROC_ID> *jobs = NULL;
	char *constraint_string = NULL;
	int i;
	static int spool_reaper_id = -1;
	static int transfer_reaper_id = -1;
	PROC_ID a_job;
	int tid;
	char *peer_version = NULL;

		// make sure this connection is authenticated, and we know who
		// the user is.  also, set a timeout, since we don't want to
		// block long trying to read from our client.   
	rsock->timeout( 10 );  
	rsock->decode();

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
			errstack.push( "SCHEDD", SCHEDD_ERR_SPOOL_FILES_FAILED,
					"Failure to spool job files - Authentication failed" );
			dprintf( D_ALWAYS, "spoolJobFiles() aborting: %s\n",
					 errstack.getFullText() );
			refuse( s );
			return FALSE;
		}
	}	


	rsock->decode();

	peer_version = NULL;
	if ( !rsock->code(peer_version) ) {
		dprintf( D_ALWAYS,
			 	"spoolJobFiles(): failed to read peer_version\n" );
		refuse(s);
		return FALSE;
	}
		// At this point, we are responsible for deallocating
		// peer_version with free()



	// read constraint string
	if ( !rsock->code(constraint_string) || constraint_string == NULL )
	{
			dprintf( D_ALWAYS, "spoolJobFiles(): "
				 	"failed to read constraint string\n" );
			refuse(s);
			return FALSE;
	}

	jobs = new ExtArray<PROC_ID>;
	ASSERT(jobs);

	setQSock(rsock);	// so OwnerCheck() will work

	time_t now = time(NULL);

	{
	ClassAd * tmp_ad = GetNextJobByConstraint(constraint_string,1);
	JobAdsArrayLen = 0;
	while (tmp_ad) {
		if ( tmp_ad->LookupInteger(ATTR_CLUSTER_ID,a_job.cluster) &&
		 	tmp_ad->LookupInteger(ATTR_PROC_ID,a_job.proc) &&
		 	OwnerCheck(a_job.cluster, a_job.proc) )
		{
			(*jobs)[JobAdsArrayLen++] = a_job;
		}
		tmp_ad = GetNextJobByConstraint(constraint_string,0);
	}
	dprintf(D_FULLDEBUG, "Scheduler::spoolJobFiles: "
		"TRANSFER_DATA/WITH_PERMS: %d jobs matched constraint %s\n",
		JobAdsArrayLen, constraint_string);
	if (constraint_string) free(constraint_string);
		// Now set ATTR_STAGE_OUT_START
	for (i=0; i<JobAdsArrayLen; i++) {
			// TODO --- maybe put this in a transaction?
		SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,
						ATTR_STAGE_OUT_START,now);
	}
	}

	unsetQSock();

	rsock->eom();

		// DaemonCore will free the thread_arg for us when the thread
		// exits, but we need to free anything pointed to by
		// job_data_transfer_t ourselves. generalJobFilesWorkerThread()
		// will free 'peer_version' and our reaper will free 'jobs' (the
		// reaper needs 'jobs' for some of its work).
	job_data_transfer_t *thread_arg = (job_data_transfer_t *)malloc( sizeof(job_data_transfer_t) );
	thread_arg->mode = mode;
	thread_arg->peer_version = peer_version;
	thread_arg->jobs = jobs;

	if ( transfer_reaper_id == -1 ) {
		transfer_reaper_id = daemonCore->Register_Reaper(
				"transferJobFilesReaper",
				(ReaperHandlercpp) &Scheduler::transferJobFilesReaper,
				"transferJobFilesReaper",
				this
			);
		}

	// Start a new thread (process on Unix) to do the work
	tid = daemonCore->Create_Thread(
			(ThreadStartFunc) &Scheduler::transferJobFilesWorkerThread,
			(void *)thread_arg,
			s,
			transfer_reaper_id
			);

	if ( tid == FALSE ) {
		free(thread_arg);
		if ( peer_version ) {
			free( peer_version );
		}
		delete jobs;
		refuse(s);
		return FALSE;
	}

		// Place this tid into a hashtable so our reaper can finish up.
	spoolJobFileWorkers->insert(tid, jobs);
	
	return TRUE;
}

// a client is getting files from the schedd
int
Scheduler::transferJobFilesWorkerThread(void *arg, Stream* s)
{
	// a client is getting files from the schedd
	return downloadGeneralJobFilesWorkerThread(arg,s);
}



// get files from the schedd
int
Scheduler::downloadGeneralJobFilesWorkerThread(void *arg, Stream* s)
{
	ReliSock* rsock = (ReliSock*)s;
	int JobAdsArrayLen = 0;
	int i;
	ExtArray<PROC_ID> *jobs = ((job_data_transfer_t *)arg)->jobs;
	char *peer_version = ((job_data_transfer_t *)arg)->peer_version;
	int mode = ((job_data_transfer_t *)arg)->mode;
	int result;
	int old_timeout;
	int cluster, proc;
	
	/* Setup a large timeout; when lots of jobs are being submitted w/ 
	 * large sandboxes, the default is WAY to small...
	 */
	old_timeout = s->timeout(60 * 60 * 8);  

	JobAdsArrayLen = jobs->getlast() + 1;
//	dprintf(D_FULLDEBUG,"TODD spoolJobFilesWorkerThread: JobAdsArrayLen=%d\n",JobAdsArrayLen);

	// if sending sandboxes, first tell the client how many
	// we are about to send.
	dprintf(D_FULLDEBUG, "Scheduler::generalJobFilesWorkerThread: "
		"TRANSFER_DATA/WITH_PERMS: %d jobs to be sent\n", JobAdsArrayLen);
	rsock->encode();
	if ( !rsock->code(JobAdsArrayLen) || !rsock->eom() ) {
		dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
				 "failed to send JobAdsArrayLen (%d) \n",
				 JobAdsArrayLen );
		refuse(s);
		return FALSE;
	}

	for (i=0; i<JobAdsArrayLen; i++) {
		FileTransfer ftrans;
		cluster = (*jobs)[i].cluster;
		proc = (*jobs)[i].proc;
		ClassAd * ad = GetJobAd( cluster, proc );
		if ( !ad ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "job ad %d.%d not found\n",cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		} else {
			dprintf(D_FULLDEBUG,"generalJobFilesWorkerThread(): "
					"transfer files for job %d.%d\n",cluster,proc);
		}

		dprintf(D_ALWAYS, "The submitting job ad as the FileTransferObject sees it\n");
		ad->dPrint(D_ALWAYS);

			// Create a file transfer object, with schedd as the server
		result = ftrans.SimpleInit(ad, true, true, rsock);
		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to init filetransfer for job %d.%d \n",
					 cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
		if ( peer_version != NULL ) {
			ftrans.setPeerVersion( peer_version );
		}

		// Send or receive files as needed
		// send sandbox out of the schedd
		rsock->encode();
		// first send the classad for the job
		result = ad->put(*rsock);
		if (!result) {
			dprintf(D_ALWAYS, "generalJobFilesWorkerThread(): "
				"failed to send job ad for job %d.%d \n",
				cluster,proc );
		} else {
			rsock->eom();
			// and then upload the files
			result = ftrans.UploadFiles();
		}

		if ( !result ) {
			dprintf( D_ALWAYS, "generalJobFilesWorkerThread(): "
					 "failed to transfer files for job %d.%d \n",
					 cluster,proc );
			refuse(s);
			s->timeout(old_timeout);
			return FALSE;
		}
	}	
		
		
	rsock->eom();

	int answer;
	rsock->decode();
	answer = -1;

	rsock->code(answer);
	rsock->eom();
	s->timeout(old_timeout);

	if ( peer_version ) {
		free( peer_version );
	}

	if (answer == OK ) {
		return TRUE;
	} else {
		return FALSE;
	}
}




// dowloading files from schedd reaper
int
Scheduler::transferJobFilesReaper(int tid,int exit_status)
{
	ExtArray<PROC_ID> *jobs;
	int i;

	dprintf(D_FULLDEBUG,"transferJobFilesReaper tid=%d status=%d\n",
			tid,exit_status);

		// find the list of jobs which we just finished receiving the files
	spoolJobFileWorkers->lookup(tid,jobs);

	if (!jobs) {
		dprintf(D_ALWAYS,
			"ERROR - transferJobFilesReaper no entry for tid %d\n",tid);
		return FALSE;
	}

	if (exit_status == FALSE) {
		dprintf(D_ALWAYS,"ERROR - Staging of job files failed!\n");
		spoolJobFileWorkers->remove(tid);
		delete jobs;
		return FALSE;
	}

		// For each job, modify its ClassAd
	time_t now = time(NULL);
	int len = (*jobs).getlast() + 1;
	for (i=0; i < len; i++) {
			// TODO --- maybe put this in a transaction?
		SetAttributeInt((*jobs)[i].cluster,(*jobs)[i].proc,ATTR_STAGE_OUT_FINISH,now);
	}

		// Now, deallocate memory
	spoolJobFileWorkers->remove(tid);
	delete jobs;
	return TRUE;
}

#endif /* if 0 */
