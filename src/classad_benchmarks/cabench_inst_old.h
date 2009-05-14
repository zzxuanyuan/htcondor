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

#ifndef CABENCH_QUERY_OLD_H
#define CABENCH_QUERY_OLD_H

#include <vector>

#include "condor_classad.h"

#include "cabench_inst_base.h"

class CaBenchInstOld : public CaBenchInstBase
{
  public:
	CaBenchInstOld( const CaBenchInstOptions & );
	virtual ~CaBenchInstOld( void );

	static const char *Name( void ) { return "New"; };

	bool initAds( int num_ads );
	bool addAttr( int adno, const char *attr, bool v );
	bool addAttr( int adno, const char *attr, int v );
	bool addAttr( int adno, const char *attr, double v );
	bool addAttr( int adno, const char *attr, const char *v );
	bool deleteAds( void );

  private:
	vector<ClassAd *>	m_ads;
};

#endif
