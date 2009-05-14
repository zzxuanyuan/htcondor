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
#include "cabench_inst_old.h"

#include "debug_timer_dprintf.h"


CaBenchInstOld::CaBenchInstOld(
	const CaBenchInstOptions &options) 
		: CaBenchInstBase( options )
{
}

CaBenchInstOld::~CaBenchInstOld( void )
{
}

bool
CaBenchInstOld::initAds( int num_ads )
{
	for( int adno = 0;  adno < num_ads;  adno++ ) {
		ClassAd *ad = new ClassAd;
		m_ads.push_back( ad );
	}
	return true;
}

bool
CaBenchInstOld::addAttr( int adno, const char *attr, bool v )
{
	ClassAd	*ad = m_ads[adno];
	ad->Assign( attr, v );
	return true;
}

bool
CaBenchInstOld::addAttr( int adno, const char *attr, int v )
{
	ClassAd	*ad = m_ads[adno];
	ad->Assign( attr, v );
	return true;
}

bool
CaBenchInstOld::addAttr( int adno, const char *attr, double v )
{
	ClassAd	*ad = m_ads[adno];
	ad->Assign( attr, v );
	return true;
}

bool
CaBenchInstOld::addAttr( int adno, const char *attr, const char *v )
{
	ClassAd	*ad = m_ads[adno];
	ad->Assign( attr, v );
	return true;
}

bool
CaBenchInstOld::deleteAds( void )
{
	vector <ClassAd *>::iterator iter;
	for ( iter = m_ads.begin(); iter != m_ads.end(); iter++ ) {
		ClassAd *ad = *iter;
		delete ad;
	}
	m_ads.clear( );
	return true;
}
