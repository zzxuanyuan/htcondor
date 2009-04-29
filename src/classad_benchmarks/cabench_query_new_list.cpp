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
CaBenchQueryNewList::CaBenchQueryNewList(
	const CaBenchQueryOptions &options) 
		: CaBenchQueryNew( options )
{
}

CaBenchQueryNewList::~CaBenchQueryNewList( void )
{
	releaseMemory( );
}

bool
CaBenchQueryNewList::releaseMemory( void )
{
	list <classad::ClassAd *>::iterator iter;
	for ( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		classad::ClassAd *ad = *iter;
		delete ad;
	}

	m_list.clear();
	return true;
}

bool
CaBenchQueryNewList::createView( const char * /*constraint_expr*/ )
{
	return false;
}

bool
CaBenchQueryNewList::printCollectionInfo( void ) const
{
	return true;
}

bool
CaBenchQueryNewList::getViewMembers( int &members ) const
{
	members = m_list.size();
	return true;
}

bool
CaBenchQueryNewList::insertAd( const char * /*key*/,
							   classad::ClassAd *ad,
							   bool &copied)
{
	m_list.push_back( ad );
	copied = false;
	return true;
}

bool
CaBenchQueryNewList::runQuery( const char *query_str,
							   int query_num,
							   bool two_way,
							   int &matches )
{
#if 0
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

	matches = 0;
	list <classad::ClassAd *>::iterator iter;
	for ( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		classad::ClassAd *ad = *iter;
	}

	classad::LocalListQuery	query;
	query.Bind( m_collection );
	if ( !query.Query( m_view_name, query_ad, two_way ) ) {
		fprintf( stderr, "Query failed\n" );
		delete query_ad;
		return false;
	}

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
#endif
	return true;
}
