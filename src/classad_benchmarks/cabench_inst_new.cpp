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

#include "cabench_adwrap_new.h"
#include "cabench_query_new_list.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;
#include <list>

#include "debug_timer_dprintf.h"


// =======================================
// CaBenchQueryNew methods
// =======================================
CaBenchQueryNew::CaBenchQueryNew(
	const CaBenchQueryOptions &options) 
		: CaBenchQueryBase( options ),
		  m_filter_ad( NULL )
{
}

CaBenchQueryNew::~CaBenchQueryNew( void )
{
	if ( m_filter_ad ) {
		m_filter_match.RemoveLeftAd( );

		delete m_filter_ad;
		m_filter_ad = NULL;
	}
}

CaBenchAdWrapBase *
CaBenchQueryNew::parseTemplateAd( FILE *stream )
{
	classad::ClassAd	*ad = m_parser.ParseClassAd( stream );
	if ( NULL == ad ) {
		if ( numTemplates() == 0 ) {
			fprintf( stderr, "Error parsing template ad\n" );
		}
		return( NULL );
	}
	return new CaBenchAdWrapNew(ad );
}

bool
CaBenchQueryNew::initFilter( void )
{
	if ( NULL == m_filter_str ) {
		return true;
	}

	m_filter_ad = m_parser.ParseClassAd( m_filter_str, true );
	if ( NULL == m_filter_ad ) {
		m_filter_ad = new classad::ClassAd;
		classad::ExprTree *req_expr =
			m_parser.ParseExpression( m_filter_str, true );
		if ( NULL == req_expr ) {
			fprintf( stderr,
					 "'%s' is neither a valid query expression nor ad\n",
					 m_filter_str );
			return false;
		}
		m_filter_ad->Insert( "Requirements", req_expr );
	}

	if ( !m_filter_match.ReplaceLeftAd( m_filter_ad ) ) {
		fprintf( stderr, "Match:ReplaceLeftAd() failed\n" );
		return false;
	}

	return true;
}

bool
CaBenchQueryNew::filterAd( const CaBenchAdWrapBase *base_ad ) const
{
	if ( NULL == m_filter_ad ) {
		return true;
	}
	classad::ClassAd	*ad = CaBenchAdWrapNew::getAd( base_ad );
	if ( !m_filter_match.ReplaceRightAd( ad ) ) {
		fprintf( stderr, "filter:ReplaceRightAd() failed\n" );
		return false;
	}

	bool	match = false;
	if( !m_filter_match.EvaluateAttrBool( "RightMatchesLeft", match ) ) {
		// Do nothing
	}

	m_filter_match.RemoveRightAd( );

	return match;
}

bool
CaBenchQueryNew::generateInsertAd( const CaBenchAdWrapBase *base_ad,
								   bool &copied )
{
	classad::ClassAd	*ad = CaBenchAdWrapNew::getAd( base_ad );
	string				 name;
	string				 type;

	if ( !ad->EvaluateAttrString( "Name", name )   ||
		 !ad->EvaluateAttrString( "MyType", type )  ) {
		fprintf( stderr, "name or type missing" );
		return false;
	}
	char	key[256];
	snprintf( key, sizeof(key), "%s/%s/%p", type.c_str(), name.c_str(), ad );
	key[sizeof(key)-1] = '\0';
	ad->InsertAttr( "key", key );

	return insertAd( key, ad, copied );
}

int
CaBenchQueryNew::getAdCount( void ) const
{
	return CaBenchAdWrapNew::getAdCount( );
}
