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

#ifndef CABENCH_ADWRAP_NEW_H
#define CABENCH_ADWRAP_NEW_H

#include "cabench_adwrap_base.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

class CaBenchAdWrapNew : public CaBenchAdWrapBase
{
  public:
	CaBenchAdWrapNew( classad::ClassAd *ad );
	virtual ~CaBenchAdWrapNew( void );

	classad::ClassAd *get( void ) const { return m_ad; };
	void deleteAd( void );
	void releaseOwnership( void );
	static int getAdCount( void );

  private:
	classad::ClassAd	*m_ad;
};

#endif
