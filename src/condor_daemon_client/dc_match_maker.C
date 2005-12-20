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
#include "condor_config.h"
#include "condor_string.h"
#include "condor_ver_info.h"
#include "condor_attributes.h"

#include "daemon.h"
#include "dc_match_maker.h"
#include "dc_match_maker_lease.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"
#include "newclassad_stream.h"
using namespace std;


DCMatchLite::DCMatchLite( const char* name, const char *pool )
		: Daemon( DT_MATCH_LITE, name, pool )
{
}


DCMatchLite::~DCMatchLite( void )
{
}


bool
DCMatchLite::getMatches( const classad::ClassAd &request_ad,
						 list< DCMatchLiteLease *> &leases )
{
		// Create the ReliSock
	CondorError errstack;
	ReliSock *rsock = (ReliSock *)startCommand(
			MATCHLITE_GET_MATCH, Stream::reli_sock, 20 );
	if ( ! rsock ) {
		return false;
	}

		// Serialize the classad onto the wire
	if ( !StreamPut( rsock, request_ad ) ) {
		delete rsock;
		return false;
	}

	rsock->eom();

		// Receive the return code
	rsock->decode();

	int		rc = 0;
	if ( !rsock->code( rc ) || ( rc != OK ) ) {
		return false;
	}

	int		num_matches;
	if ( !rsock->code( num_matches ) ) {
		delete rsock;
		return false;
	}
	if ( num_matches < 0 ) {
		rsock->close( );
		delete rsock;
		return true;
	}
	for (int num = 0;  num < num_matches;  num++ ) {
		classad::ClassAd	*ad = new classad::ClassAd( );
		if ( !StreamGet( rsock, *ad ) ) {
			delete rsock;
			delete ad;
			return false;
		}
		DCMatchLiteLease *lease = new DCMatchLiteLease( ad );
		leases.push_back( lease );
	}

	rsock->close();
	delete rsock;
	return true;
}

bool
DCMatchLite::getMatches( const char *requestor_name,
						 int number_requested,
						 int duration,
						 const char *requirements,
						 const char *rank,
						 list< DCMatchLiteLease *> &leases )
{
	if ( ( ! requestor_name ) ||
		 ( number_requested < 0 ) ||
		 ( duration < 0 ) ) {
		return false;
	}

	classad::ClassAd	ad;
	ad.InsertAttr( "Name", requestor_name );
	ad.InsertAttr( "RequestCount", number_requested );
	ad.InsertAttr( "LeaseDuration", duration );
	if ( requirements ) {
		classad::ClassAdParser	parser;
		classad::ExprTree	*expr = parser.ParseExpression( requirements );
		ad.Insert( "Requirements", expr );
	}
	if ( rank ) {
		ad.InsertAttr( "Rank", rank );
	}

	return getMatches( ad, leases );
}

bool
DCMatchLite::renewLeases(
	list< const DCMatchLiteLease *> &leases,
	list< DCMatchLiteLease *> &out_leases )
{
		// Create the ReliSock
	ReliSock *rsock = (ReliSock *)startCommand(
			MATCHLITE_RENEW_LEASE, Stream::reli_sock, 20 );
	if ( ! rsock ) {
		return false;
	}

	// Send the leases
	if ( !SendLeases( rsock, leases ) ) {
		delete rsock;
		return false;
	}

	rsock->eom();

		// Receive the return code
	int		rc;
	rsock->decode();
	if ( !rsock->get( rc ) ) {
		delete rsock;
		return false;
	}
	if ( rc != OK ) {
		delete rsock;
		return false;
	}

		// Finally, read the returned leases
	if ( !GetLeases( rsock, out_leases ) ) {
		delete rsock;
		return false;
	}

	rsock->close();
	delete rsock;
	return true;
}

bool
DCMatchLite::releaseLeases(
	list< const DCMatchLiteLease *> &leases )
{
		// Create the ReliSock
	ReliSock *rsock = (ReliSock *)startCommand(
			MATCHLITE_RELEASE_LEASE, Stream::reli_sock, 20 );
	if ( ! rsock ) {
		return false;
	}

	// Send the leases
	if ( !SendLeases( rsock, leases ) ) {
		delete rsock;
		return false;
	}

	rsock->eom();

		// Receive the return code
	int		rc;
	rsock->decode();
	if ( !rsock->get( rc ) ) {
		delete rsock;
		return false;
	}

	rsock->close();
	delete rsock;
	return true;
}

bool
DCMatchLite::SendLeases(
	Stream							*stream,
	list< const DCMatchLiteLease *>	&l_list )
{
	if ( !stream->put( l_list.size() ) ) {
		return false;
	}

	list <const DCMatchLiteLease *>::iterator iter;
	for( iter = l_list.begin(); iter != l_list.end(); iter++ ) {
		const DCMatchLiteLease	*lease = *iter;
		const char	*lease_id_str = lease->LeaseId().c_str();
		if ( !stream->put( (char*) lease_id_str ) ||
			 !stream->put( lease->LeaseDuration() ) ||
			 !stream->put( lease->ReleaseLeaseWhenDone() )  ) {
			return false;
		}
	}
	return true;
}

bool
DCMatchLite::GetLeases(
	Stream							*stream,
	std::list< DCMatchLiteLease *>	&l_list )
{
	int		num_leases;
	if ( !stream->get( num_leases ) ) {
		return false;
	}

	for( int	num = 0;  num < num_leases;  num++ ) {
		char	*lease_id_cstr = NULL;
		int		lease_duration;
		int		release_when_done;
		if ( !stream->get( lease_id_cstr ) ||
			 !stream->get( lease_duration ) ||
			 !stream->get( release_when_done ) ) {
			DCMatchLiteLease_FreeList( l_list );
			if ( lease_id_cstr ) {
				free( lease_id_cstr );
			}
			return false;
		}
		string	lease_id( lease_id_cstr );
		free( lease_id_cstr );
		DCMatchLiteLease	*lease =
			new DCMatchLiteLease( lease_id,
								  lease_duration,
								  (bool) release_when_done );
		l_list.push_back( lease );
	}
	return true;
}
