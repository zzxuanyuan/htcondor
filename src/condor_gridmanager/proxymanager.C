/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "condor_ckpt_name.h"
#include "condor_config.h"
#include "classad_hashtable.h"
#include "util_lib_proto.h"
#include "globus_utils.h"

#include "proxymanager.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, Proxy *>;
template class HashBucket<HashKey, Proxy *>;
template class HashTable<HashKey, ProxySubject *>;
template class HashBucket<HashKey, ProxySubject *>;
template class List<Proxy>;
template class Item<Proxy>;

HashTable <HashKey, Proxy *> ProxiesByFilename( HASH_TABLE_SIZE,
												hashFunction );
HashTable <HashKey, ProxySubject *> SubjectsByName( 50, hashFunction );

static bool proxymanager_initialized = false;
static int CheckProxies_tid = TIMER_UNSET;

int CheckProxies_interval = 600;		// default value
int minProxy_time = 3 * 60;				// default value

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
// which the module should place the "master" proxy file.
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

// An entity (e.g. GlobusJob, GlobusResource object) should call this
// function when it wants to use a proxy managed by ProxyManager. proxy_path
// is the path to the proxy it wants to use. notify_tid is a timer id that
// will be signalled when something interesting happens with the proxy
// (it's about to expire or has been refreshed). A Proxy struct will be
// returned. When the Proxy is no longer needed, ReleaseProxy() should be
// called with it. No blocking operations are performed, and the proxy will
// not be cached in the GAHP server when AcquireProxy() returns.
// If Proxy.id is set to a negative value, it's not ready for
// use yet with any GAHP commands. Once it is ready, Proxy.id
// will be set to set to the GAHP cache id (a non-negative number) and
// notify_tid will be signalled. If no notifications are desired, give a
// negative number for notify_tid or omit it. Note the the Proxy returned
// is a shared data-structure and shouldn't be delete'd or modified by
// the caller. If NULL is given for proxy_path, a refernce to the "master"
// proxy is returned.
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
	// create a new Proxy struct and...
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
dprintf(D_FULLDEBUG,"*** allocated new proxy %d, path=%s\n",proxy->id,proxy->proxy_filename);

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
dprintf(D_FULLDEBUG,"*** allocated new master proxy %d, path=%s\n",new_master->id,new_master->proxy_filename);

		proxy_subject->master_proxy = new_master;

		SubjectsByName.insert(HashKey(proxy_subject->subject_name),
							  proxy_subject);
	}

	proxy_subject->proxies.Append( proxy );

	proxy->subject = proxy_subject;

	if ( proxy->expiration_time > proxy_subject->master_proxy->expiration_time ) {
dprintf(D_FULLDEBUG,"*** found new source (%d,%s) for master proxy (%d,%s)\n",proxy->id,proxy->proxy_filename,proxy_subject->master_proxy->id,proxy_subject->master_proxy->proxy_filename);
			SetMasterProxy( proxy_subject->master_proxy, proxy );
	}

	free( subject_name );

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

void doCheckProxies()
{
	if ( CheckProxies_tid != TIMER_UNSET ) {
		daemonCore->Reset_Timer( CheckProxies_tid, 0 );
	}
}

// Most of the heavy lifting occurs here. All interaction with the GAHP
// server (aside from startup) happens here. This function is called
// periodically to check for updated proxies. It can be called earlier
// if a new proxy shows up, an old proxy can be removed, or a proxy is
// about to expire.
int CheckProxies()
{
	int now = time(NULL);
	int next_check = CheckProxies_interval + now;
	ProxySubject *curr_subject;

	SubjectsByName.startIterations();

	while ( SubjectsByName.iterate( curr_subject ) != 0 ) {

		Proxy *curr_proxy;
		Proxy *new_master = curr_subject->master_proxy;

		curr_subject->proxies.Rewind();

		while ( curr_subject->proxies.Next( curr_proxy ) != false ) {

dprintf(D_FULLDEBUG,"*** checking proxy %d, path=%s\n",curr_proxy->id,curr_proxy->proxy_filename);
			curr_proxy->near_expired =
				(curr_proxy->expiration_time - now) <= minProxy_time;

			int new_expiration =
				x509_proxy_expiration_time( curr_proxy->proxy_filename );

			if ( new_expiration > curr_proxy->expiration_time ) {

				curr_proxy->expiration_time = new_expiration;

				curr_proxy->near_expired =
					(curr_proxy->expiration_time - now) <= minProxy_time;

				int tid;
				curr_proxy->notification_tids.Rewind();
				while ( curr_proxy->notification_tids.Next( tid ) ) {
					daemonCore->Reset_Timer( tid, 0 );
				}
dprintf(D_FULLDEBUG,"*** found updated proxy %d, path=%s\n",curr_proxy->id,curr_proxy->proxy_filename);

				if ( curr_proxy->expiration_time > new_master->expiration_time ) {
					new_master = curr_proxy;
				}

			} else if ( curr_proxy->near_expired ) {

				int tid;
				curr_proxy->notification_tids.Rewind();
				while ( curr_proxy->notification_tids.Next( tid ) ) {
					daemonCore->Reset_Timer( tid, 0 );
				}

			}

			if ( curr_proxy->expiration_time - minProxy_time < next_check &&
				 !curr_proxy->near_expired ) {
				next_check = curr_proxy->expiration_time - minProxy_time;
			}

		}

		if ( new_master != curr_subject->master_proxy ) {
			
dprintf(D_FULLDEBUG,"*** found new source (%d,%s) for master proxy (%d,%s)\n",new_master->id,new_master->proxy_filename,curr_subject->master_proxy->id,curr_subject->master_proxy->proxy_filename);
			SetMasterProxy( curr_subject->master_proxy, new_master );

		}

	}

	// next_check is the absolute time of the next check, convert it to
	// a relative time (from now)
	daemonCore->Reset_Timer( CheckProxies_tid, next_check - now );

	return TRUE;
}
