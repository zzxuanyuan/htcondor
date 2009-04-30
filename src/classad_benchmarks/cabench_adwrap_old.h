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

#ifndef CABENCH_ADWRAP_OLD_H
#define CABENCH_ADWRAP_OLD_H

#include "cabench_adwrap_base.h"
#include "condor_classad.h"

class CaBenchAdWrapOld : public CaBenchAdWrapBase
{
  public:
	CaBenchAdWrapOld( ClassAd *ad );
	virtual ~CaBenchAdWrapOld( void );

	ClassAd *getAd( void ) const { return m_ad; };
	void deleteAd( void );
	void releaseOwnership( void );
	static int getAdCount( void );

	static CaBenchAdWrapOld * get( CaBenchAdWrapBase *base_ad ) {
		return dynamic_cast<CaBenchAdWrapOld*>( base_ad );
	}
	static const CaBenchAdWrapOld *get(const CaBenchAdWrapBase *base_ad){
		return dynamic_cast<const CaBenchAdWrapOld*>( base_ad );
	}
	static ClassAd * getAd( CaBenchAdWrapBase *base_ad ) {
		return dynamic_cast<CaBenchAdWrapOld*>( base_ad )->getAd( );
	}
	static ClassAd *getAd(const CaBenchAdWrapBase *base_ad){
		return dynamic_cast<const CaBenchAdWrapOld*>( base_ad )->getAd();
	}

  private:
	mutable ClassAd		*m_ad;
};

#endif
