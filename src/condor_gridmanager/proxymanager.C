/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "condor_config.h"
#include "classad_hashtable.h"
#include "util_lib_proto.h"
#include "env.h"
#include "directory.h"
#include "daemon.h"
#include "internet.h"
#include "simplelist.h"
#include "my_username.h"

#include "globus_utils.h"

#include "proxymanager.h"
//#include "myproxy_manager.h"
#include "gahp-client.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, Proxy *>;
template class HashBucket<HashKey, Proxy *>;
//<<<<<<< proxymanager.C


template class SimpleList<MyProxyEntry*>;


//template class HashTable<HashKey, MyProxyManager *>;
//template class HashBucket<HashKey, MyProxyManager *>;

HashTable <HashKey, Proxy *> ProxiesByPath( HASH_TABLE_SIZE,
											hashFunction );
//HashTable <HashKey, MyProxyManager *> MyProxyManagersByPath ( HASH_TABLE_SIZE, hashFunction );

//MyProxyManager myProxyManager;
//=======
template class HashTable<HashKey, ProxySubject *>;
template class HashBucket<HashKey, ProxySubject *>;
template class List<Proxy>;
template class Item<Proxy>;

HashTable <HashKey, Proxy *> ProxiesByFilename( HASH_TABLE_SIZE,
												hashFunction );
HashTable <HashKey, ProxySubject *> SubjectsByName( 50, hashFunction );
//>>>>>>> 1.1.4.1.2.8

static bool proxymanager_initialized = false;
static int CheckProxies_tid = TIMER_UNSET;

int CheckProxies_interval = 600;		// default value
int minProxy_time = 3 * 60;				// default value
int myproxyGetDelegationReaperId = 0;


static int next_proxy_id = 1;

int CheckProxies();

static bool
SetMasterProxy( Proxy *master, const Proxy *copy_src )
{
	int rc;
	MyString tmp_file;

	tmp_file.sprintf( "%s.tmp", master->proxy_filename );

	rc = copy_file( copy_src->proxy_filename, tmp_file.Value() );
	if ( rc != 0 ) {
		return false;
	}

	rc = rotate_file( tmp_file.Value(), master->proxy_filename );
	if ( rc != 0 ) {
		unlink( tmp_file.Value() );
		return false;
	}

	master->expiration_time = copy_src->expiration_time;
	master->near_expired = copy_src->near_expired;

	int tid;
	master->notification_tids.Rewind();
	while ( master->notification_tids.Next( tid ) ) {
		daemonCore->Reset_Timer( tid, 0 );
	}

	return true;
}

// Initialize the ProxyManager module. proxy_dir is the directory in
// which the module should place the "master" proxy files.
bool InitializeProxyManager( const char *proxy_dir )
{
	if ( proxymanager_initialized == true ) {
		return false;
	}

	CheckProxies_tid = daemonCore->Register_Timer( 1, CheckProxies_interval,
												   (TimerHandler)&CheckProxies,
												   "CheckProxies", NULL );

	proxymanager_initialized = true;

	return true;
}	

// Set the host:port of the MyProxy server for a given proxy
// Proxymanager will talk to the given server when the proxy is about to expire
/*int SetMyProxyHostForProxy ( const char* hostport, const char* server_dn, const char* myproxy_pwd, const Proxy * proxy ) {
	if (hostport == NULL || proxy == NULL) {
		return FALSE;
	}

	if (myProxyManager.AddMyProxyEntry (hostport, server_dn, myproxy_pwd, proxy->proxy_filename)) {

		dprintf (D_ALWAYS, "Created MyProxy manager (%s) for %s\n", hostport, proxy->proxy_filename);

		return TRUE;
	}

	return FALSE;
}*/

// An entity (e.g. GlobusJob, GlobusResource object) should call this
// function when it wants to use a proxy managed by ProxyManager. proxy_path
// is the path to the proxy it wants to use. notify_tid is a timer id that
// will be signalled when something interesting happens with the proxy
// (it's about to expire or has been refreshed). A Proxy struct will be
// returned. When the Proxy is no longer needed, ReleaseProxy() should be
// called with it. If no notifications are desired, give a
// negative number for notify_tid or omit it. Note the the Proxy returned
// is a shared data-structure and shouldn't be delete'd or modified by
// the caller.
Proxy *
AcquireProxy( const char *proxy_path, int notify_tid )
{
	if ( proxymanager_initialized == false ) {
		return NULL;
	}

	int expire_time;
	Proxy *proxy = NULL;
	ProxySubject *proxy_subject = NULL;
	char *subject_name = NULL;

	if ( ProxiesByFilename.lookup( HashKey(proxy_path), proxy ) == 0 ) {
		// We already know about this proxy,
		// return the existing Proxy struct
		proxy->num_references++;
		if ( notify_tid > 0 &&
			 proxy->notification_tids.IsMember( notify_tid ) == false ) {
			proxy->notification_tids.Append( notify_tid );
		}
		return proxy;
	}

	// We don't know about this proxy yet,
	// find the proxy's expiration time and subject name
	expire_time = x509_proxy_expiration_time( proxy_path );
	if ( expire_time < 0 ) {
		dprintf( D_ALWAYS, "Failed to get expiration time of proxy %s\n",
				 proxy_path );
		return NULL;
	}
	subject_name = x509_proxy_subject_name( proxy_path );
	if ( subject_name == NULL ) {
		dprintf( D_ALWAYS, "Failed to get subject of proxy %s\n", proxy_path );
		return NULL;
	}

	// Create a Proxy struct for our new proxy and populate it
	proxy = new Proxy;
	proxy->proxy_filename = strdup(proxy_path);
	proxy->num_references = 1;
	proxy->expiration_time = expire_time;
	proxy->near_expired = (expire_time - time(NULL)) <= minProxy_time;
	proxy->id = next_proxy_id++;
	if ( notify_tid > 0 &&
		 proxy->notification_tids.IsMember( notify_tid ) == false ) {
		proxy->notification_tids.Append( notify_tid );
	}

	ProxiesByFilename.insert(HashKey(proxy_path), proxy);

	if ( SubjectsByName.lookup( HashKey(subject_name), proxy_subject ) != 0 ) {
		// We don't know about this proxy subject yet,
		// create a new ProxySubject and fill it out
		proxy_subject = new ProxySubject;
		proxy_subject->subject_name = strdup( subject_name );

		// Create a master proxy for our new ProxySubject
		Proxy *new_master = new Proxy;
		new_master->id = next_proxy_id++;
		MyString tmp;
		tmp.sprintf( "%s/master_proxy.%d", GridmanagerScratchDir,
					 new_master->id );
		new_master->proxy_filename = strdup( tmp.Value() );
		new_master->num_references = 0;
		new_master->subject = proxy_subject;
		SetMasterProxy( new_master, proxy );
		ProxiesByFilename.insert( HashKey(new_master->proxy_filename),
								  new_master );

//<<<<<<< proxymanager.C


	daemonCore->Reset_Timer( CheckProxies_tid, 0 );
//=======
		proxy_subject->master_proxy = new_master;

		SubjectsByName.insert(HashKey(proxy_subject->subject_name),
							  proxy_subject);
	}

	proxy_subject->proxies.Append( proxy );

	proxy->subject = proxy_subject;

	// If the new Proxy is longer-lived than the current master proxy for
	// this subject, copy it for the new master.
	if ( proxy->expiration_time > proxy_subject->master_proxy->expiration_time ) {
			SetMasterProxy( proxy_subject->master_proxy, proxy );
	}

	free( subject_name );
//>>>>>>> 1.1.4.1.2.8

	return proxy;
}

Proxy *
AcquireProxy( Proxy *proxy, int notify_tid )
{
	proxy->num_references++;
	if ( notify_tid > 0 &&
		 proxy->notification_tids.IsMember( notify_tid ) == false ) {
		proxy->notification_tids.Append( notify_tid );
	}
	return proxy;
}

// Call this function to indicate that you are done with a Proxy previously
// acquired with AcquireProxy(). Do not delete the Proxy yourself. The
// ProxyManager code will take care of that for you. If you provided a
// notify_tid to AcquireProxy(), provide it again here.
void
ReleaseProxy( Proxy *proxy, int notify_tid )
{
	if ( proxymanager_initialized == false || proxy == NULL ) {
		return;
	}

	proxy->num_references--;
	if ( notify_tid > 0 ) {
		proxy->notification_tids.Delete( notify_tid );
	}

	if ( proxy->num_references < 0 ) {
		dprintf( D_ALWAYS, "Reference count for proxy %s is negative!\n",
				 proxy->proxy_filename );
	}

	if ( proxy->num_references <= 0 ) {

		ProxySubject *proxy_subject = proxy->subject;

		if ( proxy != proxy_subject->master_proxy ) {
			ProxiesByFilename.remove( HashKey(proxy->proxy_filename) );
			proxy_subject->proxies.Delete( proxy );
			free( proxy->proxy_filename );
			delete proxy;
		}

		if ( proxy_subject->proxies.IsEmpty() &&
			 proxy_subject->master_proxy->num_references <= 0 ) {

			ProxiesByFilename.remove( HashKey(proxy_subject->master_proxy->proxy_filename) );
			free( proxy_subject->master_proxy->proxy_filename );
			delete proxy_subject->master_proxy;

			SubjectsByName.remove( HashKey(proxy_subject->subject_name) );
			free( proxy_subject->subject_name );
			delete proxy_subject;
		}

	}

}

void DeleteMyProxyEntry (MyProxyEntry *& myproxy_entry) {
	if (myproxy_entry->get_delegation_pid != FALSE) {
		// Kill the process
		//daemonCore->Shutdown_Graceful (proxy->myproxy_entry->get_delegation_pid);
		myproxy_entry->get_delegation_pid=FALSE;
	}

	if (myproxy_entry->myproxy_host) {
		free (myproxy_entry->myproxy_host);
	}

	if (myproxy_entry->myproxy_server_dn) {
		free (myproxy_entry->myproxy_server_dn);
	}

	if (myproxy_entry->myproxy_password) {
		free (myproxy_entry->myproxy_password);
	}

	if (myproxy_entry->myproxy_credential_name) {
		free (myproxy_entry->myproxy_credential_name);
	}

	if (myproxy_entry->get_delegation_err_filename) {
		free (myproxy_entry->get_delegation_err_filename);
	}

	if (myproxy_entry->get_delegation_password_pipe[0] > -1) {
		close (myproxy_entry->get_delegation_password_pipe[0]);
	}

	if (myproxy_entry->get_delegation_password_pipe[1] > -1) {
		close (myproxy_entry->get_delegation_password_pipe[1]);
	}


}

// Utility function to deep-delete the Proxy data structure
void DeleteProxy (Proxy *& proxy) {

	if (proxy->proxy_filename) {
		free( proxy->proxy_filename );
	}

	MyProxyEntry * myproxy_entry;
	proxy->myproxy_entries.Rewind();
	proxy->myproxy_entries.Next(myproxy_entry);

	while (proxy->myproxy_entries.Current(myproxy_entry) == true) {
		DeleteMyProxyEntry (myproxy_entry);
		delete myproxy_entry;
	}

	delete proxy;
}

void doCheckProxies()
{
	if ( CheckProxies_tid != TIMER_UNSET ) {
		daemonCore->Reset_Timer( CheckProxies_tid, 0 );
	}
}

// This function is called
// periodically to check for updated proxies. It can be called earlier
// if a proxy is about to expire.
int CheckProxies()
{
	int now = time(NULL);
	int next_check = CheckProxies_interval + now;
//<<<<<<< proxymanager.C
	int rc;

	dprintf(D_FULLDEBUG,"CheckProxies called\n");
//=======
	ProxySubject *curr_subject;
//>>>>>>> 1.1.4.1.2.8

	SubjectsByName.startIterations();

//<<<<<<< proxymanager.C
	// As we check our proxies, keep an eye out for a new master proxy.
	// The new master needs to be valid and have an expiration time at
	// 60 seconds longer than the current master proxy.
	new_master = NULL;
	new_max_expire = MasterProxy.expiration_time + 60;
	if ( new_max_expire < now + 180 ) {
		new_max_expire = now + 180;
	}
//=======
	while ( SubjectsByName.iterate( curr_subject ) != 0 ) {
//>>>>>>> 1.1.4.1.2.8

		Proxy *curr_proxy;
		Proxy *new_master = curr_subject->master_proxy;

		curr_subject->proxies.Rewind();

//<<<<<<< proxymanager.C
		// Remove any proxies that are no longer being used by anyone
		if ( next_proxy->num_references == 0 ) {

			// If myproxy-get-delegation is still running, don't delete this just yet
			next_proxy->myproxy_entries.Rewind();
			MyProxyEntry * mpe;
			int keep = FALSE;
			while (next_proxy->myproxy_entries.Next(mpe)) {
				if (mpe->get_delegation_pid != FALSE) {
					keep = TRUE;
					break;
				}
			}
			if (keep) {
				continue;
			}


dprintf(D_FULLDEBUG,"  removing old proxy %d\n",next_proxy->gahp_proxy_id);
			ProxiesByPath.remove( HashKey(next_proxy->proxy_filename) );
			if ( my_gahp.uncacheProxy( next_proxy->gahp_proxy_id ) == false ) {
				EXCEPT( "GAHP uncache command failed!" );
			}
			/*free( next_proxy->proxy_filename );
			delete next_proxy;*/
			DeleteProxy (next_proxy);
			continue;
		}
//=======
		while ( curr_subject->proxies.Next( curr_proxy ) != false ) {
//>>>>>>> 1.1.4.1.2.8

//<<<<<<< proxymanager.C
		/*

		This is moved to RefreshProxy

		// If this proxy is renewable via myproxy
		// and we don't have the password for it yet,
		// ask the Schedd

		next_proxy->myproxy_entries.Rewind();
		MyProxyEntry * mpe = NULL;
		while (next_proxy->myproxy_entries.Next(mpe)) {
			if (!mpe->myproxy_password) {
				if (!GetMyProxyPasswordFromSchedD (mpe->cluster_id,
													mpe->proc_id,
													&(mpe->myproxy_password))) {
					dprintf (D_ALWAYS,
						"Unable to retrieve MyProxy password from SchedD for proxy (job: %d.%d) %s\n",
						mpe->cluster_id,
						mpe->proc_id,
						next_proxy->proxy_filename);
					// This is unfortunate, let's hope the proxy doesn't expire before we finish the job
				}
			}
		}*/

		int new_expiration = x509_proxy_expiration_time( next_proxy->proxy_filename );
		// If the proxy is valid, and either it hasn't been cached in the
		// gahp_server yet or it's been updated, (re)cache it in the
		// gahp_server and notify everyone who cares.
		if ( new_expiration > now + 180 &&
			 ( next_proxy->gahp_proxy_id == -1 ||
			   new_expiration > next_proxy->expiration_time ) ) {
//=======
			curr_proxy->near_expired =
				(curr_proxy->expiration_time - now) <= minProxy_time;
//>>>>>>> 1.1.4.1.2.8

			int new_expiration =
				x509_proxy_expiration_time( curr_proxy->proxy_filename );

			if ( new_expiration > curr_proxy->expiration_time ) {

//<<<<<<< proxymanager.C
dprintf(D_FULLDEBUG,"  (re)caching proxy %d\n",next_proxy->gahp_proxy_id);
			if ( my_gahp.cacheProxyFromFile( next_proxy->gahp_proxy_id,
											 next_proxy->proxy_filename ) == false ) {
				// TODO is there a better way to react?
				EXCEPT( "GAHP cache command failed!" );
			}
//=======
				curr_proxy->expiration_time = new_expiration;
//>>>>>>> 1.1.4.1.2.8

				curr_proxy->near_expired =
					(curr_proxy->expiration_time - now) <= minProxy_time;

//<<<<<<< proxymanager.C
		}


		// Check whether to renew the proxy (renew 4 hrs beforehand)
		if ( (new_expiration <= now + 4*60*60) && (!next_proxy->myproxy_entries.IsEmpty()) ) {
			dprintf (D_FULLDEBUG, "About to RefreshProxyThruMyProxy() for %s\n", next_proxy->proxy_filename);
			RefreshProxyThruMyProxy (next_proxy);
		}


		if ( new_expiration <= now + minProxy_time ) {
			// This proxy has expired or is about to expire. Mark it
			// as such and notify everyone who cares.
			if ( next_proxy->near_expired == false ) {
dprintf(D_FULLDEBUG,"  marking proxy %d as about to expire\n",next_proxy->gahp_proxy_id);
				next_proxy->near_expired = true;
//=======
//>>>>>>> 1.1.4.1.2.8
				int tid;
				curr_proxy->notification_tids.Rewind();
				while ( curr_proxy->notification_tids.Next( tid ) ) {
					daemonCore->Reset_Timer( tid, 0 );
				}

				if ( curr_proxy->expiration_time > new_master->expiration_time ) {
					new_master = curr_proxy;
				}

			} else if ( curr_proxy->near_expired ) {

				int tid;
				curr_proxy->notification_tids.Rewind();
				while ( curr_proxy->notification_tids.Next( tid ) ) {
					daemonCore->Reset_Timer( tid, 0 );
				}

//<<<<<<< proxymanager.C
	// If we found a new master proxy, copy it to the master proxy location,
	// update the master Proxy struct, update the GAHP server, and notify
	// everyone who cares
	if ( new_master != NULL && SetMasterProxy( new_master ) == true ) {

dprintf(D_FULLDEBUG,"  proxy %d is now the master proxy\n",new_master->gahp_proxy_id);
		if ( gahp_initialized == false ) {
			// This is our first master proxy, perform the callback so that
			// the GAHP can be intialized with it
dprintf(D_FULLDEBUG,"  first master found, calling gahp init function\n");
			if ( (*InitGahpFunc)( MasterProxy.proxy_filename ) == false ) {
				EXCEPT( "GAHP initalization failed!" );
//=======
//>>>>>>> 1.1.4.1.2.8
			}

			if ( curr_proxy->expiration_time - minProxy_time < next_check &&
				 !curr_proxy->near_expired ) {
				next_check = curr_proxy->expiration_time - minProxy_time;
			}

		}

//<<<<<<< proxymanager.C
			gahp_initialized = true;
		} else {
			// Refresh the master proxy credentials in the GAHP
dprintf(D_FULLDEBUG,"  refreshing master proxy in gahp\n");
			rc = my_gahp.globus_gram_client_set_credentials( MasterProxy.proxy_filename );
			// TODO if set-credentials fails, what to do?
			if ( rc != 0 ) {
				dprintf( D_ALWAYS, "GAHP set credentails failed! rc=%d\n", rc);
			}
			if ( my_gahp.cacheProxyFromFile( MasterProxy.gahp_proxy_id,
											 MasterProxy.proxy_filename ) == false ) {
				EXCEPT( "GAHP cache command failed!" );
			}
		}
//=======
		if ( new_master != curr_subject->master_proxy ) {
			
			SetMasterProxy( curr_subject->master_proxy, new_master );
//>>>>>>> 1.1.4.1.2.8

		}

	}

	// next_check is the absolute time of the next check, convert it to
	// a relative time (from now)
//<<<<<<< proxymanager.C
dprintf(D_FULLDEBUG,"  will call CheckProxies again in %d seconds\n",next_check-now);
//=======
//>>>>>>> 1.1.4.1.2.8
	daemonCore->Reset_Timer( CheckProxies_tid, next_check - now );

	return TRUE;
}

int RefreshProxyThruMyProxy(Proxy * proxy) {

	char * proxy_filename = proxy->proxy_filename;
	MyProxyEntry * myProxyEntry = NULL;


	// Starting from the most recent myproxy entry
	// Find an entry with a password
	int found = FALSE;
	proxy->myproxy_entries.Rewind();
	while (proxy->myproxy_entries.Next (myProxyEntry)) {
		if (myProxyEntry->myproxy_password ||
			GetMyProxyPasswordFromSchedD (myProxyEntry->cluster_id,
												myProxyEntry->proc_id,
												&(myProxyEntry->myproxy_password))) {
			found=TRUE;

			//. Now move it to the front of the list
			proxy->myproxy_entries.DeleteCurrent();
			proxy->myproxy_entries.Prepend(myProxyEntry);
			break;
		}
	}

	if (!found) {
		// We're screwed - can't get MyProxy passwords for any entry
		return FALSE;
	}

  // Make sure we're not called more often than necessary and if
  time_t now=time(NULL);
  if ((myProxyEntry->get_delegation_pid != FALSE) || (now - myProxyEntry->last_invoked_time < 30)) {
  	dprintf (D_ALWAYS, "proxy %s too soon or myproxy-get-delegation already started\n",proxy_filename);
	return FALSE;
  }
  myProxyEntry->last_invoked_time=now;


  // If you don't have a myproxy password, ask SchedD for it
  if (!myProxyEntry->myproxy_password) {
  	// Will there ever be a case when there is no MyProxy password needed at all?
  	return FALSE;
  }

	// Initialize reaper, if needed
	if (myproxyGetDelegationReaperId == 0 ) {
		myproxyGetDelegationReaperId = daemonCore->Register_Reaper(
					   "GetDelegationReaper",
					   (ReaperHandler) &MyProxyGetDelegationReaper,
					   "GetDelegation Reaper");
  	}

	// Set up environnment for myproxy-get-delegation
	Env myEnv;
	char buff[_POSIX_PATH_MAX];

	if (myProxyEntry->myproxy_server_dn) {
		sprintf (buff, "MYPROXY_SERVER_DN=%s", myProxyEntry->myproxy_server_dn);
		myEnv.Put (buff);
		dprintf (D_FULLDEBUG, "%s\n", buff);
	}


	sprintf (buff,"X509_USER_PROXY=%s", proxy_filename);
	myEnv.Put (buff);
	dprintf (D_FULLDEBUG, "%s\n", buff);


    // Print password (this will end up in stdin for myproxy-get-delegation)
	pipe (myProxyEntry->get_delegation_password_pipe);
	write (myProxyEntry->get_delegation_password_pipe[1],
		myProxyEntry->myproxy_password,
		strlen (myProxyEntry->myproxy_password));
	write (myProxyEntry->get_delegation_password_pipe[1], "\n", 1);


	// Figure out user name;
	char * username = my_username(0);	


	// Figure out myproxy host and port
	char * myproxy_host = getHostFromAddr (myProxyEntry->myproxy_host);
	int myproxy_port = getPortFromAddr (myProxyEntry->myproxy_host);

	// args
	char args[1000];
	sprintf(args, "-v -o %s -s %s -d -t %d -S -l %s",
		proxy_filename,
		myproxy_host,
		12,
		username);


	// Optional port argument
	if (myproxy_port) {
		sprintf (buff, " -p %d ", myproxy_port);
		strcat (args, buff);
	}

	// Optional credential name argument
	if (myProxyEntry->myproxy_credential_name) {
		sprintf (buff, " -k %s ", myProxyEntry->myproxy_credential_name);
		strcat (args, buff);
	}

	free (username);
	free (myproxy_host);

	// Create temporary file to store myproxy-get-delegation's stderr
	myProxyEntry->get_delegation_err_filename = create_temp_file();
	chmod (myProxyEntry->get_delegation_err_filename, 0600);
	myProxyEntry->get_delegation_err_fd = open (myProxyEntry->get_delegation_err_filename,O_RDWR);
	if (myProxyEntry->get_delegation_err_fd == -1) {
		dprintf (D_ALWAYS, "Error opening file %s\n",myProxyEntry->get_delegation_err_filename);
	}


	int arrIO[3];
	arrIO[0]=myProxyEntry->get_delegation_password_pipe[0]; //stdin
	arrIO[1]=-1; //myProxyEntry->get_delegation_err_fd;
	arrIO[2]=myProxyEntry->get_delegation_err_fd; // stderr

	char * myproxy_get_delegation_pgm = param ("MYPROXY_GET_DELEGATION");
	if (!myproxy_get_delegation_pgm) {
		dprintf (D_ALWAYS, "MYPROXY_GET_DELEGATION not defined in config file\n");
		return FALSE;
	}


	dprintf (D_ALWAYS, "Calling %s %s\n", myproxy_get_delegation_pgm, args);

	int pid = daemonCore->Create_Process (
					myproxy_get_delegation_pgm,
					args,
					PRIV_USER_FINAL,
					myproxyGetDelegationReaperId,
					FALSE,
					myEnv.getDelimitedString(),
					NULL,	// cwd
					FALSE, // new proc group
					NULL,  // socket inherit
					arrIO); // in/out/err streams

	free (myproxy_get_delegation_pgm);

  	if (pid == FALSE) {
		dprintf (D_ALWAYS, "Failed to run myproxy-get-delegation\n");

		myProxyEntry->get_delegation_err_fd=-1;
		myProxyEntry->get_delegation_pid=FALSE;
		close (myProxyEntry->get_delegation_err_fd);
		unlink (myProxyEntry->get_delegation_err_filename);// Remove the temporary file
		free (myProxyEntry->get_delegation_err_filename);

		close (myProxyEntry->get_delegation_password_pipe[0]);
		close (myProxyEntry->get_delegation_password_pipe[1]);
		myProxyEntry->get_delegation_password_pipe[0]=-1;
		myProxyEntry->get_delegation_password_pipe[1]=-1;

		return FALSE;
	}

  myProxyEntry->get_delegation_pid = pid;

  return TRUE;

}


int MyProxyGetDelegationReaper(Service *, int exitPid, int exitStatus) {

	// Find the right MyProxyEntry
	Proxy *proxy=NULL;
	MyProxyEntry *matched_entry=NULL;
	int found = FALSE;

	// Iterate through each proxy
	ProxiesByPath.startIterations();
	while ( ProxiesByPath.iterate( proxy ) != 0 ) {
		// Iterate through all myproxy entries for the proxy
		proxy->myproxy_entries.Rewind();
		while (proxy->myproxy_entries.Next(matched_entry)) {
			if (matched_entry->get_delegation_pid == exitPid) {
				found = TRUE;
				break;
			}
		}
		if (found) {
			break;
		}
	}

 	if (!found) {
		dprintf (D_ALWAYS, "WEIRD! MyProxyManager::GetDelegationReaper unable to find entry for pid %d", exitPid);
		return FALSE;
	}

	if (exitStatus == 0) {
		dprintf (D_ALWAYS, "myproxy-get-delegation for proxy %s exited successfully\n", proxy->proxy_filename);
		close (matched_entry->get_delegation_err_fd);
	} else {
		// This myproxyEntry is no good, move it to the back of the list
		MyProxyEntry * myProxyEntry = NULL;
		proxy->myproxy_entries.Rewind();
		if (proxy->myproxy_entries.Next (myProxyEntry)) {
			proxy->myproxy_entries.DeleteCurrent();
			proxy->myproxy_entries.Append (myProxyEntry);
		}

		// In the case of an error, append the stderr stream of myproxy-get-delegation to log
		close (matched_entry->get_delegation_err_fd);

		char buff[500];
		buff[0]='\0';
		int fd = open (matched_entry->get_delegation_err_filename, O_RDONLY);
		if (fd != -1) {
			int bytes_read = read (fd, buff, 499);
			close (fd);
			buff[bytes_read]='\0';
		} else {
			dprintf (D_ALWAYS, "WEIRD! Cannot read err file %s\n", matched_entry->get_delegation_err_filename);
		}

		dprintf (D_ALWAYS, "myproxy-get-delegation for proxy %s, for job (%d,%d) exited with code %d, output (top):\n%s\n\n",
			proxy->proxy_filename,
			matched_entry->cluster_id,
			matched_entry->proc_id,
			WEXITSTATUS(exitStatus),
			buff);

	}


	// Clean up
	close (matched_entry->get_delegation_password_pipe[0]);
	close (matched_entry->get_delegation_password_pipe[1]);
	matched_entry->get_delegation_password_pipe[0]=-1;
	matched_entry->get_delegation_password_pipe[1]=-1;

	matched_entry->get_delegation_err_fd=-1;
	matched_entry->get_delegation_pid=FALSE;
	unlink (matched_entry->get_delegation_err_filename);// Remove the temporary file
	free (matched_entry->get_delegation_err_filename);
	matched_entry->get_delegation_err_filename=NULL;

   return TRUE;
}

int GetMyProxyPasswordFromSchedD (int cluster_id, int proc_id, char ** password) {

	// This might seem not necessary, but it IS
	// For some reason you can't just pass cluster_id to sock->code() directly!!!!
	int cluster, proc;
	cluster = cluster_id;
	proc = proc_id;

	dprintf ( D_FULLDEBUG, " GetMyProxyPasswordFromSchedD %d, %d\n", cluster_id, proc_id);

	// Get At Schedd
	Daemon	schedd( DT_SCHEDD );
	if( ! schedd.locate() ) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Can't find address of local schedd\n" );
		return FALSE;
	}

	// Start command
	Sock* sock;
	if (!(sock = schedd.startCommand( GET_MYPROXY_PASSWORD, Stream::reli_sock, 0))) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Could not connect to local schedd\n" );
		return FALSE;
	}

	sock->encode();

	if (!sock->code (cluster) || !sock->code(proc)) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Could not encode clusterId, procId\n" );
		return FALSE;
	}

	sock->eom();
	sock->decode();

	if (!sock->code (*password)) {
		dprintf( D_ALWAYS, "GetMyProxyPasswordFromSchedD: Can't retrieve password\n" );
		return FALSE;

	}

	sock->eom();
	sock->close();
	delete sock;
	return TRUE;
}
