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
#include "cabench_query_new_collection.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

#include "debug_timer_dprintf.h"


// =======================================
// CaBenchQueryNew methods
// =======================================
CaBenchQueryNewCollection::CaBenchQueryNewCollection(
	const CaBenchQueryOptions &options) 
		: CaBenchQueryBase( options )
{
	m_collection = new classad::ClassAdCollection;
	m_view_name = "root";
}

CaBenchQueryNewCollection::~CaBenchQueryNewCollection( void )
{
	releaseMemory( );
}

void
CaBenchQueryNewCollection::releaseMemory( void )
{
	if ( m_collection ) {
		delete m_collection;
		m_collection = NULL;
	}
}

CaBenchAdWrapBase *
CaBenchQueryNewCollection::parseTemplateAd( FILE *stream, bool dtor_del_ad )
{
	static classad::ClassAdParser	 parser;
	classad::ClassAd				*ad = parser.ParseClassAd( stream );
	if ( NULL == ad ) {
		if ( numTemplates() == 0 ) {
			fprintf( stderr, "Error parsing template ad\n" );
		}
		return( NULL );
	}
	return new CaBenchAdWrapNew(ad, dtor_del_ad );
}

bool
CaBenchQueryNewCollection::createView( const char *constraint_expr )
{
	if ( !constraint_expr ) {
		return false;
	}

	printf( "setting up view with expr '%s'\n", constraint_expr );
	string constraint = constraint_expr;
	string rank;
	string expr;
	m_view_name = "VIEW";
	if ( !m_collection->CreateSubView( m_view_name, "root",
									   constraint, rank, expr) ) {
		fprintf( stderr, "Error creating resources view\n" );
		return false;
	}

	return true;
}

bool
CaBenchQueryNewCollection::printCollectionInfo( void ) const
{
	if ( isVerbose(1) ) {
		classad::ClassAd	*ad;
		if ( !m_collection->GetViewInfo( m_view_name, ad ) ) {
			fprintf( stderr, "Error getting view information\n" );
			return false;
		}

		classad::PrettyPrint u;
		std::string adbuffer;
		u.Unparse( adbuffer, ad );
		printf( "ViewInfo:%s\n", adbuffer.c_str() );
	}
	return true;
}

bool
CaBenchQueryNewCollection::getViewMembers( int &members ) const
{
	classad::ClassAd	*ad;
	if ( !m_collection->GetViewInfo( m_view_name, ad ) ) {
		fprintf( stderr, "Error getting view information\n" );
		return false;
	}

	if ( !ad->EvaluateAttrInt( "NumMembers", members ) ) {
		fprintf( stderr, "Error getting view members\n" );
		return false;
	}
	delete ad;
	return true;
}

bool
CaBenchQueryNewCollection::generateAd( const CaBenchAdWrapBase *base_ad )
{
	const CaBenchAdWrapNew	*gad = 
		dynamic_cast<const CaBenchAdWrapNew*>(base_ad);
	classad::ClassAd	*ad = gad->get();
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
	m_collection->AddClassAd( key, ad );
	return true;
}

bool
CaBenchQueryNewCollection::runQuery( const char *query_str,
									 int query_num,
									 bool two_way,
									 int &matches )
{
	// Is the query string a ClassAd ?
	static classad::ClassAdParser	 parser;
	classad::ClassAd	*query_ad = parser.ParseClassAd( query_str, true );
	if ( NULL == query_ad ) {
		query_ad = new classad::ClassAd;
		classad::ExprTree		*req_expr =
			parser.ParseExpression( query_str, true );
		if ( NULL == req_expr ) {
			fprintf( stderr,
					 "'%s' is neither a valid query expression nor ad\n",
					 query_str );
			return false;
		}
		query_ad->Insert( "Requirements", req_expr );
	}

	if ( (isVerbose(1) && (0==query_num)) || isVerbose(2) ) {
		classad::PrettyPrint u;
		std::string adbuffer;
		u.Unparse( adbuffer, query_ad );
		printf( "SearchAd=%s\n", adbuffer.c_str() );
	}

	classad::LocalCollectionQuery	query;
	query.Bind( m_collection );
	if ( !query.Query( m_view_name, query_ad, two_way ) ) {
		fprintf( stderr, "Query failed\n" );
		delete query_ad;
		return false;
	}

	matches = 0;
	string	key;
	do {
		if ( !query.Current( key ) ) {
			break;
		}
		classad::ClassAd	*ad = m_collection->GetClassAd( key );
		(void) ad;
		matches++;
	} while ( query.Next( key ) );

	delete query_ad;
	return true;
}

int
CaBenchQueryNewCollection::getAdCount( void ) const
{
	return CaBenchAdWrapNew::getAdCount( );
}
