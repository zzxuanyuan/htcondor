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
#include "condor_config.h"
#include "condor_ver_info.h"

#include "daemon.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "condor_query.h"
#include "get_daemon_addr.h"
#include "get_full_hostname.h"
#include "my_hostname.h"
#include "internet.h"
#include "HashTable.h"
#include "KeyCache.h"


KeyCacheByAddr* Daemon::enc_key_cache = NULL;
int Daemon::enc_key_daemon_ref_count = 0;


// Hash function for encryption key hash table
static int
compute_enc_hash(struct sockaddr_in * const &addr, int numBuckets)
{
    unsigned int h = 0;
	char *a = sin_to_string(addr);
	for (int n = 0; a[n]; n++) {
		h = (h * 37 + a[n]) % (MAXINT / 37);
	}
	return ( h % numBuckets );
}


Daemon::Daemon( char* sinful_addr, int port ) 
{
	_type = DT_ANY;
	_port = 0;
	_is_local = false;
	_tried_locate = true;
	_auth_cap_known = false;
	_is_auth_cap = false;

	_addr = NULL;
	_name = NULL;
	_pool = NULL;
	_version = NULL;
	_error = NULL;
	_id_str = NULL;
	_hostname = NULL;
	_full_hostname = NULL;

	if( sinful_addr && is_valid_sinful(sinful_addr) ) {
		if (port) {
			char new_sinful_addr[128] = {0};
			int iplen = strchr( sinful_addr, ':') - sinful_addr;
			strncpy (new_sinful_addr, sinful_addr, iplen);
			sprintf (new_sinful_addr + iplen, ":%i>", port);
			_addr = strnewp( new_sinful_addr );
		} else {
			_addr = strnewp( sinful_addr );
		}
		_port = string_to_port( _addr );
	} else {
		_addr = NULL;
		_port = 0;
	}

	if (enc_key_daemon_ref_count == 0) {
		printf ("ZKM: creating enc key table!\n");
		enc_key_cache = new KeyCacheByAddr(59, &compute_enc_hash, rejectDuplicateKeys);
	}
	enc_key_daemon_ref_count++;
}


Daemon::Daemon( daemon_t type, const char* name, const char* pool ) 
{
	_type = type;
	_port = 0;
	_is_local = false;
	_tried_locate = false;
	_auth_cap_known = false;
	_is_auth_cap = false;

	_addr = NULL;
	_name = NULL;
	_version = NULL;
	_error = NULL;
	_id_str = NULL;
	_hostname = NULL;
	_full_hostname = NULL;

	if( pool ) {
		_pool = strnewp( pool );
	} else {
		_pool = NULL;
	}

	if( name && name[0] ) {
		if( is_valid_sinful(name) ) {
			_addr = strnewp( name );
		} else {
			_name = strnewp( name );
		}
	} 

	if (enc_key_daemon_ref_count == 0) {
		printf ("ZKM: creating enc key table!\n");
		enc_key_cache = new KeyCacheByAddr(101, &compute_enc_hash, rejectDuplicateKeys);
	}
	enc_key_daemon_ref_count++;
}


Daemon::~Daemon() 
{
	if( _name ) delete [] _name;
	if( _pool ) delete [] _pool;
	if( _addr ) delete [] _addr;
	if( _error ) delete [] _error;
	if( _id_str ) delete [] _id_str;
	if( _hostname ) delete [] _hostname;
	if( _full_hostname ) delete [] _full_hostname;
	if( _version ) delete [] _version;

	enc_key_daemon_ref_count--;
	if (enc_key_daemon_ref_count == 0) {
		printf ("ZKM: deleting enc key table!\n");
		// need to walk table and delete individual keys
		delete enc_key_cache;
	}

}


//////////////////////////////////////////////////////////////////////
// Data-providing methods
//////////////////////////////////////////////////////////////////////

char*
Daemon::name( void )
{
	if( ! _name ) {
		locate();
	}
	return _name;
}


char*
Daemon::hostname( void )
{
	if( ! _hostname ) {
		locate();
	}
	return _hostname;
}


char*
Daemon::version( void )
{
	if( ! _version ) {
		locate();
	}
	return _version;
}


char*
Daemon::fullHostname( void )
{
	if( ! _full_hostname ) {
		locate();
	}
	return _full_hostname;
}


char*
Daemon::addr( void )
{
	if( ! _addr ) {
		locate();
	}
	return _addr;
}


char*
Daemon::pool( void )
{
	if( ! _pool ) {
		locate();
	}
	return _pool;
}


int
Daemon::port( void )
{
	if( _port < 0 ) {
		locate();
	}
	return _port;
}


const char*
Daemon::idStr( void )
{
	if( _id_str ) {
		return _id_str;
	}
	if( ! locate() ) {
		return "unknown daemon";
	}
	char buf[128];
	if( _is_local ) {
		sprintf( buf, "local %s", daemonString(_type) );
	} else if( _name ) {
		sprintf( buf, "%s %s", daemonString(_type), _name );
	} else if( _addr ) {
		sprintf( buf, "%s at %s", daemonString(_type), _addr );
		if( _full_hostname ) {
			strcat( buf, " (" );
			strcat( buf, _full_hostname );
			strcat( buf, ")" );
		}
	} else {
		EXCEPT( "Daemon::idStr: locate() successful but _addr not found" );
	}
	_id_str = strnewp( buf );
	return _id_str;
}


void
Daemon::display( int debugflag ) 
{
	dprintf( debugflag, "Type: %d (%s), Name: %s, Addr: %s\n", 
			 (int)_type, daemonString(_type), 
			 _name ? _name : "(null)", 
			 _addr ? _addr : "(null)" );
	dprintf( debugflag, "FullHost: %s, Host: %s, Pool: %s, Port: %d\n", 
			 _full_hostname ? _full_hostname : "(null)",
			 _hostname ? _hostname : "(null)", 
			 _pool ? _pool : "(null)", _port );
	dprintf( debugflag, "IsLocal: %s, IdStr: %s, Error: %s\n", 
			 _is_local ? "Y" : "N",
			 _id_str ? _id_str : "(null)", 
			 _error ? _error : "(null)" );
}


void
Daemon::display( FILE* fp ) 
{
	fprintf( fp, "Type: %d (%s), Name: %s, Addr: %s\n", 
			 (int)_type, daemonString(_type), 
			 _name ? _name : "(null)", 
			 _addr ? _addr : "(null)" );
	fprintf( fp, "FullHost: %s, Host: %s, Pool: %s, Port: %d\n", 
			 _full_hostname ? _full_hostname : "(null)",
			 _hostname ? _hostname : "(null)", 
			 _pool ? _pool : "(null)", _port );
	fprintf( fp, "IsLocal: %s, IdStr: %s, Error: %s\n", 
			 _is_local ? "Y" : "N",
			 _id_str ? _id_str : "(null)", 
			 _error ? _error : "(null)" );
}


//////////////////////////////////////////////////////////////////////
// Communication methods
//////////////////////////////////////////////////////////////////////

ReliSock*
Daemon::reliSock( int sec )
{
	if( ! _addr ) {
		if( ! locate() ) {
			return NULL;
		}
	}
	ReliSock* reli;
	reli = new ReliSock();
	if( sec ) {
		reli->timeout( sec );
	}
	if( reli->connect(_addr, 0) ) {
		return reli;
	} else {
		delete reli;
		return NULL;
	}
}


SafeSock*
Daemon::safeSock( int sec )
{
	if( ! _addr ) {
		if( ! locate() ) {
			return NULL;
		}
	}
	SafeSock* safe;
	safe = new SafeSock();
	if( sec ) {
		safe->timeout( sec );
	}
	if( safe->connect(_addr, 0) ) {
		return safe;
	} else {
		delete safe;
		return NULL;
	}
}


Sock*
Daemon::startCommand( int cmd, Stream::stream_type st, int sec )
{
	Sock* sock;
	switch( st ) {
	case Stream::reli_sock:
		sock = reliSock();
		break;
	case Stream::safe_sock:
		sock = safeSock();
		break;
	default:
		EXCEPT( "Unknown stream_type (%d) in Daemon::startCommand",
				(int)st );
	}
	if( ! sock ) {
			// _error will already be set.
		return NULL;
	}


	if (startCommand ( cmd, sock, sec )) {
		return sock;
	} else {
		delete sock;
		return NULL;
	}

}


bool
Daemon::startCommand( int cmd, Sock* sock, int sec )
{
	// the classad for sending
	ClassAd auth_info;

	// temp vars for putting the classad together
	char *buf;

	// for those of you reading this code, a 'paramer'
	// is a thing that param()s.
	char *paramer;


	// basic sanity check
	if( ! sock ) {
		dprintf ( D_ALWAYS, "startCommand() called with a NULL Sock*, failing." );
		return false;
	} else {
		dprintf ( D_SECURITY, "STARTCOMMAND: starting %i to %s.\n", cmd, sin_to_string(sock->endpoint()));
	}

	// set up the timeout
	if( sec ) {
		sock->timeout( sec );
	}


	// gather info we need from the config file.  this will determine our
	// course of action.

	// default value
	bool always_authenticate = false; 

	paramer = param("ALWAYS_AUTHENTICATE");
	if (paramer) {
		dprintf (D_SECURITY, "STARTCOMMAND: param(ALWAYS_AUTHENTICATE)"
					" == %s\n", paramer);
		if (paramer[0] == 'Y' || paramer[0] == 'y' ||
				paramer[0] == 'T' || paramer[0] == 't') {
			dprintf ( D_SECURITY, "STARTCOMMAND: forcing authentication.\n");
			always_authenticate = true;
		}
		free(paramer);
	} else {
		dprintf (D_SECURITY, "STARTCOMMAND: param(ALWAYS_AUTHENTICATE) "
					"-> NULL, assuming FALSE.\n" );
	}


	// regarding DISABLE_AUTH_NEGOTIATION:
	// the default is disabled
	// the config can override that to be enabled
	// the version can override that to be disabled

	bool disable_auth_negotiation = true;

	paramer = param("DISABLE_AUTH_NEGOTIATION");
	if (paramer) {
		dprintf (D_SECURITY, "STARTCOMMAND: param(DISABLE_AUTH_NEGOTIATION)"
					" == %s\n", paramer);
		if ((paramer[0] == 'N') || (paramer[0] == 'n') ||
			(paramer[0] == 'F') || (paramer[0] == 'f' )) {
			disable_auth_negotiation = false;
		}
		free(paramer);
	} else {
		dprintf (D_SECURITY, "STARTCOMMAND: param(DISABLE_AUTH_NEGOTIATION) -> NULL, "
					"assuming TRUE.\n");
	}

	// look at the version if we haven't and it is available
	if (_version) {
		dprintf(D_SECURITY, "STARTCOMMAND: talking to a %s daemon.\n", _version);

		CondorVersionInfo vi(_version);
		if ( !vi.built_since_version(6,3,1) ) {
			disable_auth_negotiation = true;
			dprintf (D_SECURITY, "STARTCOMMAND: disabling auth "
						"negotiation to talk to pre 6.3.1.\n");
		}
	}


	// find out if we have previosly authenticated with them.
	bool previously_auth = false;

	// see if we have a cached key
	KeyCacheEntry *enc_key = NULL;
	previously_auth = (enc_key_cache->lookup(sock->endpoint(), enc_key) == 0);
	if (previously_auth) {
		dprintf (D_SECURITY, "STARTCOMMAND: found cached key id %i.\n", enc_key->id());
	}


choose_action:
	// possible courses of authentication action:
	const int AUTH_FAIL     = 0;
	const int AUTH_OLD      = 1;
	const int AUTH_NO       = 2;
	const int AUTH_ASK      = 3;
	const int AUTH_YES      = 4;
	const int AUTH_VIA_ENC  = 5;

	int authentication_action = AUTH_NO;

	// HACK!!!!!
	// FOR TESTING ONLY
	// <<< === >>>
	// ZKM
	if (sock->type() == Stream::safe_sock) {
		dprintf ( D_SECURITY, "STARTCOMMAND: UDP will use 6.2 style command.\n");
		authentication_action = AUTH_OLD;
	}
	else

	// END HACK


	// now the decision
	if (always_authenticate) {
		if (disable_auth_negotiation) {
			authentication_action = AUTH_FAIL;
		} else {
			if (previously_auth) {
				authentication_action = AUTH_VIA_ENC;
			} else {
				authentication_action = AUTH_YES;
			}
		}
	} else {
		if (disable_auth_negotiation) {
			authentication_action = AUTH_OLD;
		} else {
			if (previously_auth) {
				// the reason for this is simple... if we already have an
				// encryption key for them, they must have demanded it in
				// the past.  rather then ask them every time, we'll just
				// keep using it.
				authentication_action = AUTH_VIA_ENC;
			} else {
				authentication_action = AUTH_ASK;
			}
		}
	}
	dprintf ( D_SECURITY, "STARTCOMMAND: auth_action is %i.\n", authentication_action);


	// now take action.
	// a couple of the cases are handled easily:

	sock->encode();

	// AUTH_FAIL - we demand auth, the daemon does not support it
	if (authentication_action == AUTH_FAIL) {
		// cannot possibly authenticate to pre-6.3.1,
		// so exit with error.
		dprintf(D_SECURITY, "STARTCOMMAND: cannot authenticate"
					" with pre 6.3.1\n");

		return false;
	}

	// AUTH_OLD - we support auth, daemon does not.  command is sent
	// like it was in pre 6.3, just code an int and be done.
	if (authentication_action == AUTH_OLD) {
		dprintf(D_SECURITY, "STARTCOMMAND: sending unauthenticated command (%i)\n", cmd);

		// just code the command and be done
		sock->code(cmd);

		// we must _NOT_ do an eom() here!  Ques?  See Todd or Zach 9/01

		return true;
	}


	// if we've made it here, we need to talk with the other side
	// to either tell them what to do or ask what they want to do.

	dprintf ( D_SECURITY, "STARTCOMMAND: negotiating auth for command %i.\n", cmd);




	// package a ClassAd together

	// allocate a buffer big enough to work with all fields
	int buflen = 128;

	paramer = param("AUTHENTICATION_METHODS");
	if (paramer != NULL) {
		dprintf ( D_SECURITY, "STARTCOMMAND: param(AUTHENTICATION_METHODS) == %s\n", paramer );
		// expand the buffer size to hold the names of all the methods
		buflen += strlen(paramer);
	} else {
		dprintf ( D_SECURITY, "STARTCOMMAND: param(AUTHENTICATION_METHODS) -> NULL!\n" );
	}

	buf = new char[buflen];
	if (buf == NULL) {
		dprintf ( D_ALWAYS, "STARTCOMMAND: new failed!\n" );
		if (paramer) {
			delete paramer;
		}
		return false;
	}


	// fill in auth_types
	if (paramer) {
		sprintf(buf, "%s=\"%s\"", ATTR_AUTH_TYPES, paramer);
		free(paramer);
	} else {
#if defined(WIN32)
		// default windows method
		sprintf(buf, "%s=\"NTSSPI\"", ATTR_AUTH_TYPES);
#else
		// default unix method
		sprintf(buf, "%s=\"FS\"", ATTR_AUTH_TYPES);
#endif
	}

	auth_info.Insert(buf);
	dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);


	// fill in command
	sprintf(buf, "%s=%i", ATTR_AUTH_COMMAND, cmd);
	auth_info.Insert(buf);
	dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);


	// handle each of the authentication actions

	if (authentication_action == AUTH_VIA_ENC) {
		// fill in the key id
		sprintf(buf, "%s=%i", ATTR_KEY_ID, enc_key->id());
		auth_info.Insert(buf);
		dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);

		// let the other side we'll be authenticating via an encryption key
		sprintf(buf, "%s=\"VIA_ENC\"", ATTR_AUTH_ACTION);
		auth_info.Insert(buf);
		dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);

	} else if (authentication_action == AUTH_YES) {
		// tell the other side to authenticate now.
		sprintf(buf, "%s=\"YES\"", ATTR_AUTH_ACTION);
		auth_info.Insert(buf);
		dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);

	} else if (authentication_action == AUTH_ASK) {
		// ask the other side what it wants to do
		sprintf(buf, "%s=\"ASK\"", ATTR_AUTH_ACTION);
		auth_info.Insert(buf);
		dprintf ( D_SECURITY, "STARTCOMMAND: inserted '%s'\n", buf);

	} else {
		dprintf (D_ALWAYS, "STARTCOMMAND: invalid state, failing!\n");
		delete [] buf;
		return false;
	}


	// free the buffer
	delete [] buf;


	// now send the actual DC_AUTHENTICATE command
	dprintf ( D_SECURITY, "STARTCOMMAND: sending DC_AUTHENTICATE command\n");
	int authcmd = DC_AUTHENTICATE;
	if (! sock->code(authcmd)) {
		dprintf ( D_ALWAYS, "STARTCOMMAND: failed to send DC_AUTHENTICATE\n");
		return false;
	}


	// send the classad
	dprintf ( D_SECURITY, "STARTCOMMAND: sending following classad:\n");
	auth_info.dPrint ( D_SECURITY );

	if (! auth_info.put(*sock)) {
		dprintf ( D_ALWAYS, "STARTCOMMAND: failed to send auth_info\n");
		return false;
	}

	if (! sock->end_of_message()) {
		dprintf ( D_ALWAYS, "STARTCOMMAND: failed to end classad message\n");
		return false;
	}



	// if we asked them what to do, get their response

	if (authentication_action == AUTH_ASK) {

		ClassAd auth_response;
		sock->decode();

		if (!auth_response.initFromStream(*sock) ||
		    !sock->end_of_message() ) {

			dprintf ( D_SECURITY, "STARTCOMMAND: server did not respond, failing\n");
			return false;
		}


		dprintf ( D_SECURITY, "STARTCOMMAND: server responded with:\n");
		auth_response.dPrint( D_SECURITY );

		char buf[128];

		if (auth_response.LookupString(ATTR_VERSION, buf)) {
			dprintf ( D_SECURITY, "STARTCOMMAND: %s "
				"== %s in response ClassAd", ATTR_VERSION, buf);
			New_version(buf);
		} else {
			dprintf ( D_SECURITY, "STARTCOMMAND: no %s "
						"in response ClassAd.\n", ATTR_VERSION,
						ATTR_AUTH_ACTION );
		}

		if (!auth_response.LookupString(ATTR_AUTH_ACTION, buf)) {
			dprintf ( D_ALWAYS, "STARTCOMMAND: no %s "
						"in response ClassAd!\n", ATTR_AUTH_ACTION );
			return false;
		}

		if( buf[0] == 'Y' || buf[0] == 'y' ||
			buf[0] == 'T' || buf[0] == 't' ) {
			authentication_action = AUTH_YES;
		} else {
			authentication_action = AUTH_NO;
		}
	}
	

	// at this point, we know exactly what needs to happen.  if we asked
	// the other side, their choice is in authentication action.  if we
	// didn't ask, then our choice is in authentication_action.

	bool retval;

	if (authentication_action == AUTH_VIA_ENC) {
		if (!sock->set_crypto_key(enc_key->key())) {
			dprintf ( D_SECURITY, "STARTCOMMAND: cached key invalid (%i), removing.\n", enc_key->id());
			// a failure here signals that the cache may be invalid.
			// delete this entry from table and force normal auth.
			KeyCacheEntry * ek = NULL;
			if (enc_key_cache->lookup(sock->endpoint(), ek) == 0) {
				delete ek;
			} else {
				dprintf (D_SECURITY, "STARTCOMMAND: unable to delete KeyCacheEntry.\n");
			}
			enc_key_cache->remove(sock->endpoint());
			previously_auth = false;

			// close this connection and start a new one
			sock->close();
			if (!sock->connect(_addr, 0)) {
				dprintf ( D_ALWAYS, "STARTCOMMAND: could not reconnect to %s.", _addr);
				return false;
			}

			dprintf ( D_SECURITY, "STARTCOMMAND: will attempt to re-authenticate.\n");
			goto choose_action;

		} else {
			dprintf ( D_SECURITY, "STARTCOMMAND: successfully set crypto key!\n");
			retval = true;
		}
	}
	
	if (authentication_action == AUTH_YES) {

		assert (sock->type() == Stream::reli_sock);

		dprintf ( D_SECURITY, "STARTCOMMAND: authenticate(ki, 0xFFFF) "
					"RIGHT NOW.\n");

		// this gets created by authenticate.
		KeyInfo* ki = NULL;

		if (!sock->authenticate(ki, 0xFFFF)) {
			dprintf ( D_SECURITY, "STARTCOMMAND: authenticate failed!");
			retval = false;
		} else if (ki == NULL) {
			dprintf ( D_SECURITY, "STARTCOMMAND: did not receive crypto key... "
						"disabling crypto.\n");

			// negotiation for encryption not yet implemented.
			// ZKM: retval = !always_encrypt;
			retval = true;
		} else {
			dprintf ( D_SECURITY, "STARTCOMMAND: securely received crypto key... "
						"enabling crypto.\n" );

			if (!sock->set_crypto_key(ki)) {
				dprintf ( D_SECURITY, "STARTCOMMAND: set_crypto_key() failed!\n");
				// if it fails here, do not retry.
				retval = false;
			} else {

				// receive the key's ID
				int key_id = 0;
				sock->decode();
				if (!sock->code(key_id) || !sock->eom()) {
					dprintf (D_ALWAYS, "STARTCOMMAND: could not receive key id number.\n");
					retval = false;
				} else {
					dprintf (D_SECURITY, "STARTCOMMAND: server sent crypto key id %i.\n",
							key_id);
				}

				// cache the key
				KeyCacheEntry * nkey = new KeyCacheEntry(
											key_id,
											sock->endpoint(),
											ki,
											0);
				enc_key_cache->insert(sock->endpoint(), nkey);
				// ki is copied by KeyCacheEntry's constructor.
				// nkey is "leaked" at this point, to be cleaned up later.

				retval = true;
			}

			// clean up
			delete ki;
		}
	} else if (authentication_action == AUTH_NO) {
		dprintf ( D_SECURITY, "STARTCOMMAND: not authenticating (AUTH_NO).\n");
		retval = true;
	}

	if (retval) {
		dprintf ( D_SECURITY, "STARTCOMMAND: setting sock->encode()\n");
		dprintf ( D_SECURITY, "STARTCOMMAND: Success.\n");
		sock->encode();
		sock->allow_one_empty_message();
	} else {
		dprintf ( D_SECURITY, "STARTCOMMAND: startCommand failed.\n");
	}

	return retval;
}



bool
Daemon::sendCommand( int cmd, Sock* sock, int sec )
{
	
	if( ! startCommand( cmd, sock, sec )) {
		return false;
	}
	if( ! sock->eom() ) {
		char err_buf[256];
		sprintf( err_buf, "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( err_buf );
		return false;
	}
	return true;
}


bool
Daemon::sendCommand( int cmd, Stream::stream_type st, int sec )
{
	Sock* tmp = startCommand( cmd, st, sec );
	if( ! tmp ) {
		return false;
	}
	if( ! tmp->eom() ) {
		char err_buf[256];
		sprintf( err_buf, "Can't send eom for %d to %s", cmd,  
				 idStr() );
		newError( err_buf );
		delete tmp;
		return false;
	}
	delete tmp;
	return true;
}




//////////////////////////////////////////////////////////////////////
// Locate-related methods
//////////////////////////////////////////////////////////////////////

bool
Daemon::locate( void )
{
	char buf[256], *tmp;
	bool rval;

		// Make sure we only call locate() once.
	if( _tried_locate ) {
			// If we've already been here, return whether we found
			// addr or not, the best judge for if locate() worked.
		if( _addr ) {
			return true;
		} else {
			return false;
		}
	}
	_tried_locate = true;

		// First call a subsystem-specific helper to get everything we
		// have to.  What we do is mostly different between regular
		// daemons and CM daemons.  These must set: _addr, _port, and
		// _is_local.  If possible, they will also set _full_hostname
		// and _name. 
	switch( _type ) {
	case DT_ANY:
		// don't do anything
		break;
	case DT_SCHEDD:
		rval = getDaemonInfo( "SCHEDD", SCHEDD_AD );
		break;
	case DT_STARTD:
		rval = getDaemonInfo( "STARTD", STARTD_AD );
		break;
	case DT_MASTER:
		rval = getDaemonInfo( "MASTER", MASTER_AD );
		break;
	case DT_COLLECTOR:
		rval = getCmInfo( "COLLECTOR", COLLECTOR_PORT );
		break;
	case DT_NEGOTIATOR:
		rval = getCmInfo( "NEGOTIATOR", NEGOTIATOR_PORT );
		break;
	case DT_VIEW_COLLECTOR:
		if( (rval = getCmInfo("CONDOR_VIEW", COLLECTOR_PORT)) ) {
				// If we found it, we're done.
			break;
		} 
			// If there's nothing CONDOR_VIEW-specific, try just using
			// "COLLECTOR".
		rval = getCmInfo( "COLLECTOR", COLLECTOR_PORT ); 
		break;
	default:
		EXCEPT( "Unknown daemon type (%d) in Daemon::init", (int)_type );
	}

	if( ! rval) {
			// _error will already be set appropriately.
		return false;
	}

		// Now, deal with everything that's common between both.

		// get*Info() will usually fill in _full_hostname, but not
		// _hostname.  In all cases, if we have _full_hostname we just
		// want to trim off the domain the same way for _hostname. 
	if( _full_hostname ) {
		strcpy( buf, _full_hostname );
		tmp = strchr( buf, '.' );
		if( tmp ) {
			*tmp = '\0';
		}
		New_hostname( strnewp(buf) );
	}

		// Now that we're done with the get*Info() code, if we're a
		// local daemon and we still don't have a name, fill it in.  
	if( ! _name && _is_local) {
		_name = localName();
	}

	return true;
}


bool
Daemon::getDaemonInfo( const char* subsys, AdTypes adtype )
{
	char				buf[512], tmpname[512];
	char				*addr_file, *tmp, *my_name;
	FILE				*addr_fp;
	struct				sockaddr_in sockaddr;
	struct				hostent* hostp;

		// Figure out if we want to find a local daemon or not, and
		// fill in the various hostname fields.
	if( _name ) {
			// We were passed a name, so try to look it up in DNS to
			// get the full hostname.
		
			// First, make sure we're only trying to resolve the
			// hostname part of the name...
		strncpy( tmpname, _name, 512 );
		tmp = strchr( tmpname, '@' );
		if( tmp ) {
				// There's a '@'.
			*tmp = '\0';
				// Now, tmpname holds whatever was before the @ 
			tmp++;
			if( *tmp ) {
					// There was something after the @, try to resolve it
					// as a full hostname:
				if( ! (New_full_hostname(get_full_hostname(tmp))) ) { 
						// Given a hostname, this is a fatal error.
					sprintf( buf, "unknown host %s", tmp );  
					newError( buf );
					return false;
				} 
			} else {
					// There was nothing after the @, use localhost:
				New_full_hostname( strnewp(my_full_hostname()) );
			}
			sprintf( buf, "%s@%s", tmpname, _full_hostname );
			New_name( strnewp(buf) );
		} else {
				// There's no '@', just try to resolve the hostname.
			if( (New_full_hostname(get_full_hostname(tmpname))) ) {
				New_name( strnewp(_full_hostname) );
			} else {
					// Given a hostname, this is a fatal error.
				sprintf( buf, "unknown host %s", tmpname );  
				newError( buf );
				return false;
			}           
		}
			// Now that we got this far and have the correct name, see
			// if that matches the name for the local daemon.  
			// If we were given a pool, never assume we're local --
			// always try to query that pool...
		my_name = localName();
		if( !_pool && !strcmp(_name, my_name) ) {
			_is_local = true;
		}
		delete [] my_name;
	} else if( _addr ) {
			// We got no name, but we have an address.  Try to
			// do an inverse lookup and fill in some hostname info
			// from the IP address we already have.
		string_to_sin( _addr, &sockaddr );
		hostp = gethostbyaddr( (char*)&sockaddr.sin_addr, 
							   sizeof(struct in_addr), AF_INET ); 
		if( ! hostp ) {
			New_full_hostname( NULL );

			sprintf( buf, "cant find host info for %s", _addr );
			newError( buf );
		} else {
			New_full_hostname( strnewp(hostp->h_name) );
		}
	} else {
			// We were passed neither a name nor an address, so use
			// the local daemon.
		_is_local = true;
		New_name( localName() );
		New_full_hostname( strnewp(my_full_hostname()) );
	}

		// Now that we have the real, full names, actually find the
		// address of the daemon in question.

	if( _is_local ) {
		sprintf( buf, "%s_ADDRESS_FILE", subsys );
		addr_file = param( buf );
		if( addr_file ) {
			if( (addr_fp = fopen(addr_file, "r")) ) {
					// Read out the sinful string.
				fgets( buf, 100, addr_fp );
				fclose( addr_fp );
					// chop off the newline
				tmp = strchr( buf, '\n' );
				if( tmp ) {
					*tmp = '\0';
				}
			}
			free( addr_file );
		} 
		if( is_valid_sinful(buf) ) {
			New_addr( strnewp(buf) );
		}

		sprintf( buf, "%s", subsys );
		char* exe_file = param( buf );
		if( exe_file ) {
			char ver[128];
			CondorVersionInfo vi;
			vi.get_version_from_file(exe_file, ver, 128);
			New_version( strnewp(ver) );
		}
		free( exe_file );


	}

	if( ! _addr ) {
			// If we still don't have it (or it wasn't local), query
			// the collector for the address.
		CondorQuery			query(adtype);
		ClassAd*			scan;
		ClassAdList			ads;

		if( _type == DT_STARTD ) {
				/*
				  So long as an SMP startd has only 1 command socket
				  per startd, we want to take advantage of that and
				  query based on Machine, not Name.  This way, if
				  people supply just the hostname of an SMP, we can
				  still find the daemon.  For example, "condor_vacate
				  host" will vacate all VMs on that host, but only if
				  condor_vacate can find the address in the first
				  place.  -Derek Wright 8/19/99 
				*/
			sprintf(buf, "%s == \"%s\"", ATTR_MACHINE, _full_hostname ); 
		} else {
			sprintf(buf, "%s == \"%s\"", ATTR_NAME, _name ); 
		}
		query.addANDConstraint(buf);
		query.fetchAds(ads, _pool);
		ads.Open();
		scan = ads.Next();
		if(!scan) {
			dprintf(D_ALWAYS, "Can't find address for %s %s", 
					 daemonString(_type), _name );
			sprintf( buf, "Can't find address for %s %s", 
					 daemonString(_type), _name );
			newError( buf );
			return false; 
		}

		// construct the IP_ADDR attribute
		sprintf( tmpname, "%sIpAddr", subsys );
		if(scan->EvalString( tmpname, NULL, buf ) == FALSE) {
			dprintf(D_ALWAYS, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			sprintf( buf, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			newError( buf );
			return false;
		}
		New_addr( strnewp(buf) );

		sprintf( tmpname, ATTR_VERSION );
		if(scan->EvalString( tmpname, NULL, buf ) == FALSE) {
			dprintf(D_ALWAYS, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			sprintf( buf, "Can't find %s in classad for %s %s",
					 tmpname, daemonString(_type), _name );
			newError( buf );
			return false;
		}
		dprintf (D_ALWAYS, "VERSION: %s\n", buf);
		New_version( strnewp(buf) );

	}

		// Now that we have the sinful string, fill in the port. 
	_port = string_to_port( _addr );
	return true;
}


bool
Daemon::getCmInfo( const char* subsys, int port )
{
	char buf[128];
	char* host = NULL;
	char* local_host = NULL;
	char* remote_host = NULL;
	char* tmp;
	struct in_addr sin_addr;
	struct hostent* hostp;

		// We know this without any work.
	_port = port;

		// For CM daemons, normally, we're going to be local (we're
		// just not sure which config parameter is going to find it
		// for us).  So, by default, we want _is_local set to true,
		// and only if either _name or _pool are set do we change
		// _is_local to false.  
	_is_local = true;

		// For CM daemons, the "pool" and "name" should be the same
		// thing.  See if either is set, and if so, use it for both.  
	if( _name && ! _pool ) {
		New_pool( strnewp(_name) );
	} else if ( ! _name && _pool ) {
		New_name( strnewp(_pool) );
	} else if ( _name && _pool ) {
		if( strcmp(_name, _pool) ) {
				// They're different, this is bad.
			EXCEPT( "Daemon: pool (%s) and name (%s) conflict for %s",
					_pool, _name, subsys );
		}
	}

		// Figure out what name we're really going to use.
	if( _name && *_name ) {
			// If we were given a name, use that.
		remote_host = strdup( _name );
		host = remote_host;
		_is_local = false;
	}

		// Try the config file for a subsys-specific IP addr 
	sprintf( buf, "%s_IP_ADDR", subsys );
	local_host = param( buf );

	if( ! local_host ) {
			// Try the config file for a subsys-specific hostname 
		sprintf( buf, "%s_HOST", subsys );
		local_host = param( buf );
	}

	if( ! local_host ) {
			// Try the generic CM_IP_ADDR setting (subsys-specific
			// settings should take precedence over this). 
		local_host = param( "CM_IP_ADDR" );
	}

	if( local_host && ! host ) {
		host = local_host;
	}
	if( local_host && remote_host && !strcmp(local_host, remote_host) ) { 
			// We've got the same thing, we're really local, even
			// though we were given a "remote" host.
		_is_local = true;
		host = local_host;
	}
	if( ! host ) {
		sprintf( buf, "%s address or hostname not specified in config file",
				 subsys ); 
		newError( buf );
		if( local_host ) free( local_host );
		if( remote_host ) free( remote_host );
		return false;
	} 

	if( is_ipaddr(host, &sin_addr) ) {
		sprintf( buf, "<%s:%d>", host, _port );
		if( local_host ) free( local_host );
		if( remote_host ) free( remote_host );
		New_addr( strnewp(buf) );

			// See if we can get the canonical name
		hostp = gethostbyaddr( (char*)&sin_addr, 
							   sizeof(struct in_addr), AF_INET ); 
		if( ! hostp ) {
			New_full_hostname( NULL );
			sprintf( buf, "can't find host info for %s", _addr );
			newError( buf );
		} else {
			New_full_hostname( strnewp(hostp->h_name) );
		}
	} else {
			// We were given a hostname, not an address.
		tmp = get_full_hostname( host, &sin_addr );
		if( ! tmp ) {
				// With a hostname, this is a fatal Daemon error.
			sprintf( buf, "unknown host %s", host );
			newError( buf );
			if( local_host ) free( local_host );
			if( remote_host ) free( remote_host );
			return false;
		}
		if( local_host ) free( local_host );
		if( remote_host ) free( remote_host );
		sprintf( buf, "<%s:%d>", inet_ntoa(sin_addr), _port );
		New_addr( strnewp(buf) );
		New_full_hostname( tmp );
	}

		// For CM daemons, we always want the name to be whatever the
		// full_hostname is.
	New_name( strnewp(_full_hostname) );

		// If the pool was set, we want to use the full-hostname for
		// that, too.
	if( _pool ) {
		New_pool( strnewp(_full_hostname) );
	}

	return true;
}


//////////////////////////////////////////////////////////////////////
// Other helper methods
//////////////////////////////////////////////////////////////////////

void
Daemon::newError( char* str )
{
	if( _error ) {
		delete [] _error;
	}
	_error = strnewp( str );
}


char*
Daemon::localName( void )
{
	char buf[100], *tmp, *my_name;
	sprintf( buf, "%s_NAME", daemonString(_type) );
	tmp = param( buf );
	if( tmp ) {
		my_name = build_valid_daemon_name( tmp );
		free( tmp );
	} else {
		my_name = strnewp( my_full_hostname() );
	}
	return my_name;
}


char*
Daemon::New_full_hostname( char* str )
{
	if( _full_hostname ) {
		delete [] _full_hostname;
	} 
	_full_hostname = str;
	return str;
}


char*
Daemon::New_hostname( char* str )
{
	if( _hostname ) {
		delete [] _hostname;
	} 
	_hostname = str;
	return str;
}


char*
Daemon::New_addr( char* str )
{
	if( _addr ) {
		delete [] _addr;
	} 
	_addr = str;
	return str;
}

char*
Daemon::New_version ( char* ver )
{
	if( _version ) {
		delete [] _version;
	} 
	_version = ver;
	return ver;
}


char*
Daemon::New_name( char* str )
{
	if( _name ) {
		delete [] _name;
	} 
	_name = str;
	return str;
}


char*
Daemon::New_pool( char* str )
{
	if( _pool ) {
		delete [] _pool;
	} 
	_pool = str;
	return str;
}
