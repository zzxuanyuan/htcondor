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
#include "condor_string.h"
#include "internet.h"
#include "daemon.h"
#include "dc_collector.h"


DCCollector::DCCollector( const char* name, const char* pool ) 
	: Daemon( DT_COLLECTOR, name, pool )
{
	init();
}


DCCollector::DCCollector( const char* addr, int port ) 
	: Daemon( addr, port )
{
	_type = DT_COLLECTOR;
	init();
}


void
DCCollector::init( void ) 
{
	update_rsock = NULL;
	tcp_collector_host = NULL;
	tcp_collector_addr = NULL;
	tcp_collector_port = 0;
	use_tcp = false;
	update_destination = NULL;
	reconfig();
}


DCCollector::~DCCollector( void )
{
	if( update_rsock ) {
		delete( update_rsock );
	}
	if( tcp_collector_addr ) {
		delete [] tcp_collector_addr;
	}
	if( tcp_collector_host ) {
		delete [] tcp_collector_host;
	}
	if( update_destination ) {
		delete [] update_destination;
	}
}


void
DCCollector::reconfig( void )
{
	char* tmp;
	tmp = param( "TCP_COLLECTOR_HOST" );
	if( tmp ) {
		use_tcp = true;
		if( tcp_collector_host ) {
			if( strcmp(tcp_collector_host, tmp) ) { 
					// the TCP_COLLECTOR_HOST has changed...
				if( update_rsock ) {
					delete( update_rsock );
					update_rsock = NULL;
				}
				delete [] tcp_collector_host;
				tcp_collector_host = strnewp( tmp );
			}
		} else {
				// nothing set yet, so store it now
			tcp_collector_host = strnewp( tmp );
		}
		free( tmp );
	}
	tmp = param( "UPDATE_COLLECTOR_WITH_TCP" );
	if( tmp ) {
		use_tcp = true;
		free( tmp );
	}
	if( ! _addr ) {
		locate();
	}
	parseTCPInfo();

	if( update_destination ) {
		delete [] update_destination;
		update_destination = NULL;
	}
	displayResults();
}


void
DCCollector::parseTCPInfo( void )
{
	if( ! use_tcp ) {
			// nothing to do
		return;
	}
	if( tcp_collector_addr ) {
		delete [] tcp_collector_addr;
		tcp_collector_addr = NULL;
	}

	if( ! tcp_collector_host ) {
			// they want to use TCP but haven't given us a specific
			// host to use.  so, use the default collector...
		tcp_collector_port = _port;
		tcp_collector_addr = strnewp( _addr );
	} else {
			// they gave us a specific string.  parse it so we know
			// where to send the TCP updates.  this is in case they
			// want to setup a tree of collectors, etc.
		if( is_valid_sinful(tcp_collector_host) ) {
			tcp_collector_addr = strnewp( tcp_collector_host );
			tcp_collector_port = string_to_port( tcp_collector_host );
			return;
		} 

			// if we're still here, they didn't give us a valid
			// "sinful string", so see if they specified a port... 
		char* host = strnewp( tcp_collector_host );
		char* colon = NULL;
		if( !(colon = strchr(host, ':')) ) {
				// no colon, use the default port, and treat the given
				// string as the address.
			tcp_collector_port = COLLECTOR_PORT;
			tcp_collector_addr = strnewp( tcp_collector_host );
		} else { 
				// there's a colon, so grab what's after it for the
				// port, and what's before it for the address.
			*colon = '\0';
			tcp_collector_addr = strnewp( host );
			colon++;
			tcp_collector_port = atoi( colon );
		}
		delete [] host;
	}
}


bool
DCCollector::sendUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 ) 
{
	dprintf( D_FULLDEBUG, 
			 "Attempting to send update via %s to collector %s\n",
			 use_tcp ? "TCP" : "UDP", update_destination );  
	if( use_tcp ) {
		return sendTCPUpdate( cmd, ad1, ad2 );
	}
	return sendUDPUpdate( cmd, ad1, ad2  );
}


bool
DCCollector::sendUDPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 )
{
		// with UDP it's pretty straight forward.  We always want to
		// use Daemon::startCommand() so we get all the security stuff
		// in every update.  In fact, we want to recreate the SafeSock
		// itself each time, since it doesn't seem to work if we reuse
		// the SafeSock object from one update to the next...
	SafeSock ssock;
	ssock.timeout( 30 );
	ssock.encode();

		// since we're dealing w/ UDP here, we can use all the info
		// already stored in the generic Daemon object data members
		// for this...
	if( ! ssock.connect(_addr, _port) ) {
		MyString err_msg = "Failed to connect to collector ";
		err_msg += update_destination;
		newError( err_msg.Value() );
		return false;
	}

	if( ! startCommand(cmd, &ssock, 20) ) { 
		newError( "Failed to send UDP update command to collector" );
		return false;
	}

	ssock.encode();
	if( ad1 && ! ad1->put(ssock) ) {
		newError( "Failed to send ClassAd #1 to collector via UDP" );
		return false;
	}
	if( ad2 && ! ad2->put(ssock) ) {
		newError( "Failed to send ClassAd #2 to collector via UDP" );
		return false;
	}
	if( ! ssock.eom() ) {
		newError( "Failed to send EOM to collector via UDP" );
		return false;
	}
	return true;
}


bool
DCCollector::sendTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 )
{
	if( ! update_rsock ) {
			// we don't have a TCP sock for sending an update.  we've
			// got to create one.  this is a somewhat complicated
			// process, since we've got to create a ReliSock, connect
			// to the collector, and start a security session.
			// unfortunately, the only way to start a security session
			// is to send an initial command, so we'll handle that
			// update at the same time.  if the security API changes
			// in the future, we'll be able to make this code a little
			// more straight-forward...
		return initiateTCPUpdate( cmd, ad1, ad2 );
	}

		// otherwise, we've already got our socket, it's connected,
		// the security session is going, etc.  so, all we have to do
		// is send the actual update to the existing socket.  the only
		// thing we have to watch out for is if the collector
		// invalidated our cached socket, and if so, we'll have to
		// start another connection.

	bool had_failure = false;
	update_rsock->encode();
	if( ! update_rsock->code(cmd) ) {
		had_failure = true;
	}
	if( !had_failure && ad1 && ! ad1->put(*update_rsock) ) {
		had_failure = true;
	}
	if( !had_failure && ad2 && ! ad2->put(*update_rsock) ) {
		had_failure = true;
	}
	if( !had_failure && ! update_rsock->eom() ) {
		had_failure = true;
	}
	if( !had_failure ) {
		return true;
	}
	dprintf( D_FULLDEBUG, 
			 "Couldn't reuse TCP socket to update collector, "
			 "starting new connection\n" );
	delete update_rsock;
	update_rsock = NULL;
	return initiateTCPUpdate( cmd, ad1, ad2 );
}



bool
DCCollector::initiateTCPUpdate( int cmd, ClassAd* ad1, ClassAd* ad2 )
{
	if( update_rsock ) {
		delete update_rsock;
		update_rsock = NULL;
	}
	update_rsock = new ReliSock;
	update_rsock->timeout( 30 );
	update_rsock->encode();
	if( ! update_rsock->connect(tcp_collector_addr, tcp_collector_port) ) {
		MyString err_msg = "Failed to connect to collector ";
		err_msg += updateDestination();
		newError( err_msg.Value() );
		delete update_rsock;
		update_rsock = NULL;
		return false;
	}
	if( ! startCommand(cmd, update_rsock, 20) ) { 
		newError( "Failed to send TCP update command to collector" );
		return false;
	}
	update_rsock->encode();
	if( ad1 && ! ad1->put(*update_rsock) ) {
		newError( "Failed to send ClassAd #1 to collector via TCP" );
		return false;
	}
	if( ad2 && ! ad2->put(*update_rsock) ) {
		newError( "Failed to send ClassAd #2 to collector via TCP" );
		return false;
	}
	if( ! update_rsock->eom() ) {
		newError( "Failed to send EOM to collector via TCP" );
		return false;
	}
	return true;
}


void
DCCollector::displayResults( void )
{
	dprintf( D_FULLDEBUG, "Will use %s to update collector %s\n", 
			 use_tcp ? "TCP" : "UDP", updateDestination() );
}


// Figure out how we're going to identify the destination for our
// updates when printing to the logs, etc. 
const char*
DCCollector::updateDestination( void )
{
	if( update_destination ) {
		return update_destination;
	}
	MyString dest;
	bool use_daemon_info = false;
	if( use_tcp ) {
		if( ! tcp_collector_host ) { 
				// they didn't supply anything, so we should use the
				// info in the Daemon part of ourself...
			use_daemon_info = true;
		} else if( is_valid_sinful(tcp_collector_host) ) { 
				// they gave us a specific host, but it's already in
				// sinful-string form, so that's all we can do...
			update_destination = strnewp( tcp_collector_host );
		} else {
				// they gave us either an IP or a hostname, either
				// way... use what they gave us, and tack on the port
				// we're using (which either came from them, or is the
				// default COLLECTOR_PORT if unspecified).
			dest = tcp_collector_addr;
			char buf[64];
			sprintf( buf, "%d", tcp_collector_port );
			dest += " (port: ";
			dest += buf;
			dest += ')';
			update_destination = strnewp( dest.Value() );
		}
	} else {
		use_daemon_info = true;
	}
	if( ! update_destination && use_daemon_info ) { 
		if( _full_hostname ) {
			dest = _full_hostname;
			dest += ' ';
			dest += _addr;
		} else {
			dest = _addr;
		}
		update_destination = strnewp( dest.Value() );
	}
	return update_destination;
}
