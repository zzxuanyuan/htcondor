/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "MyString.h"

#include "cabench_adwrap_old.h"
#include "cabench_query_old.h"

#include "debug_timer_dprintf.h"


CaBenchQueryOld::CaBenchQueryOld(
	const CaBenchQueryOptions &options) 
		: CaBenchQueryBase( options ),
		  m_filter_ad( NULL )
{
}

CaBenchQueryOld::~CaBenchQueryOld( void )
{
	if ( m_filter_ad ) {
		delete m_filter_ad;
		m_filter_ad = NULL;
	}
}

CaBenchAdWrapBase *
CaBenchQueryOld::parseTemplateAd( FILE *stream )
{
	int			isEOF = 0, error = 0, empty = 0;
	ClassAd		*ad = new ClassAd( stream, ";", isEOF, error, empty );
	if ( isEOF ) {
		if ( ad ) {
			delete ad;
		}
		return false;
	}
	if ( error || empty ) {
		fprintf( stderr, "Error parsing template ad\n" );
		if ( ad ) {
			delete ad;
		}
		return false;
	}
	if ( NULL == ad ) {
		fprintf( stderr, "NULL ad\n" );
		return( false );
	}
	return new CaBenchAdWrapOld( ad );
}

bool
CaBenchQueryOld::initFilter( void )
{
	if ( NULL == m_filter_str ) {
		return true;
	}

	// Is the query string a ClassAd ? (surrounded by [] like new classads)
	int len = strlen(m_filter_str );
	if ( m_filter_str[0] == '[' && m_filter_str[len-1] == ']' ) {
		char		*ad_str = strdup( m_filter_str+1 );
		ad_str[len-2] = '\0'; len -= 2;
		while( len && isspace(ad_str[len-1]) ) {
			ad_str[--len] = '\0';
		}
		m_filter_ad = new ClassAd( ad_str, ';' );
	}
	else {
		MyString	req_expr = "Requirements = ";
		req_expr += m_filter_str;
		m_filter_ad = new ClassAd;
		m_filter_ad->Insert( req_expr.Value() );
	}

	return true;
}

bool
CaBenchQueryOld::filterAd( const CaBenchAdWrapBase *base_ad ) const
{
	if ( NULL == m_filter_ad ) {
		return true;
	}
	return ( (*m_filter_ad) <= (*CaBenchAdWrapOld::getAd(base_ad)) );
}

bool
CaBenchQueryOld::generateInsertAd( const CaBenchAdWrapBase *base_ad,
								   bool &copied)
{
	ClassAd		*ad = CaBenchAdWrapOld::getAd(base_ad);
	MyString	 name;
	MyString	 type;
	static int	 n = 0;

	if ( Options().getQueryEnabled() ) {
		if ( !ad->LookupString( "Name", name )   ||
			 !ad->LookupString( "MyType", type )  ) {
			fprintf( stderr, "name or type missing\n" );
			return false;
		}
	}
	char	key[256];
	snprintf(key, sizeof(key), "%s/%s/%06d", type.Value(), name.Value(), n++);
	key[sizeof(key)-1] = '\0';
	ad->Assign( "key", key );

	return insertAd( key, ad, copied );
}

int
CaBenchQueryOld::getAdCount( void ) const
{
	return CaBenchAdWrapOld::getAdCount( );
}
