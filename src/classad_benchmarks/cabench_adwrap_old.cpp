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
#include "condor_attributes.h"

#include "cabench_adwrap_old.h"

static int adCount = 0;

CaBenchAdWrapOld::CaBenchAdWrapOld( ClassAd *ad, bool dtor_del_ad )
		: CaBenchAdWrapBase( dtor_del_ad ),
		  m_ad( ad )
{
	adCount++;
}

CaBenchAdWrapOld::~CaBenchAdWrapOld( void )
{
	if ( getDtorDelAd() ) {
		deleteAd( );
	}
}

void
CaBenchAdWrapOld::deleteAd( void )
{
	adCount--;
	if ( m_ad ) {
		delete m_ad;
		m_ad = NULL;
	}
}

int
CaBenchAdWrapOld::getAdCount( void )
{
	return adCount;
}
