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

#include "classad_benchmark_query_old.h"
#include "classad_collection.h"

#include "debug_timer_dprintf.h"


// =======================================
// ClassAdGenericOld methods
// =======================================
static int adCount = 0;
ClassAdGenericOld::ClassAdGenericOld( ClassAd *ad, bool dtor_del_ad )
		: ClassAdGenericBase( dtor_del_ad ),
		  m_ad( ad )
{
	adCount++;
};
ClassAdGenericOld::~ClassAdGenericOld( void )
{
	if ( getDtorDelAd() ) {
		deleteAd( );
	}
};
void
ClassAdGenericOld::deleteAd( void )
{
	adCount--;
	if ( m_ad ) {
		delete m_ad;
		m_ad = NULL;
	}
};


// =======================================
// ClassAdQueryBenchmarkOld methods
// =======================================
ClassAdQueryBenchmarkOld::ClassAdQueryBenchmarkOld(
	const ClassAdQueryBenchmarkOptions &options) 
		: ClassAdQueryBenchmarkBase( options )
{
	m_collection = new ClassAdCollection;
}

ClassAdQueryBenchmarkOld::~ClassAdQueryBenchmarkOld( void )
{
	releaseMemory( );
}

void
ClassAdQueryBenchmarkOld::releaseMemory( void )
{
	if ( m_collection ) {
		delete m_collection;
		m_collection = NULL;
	}
}

ClassAdGenericBase *
ClassAdQueryBenchmarkOld::parseTemplateAd( FILE *stream,
												bool dtor_del_ad )
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
	return new ClassAdGenericOld( ad, dtor_del_ad );
}

bool
ClassAdQueryBenchmarkOld::createView( const char * /*expr*/ )
{
	return false;
}

bool
ClassAdQueryBenchmarkOld::printCollectionInfo( void ) const
{
	return true;
}

bool
ClassAdQueryBenchmarkOld::getViewMembers( int &members ) const
{
	members = m_num_ads;
	return true;
}

bool
ClassAdQueryBenchmarkOld::generateAd( const ClassAdGenericBase *base_ad )
{
	const ClassAdGenericOld	*gad =
		dynamic_cast<const ClassAdGenericOld*>(base_ad);
	ClassAd				*ad = gad->get();
	MyString			 name;
	MyString			 type;
	static int			 n = 0;

	if ( !ad->LookupString( "Name", name )   ||
		 !ad->LookupString( "MyType", type )  ) {
		fprintf( stderr, "name or type missing" );
		return false;
	}
	char	key[256];
	snprintf(key, sizeof(key), "%s/%s/%06d", type.Value(), name.Value(), n++);
	key[sizeof(key)-1] = '\0';
	ad->Assign( "key", key );
	m_collection->NewClassAd( key, ad );
	return true;
}

bool
ClassAdQueryBenchmarkOld::runQuery( const char *query_str,
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
	int iters = 0;
	m_collection->StartIterateAllClassAds();
	do {
		ClassAd		*ad;
		if (!m_collection->IterateAllClassAds( ad ) ) {
			break;
		}
		iters++;
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
	} while( true );

	delete query_ad;
	return true;
}

int
ClassAdQueryBenchmarkOld::getAdCount( void ) const
{
	return adCount;
}
