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
		: CaBenchQueryBase( options )
{
}

CaBenchQueryNew::~CaBenchQueryNew( void )
{
}

CaBenchAdWrapBase *
CaBenchQueryNew::parseTemplateAd( FILE *stream )
{
	static classad::ClassAdParser	 parser;
	classad::ClassAd				*ad = parser.ParseClassAd( stream );
	if ( NULL == ad ) {
		if ( numTemplates() == 0 ) {
			fprintf( stderr, "Error parsing template ad\n" );
		}
		return( NULL );
	}
	return new CaBenchAdWrapNew(ad );
}

bool
CaBenchQueryNew::generateInsertAd( const CaBenchAdWrapBase *base_ad,
								   bool &copied )
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

	return insertAd( key, ad, copied );
}

int
CaBenchQueryNew::getAdCount( void ) const
{
	return CaBenchAdWrapNew::getAdCount( );
}
