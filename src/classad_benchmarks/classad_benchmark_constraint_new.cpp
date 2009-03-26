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

#include "classad_benchmark_constraint_new.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

#include "debug_timer_dprintf.h"

ClassAdConstraintBenchmarkNew::ClassAdConstraintBenchmarkNew(
	const ClassAdConstraintBenchmarkOptions &options) 
		: ClassAdConstraintBenchmarkBase( options )
{
	m_view_name = "root";
}

ClassAdConstraintBenchmarkNew::~ClassAdConstraintBenchmarkNew( void )
{
}

bool
ClassAdConstraintBenchmarkNew::parseTemplateAd( FILE *stream )
{
	classad::ClassAdParser	parser;
	classad::ClassAd		*ad = parser.ParseClassAd( stream );
	if ( NULL == ad ) {
		if ( numTemplates() == 0 ) {
			fprintf( stderr, "Error parsing template ad\n" );
		}
		return( false );
	}
	m_template_ads.push_back( ad );
	return true;
}

int
ClassAdConstraintBenchmarkNew::numTemplates( void ) const
{
	return m_template_ads.size();
}

bool
ClassAdConstraintBenchmarkNew::createView( const char *constraint_expr )
{
	if ( !constraint_expr ) {
		return false;
	}

	printf( "setting up view with expr '%s'\n", constraint_expr );
	string constraint = constraint_expr;
	string rank;
	string expr;
	m_view_name = "VIEW";
	if ( !m_collection.CreateSubView( m_view_name, "root",
									  constraint, rank, expr) ) {
		fprintf( stderr, "Error creating resources view\n" );
		return false;
	}

	return true;
}

bool
ClassAdConstraintBenchmarkNew::printCollectionInfo( void ) const
{
	if ( isVerbose(1) ) {
		classad::ClassAd	*ad;
		if ( !m_collection.GetViewInfo( m_view_name, ad ) ) {
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
ClassAdConstraintBenchmarkNew::getViewMembers( int &members ) const
{
	classad::ClassAd	*ad;
	if ( !m_collection.GetViewInfo( m_view_name, ad ) ) {
		fprintf( stderr, "Error getting view information\n" );
		return false;
	}

	if ( !ad->EvaluateAttrInt( "NumMembers", members ) ) {
		fprintf( stderr, "Error getting view members\n" );
		return false;
	}
	return true;
}

bool
ClassAdConstraintBenchmarkNew::generateAd( int template_num )
{
	const classad::ClassAd	*template_ad = m_template_ads[template_num];
	classad::ClassAd		*ad = new classad::ClassAd( *template_ad );
	string					 name;
	string					 type;

	if ( !ad->EvaluateAttrString( "Name", name )   ||
		 !ad->EvaluateAttrString( "MyType", type )  ) {
		fprintf( stderr, "name or type missing" );
		return false;
	}
	char	key[256];
	snprintf( key, sizeof(key), "%s/%s/%p", type.c_str(), name.c_str(), ad );
	key[sizeof(key)-1] = '\0';
	ad->InsertAttr( "key", key );
	m_collection.AddClassAd( key, ad );
	return true;
}

bool
ClassAdConstraintBenchmarkNew::runQuery( const char *query_str,
										 bool two_way,
										 int &matches )
{
	// Is the query string a ClassAd ?
	classad::ClassAdParser	 parser;
	classad::ClassAd		*query_ad = parser.ParseClassAd( query_str, true );
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

	if ( isVerbose(1) ) {
		classad::PrettyPrint u;
		std::string adbuffer;
		u.Unparse( adbuffer, query_ad );
		printf( "SearchAd=%s\n", adbuffer.c_str() );
	}

	classad::LocalCollectionQuery	query;
	query.Bind( &m_collection );
	if ( !query.Query( m_view_name, query_ad, two_way ) ) {
		fprintf( stderr, "Query failed\n" );
		return false;
	}

	matches = 0;
	string	key;
	do {
		if ( !query.Current( key ) ) {
			break;
		}
		classad::ClassAd	*ad = m_collection.GetClassAd( key );
		matches++;
	} while ( query.Next( key ) );

	return true;
}
