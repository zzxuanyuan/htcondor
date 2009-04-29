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
#include "cabench_query_old_list.h"
#include "classad_collection.h"

#include "debug_timer_dprintf.h"


CaBenchQueryOldList::CaBenchQueryOldList(
	const CaBenchQueryOptions &options) 
		: CaBenchQueryOld( options )
{
}

CaBenchQueryOldList::~CaBenchQueryOldList( void )
{
	releaseMemory( );
}

bool
CaBenchQueryOldList::releaseMemory( void )
{
	list <ClassAd *>::iterator iter;
	for ( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		ClassAd *ad = *iter;
		delete ad;
	}

	m_list.clear();
	return true;
}

bool
CaBenchQueryOldList::createView( const char * /*expr*/ )
{
	return false;
}

bool
CaBenchQueryOldList::printCollectionInfo( void ) const
{
	return true;
}

bool
CaBenchQueryOldList::getViewMembers( int &members ) const
{
	members = m_num_ads;
	return true;
}

bool
CaBenchQueryOldList::insertAd( const char * /*key*/, ClassAd *ad, bool &copied)
{
	m_list.push_back( ad );
	copied = false;
	return true;
}

bool
CaBenchQueryOldList::runQuery( const char *query_str,
									 int query_num,
									 bool two_way,
									 int &matches )
{
	ClassAd		*query_ad;

	// Is the query string a ClassAd ? (surrounded by [] like new classads)
	int len = strlen(query_str);
	if ( query_str[0] == '[' && query_str[len-1] == ']' ) {
		char		*ad_str = strdup( query_str+1 );
		ad_str[len-2] = '\0'; len -= 2;
		while( len && isspace(ad_str[len-1]) ) {
			ad_str[--len] = '\0';
		}
		query_ad = new ClassAd( ad_str, ';' );
	}
	else {
		MyString	req_expr = "Requirements = ";
		req_expr += query_str;
		query_ad = new ClassAd;
		query_ad->Insert( req_expr.Value() );
	}

	if ( (isVerbose(1) && (0==query_num)) || isVerbose(2) ) {
		printf( "SearchAd=\n" );
		query_ad->fPrint( stdout );
	}

	matches = 0;
	list <ClassAd *>::iterator iter;
	for ( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		ClassAd *ad = *iter;

		bool	result;
		if ( two_way ) {
			result = ( (*ad) == (*query_ad) );
		}
		else {
			result = ( (*ad) >= (*query_ad) );
		}
		if ( result ) {
			matches++;
		}
	}

	delete query_ad;
	return true;
}
