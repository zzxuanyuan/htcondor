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

#include <list>

#include "condor_classad.h"

#include "cabench_adwrap_base.h"
#include "cabench_query_base.h"

class CaBenchQueryOld : public CaBenchQueryBase
{
  public:
	CaBenchQueryOld( const CaBenchQueryOptions & );
	virtual ~CaBenchQueryOld( void );

	CaBenchAdWrapBase *parseTemplateAd( FILE *fp );
	bool generateInsertAd( const CaBenchAdWrapBase *template_ad,
						   bool &copied );

	virtual bool createView( const char *expr ) = 0;
	virtual bool insertAd( const char *key, ClassAd *ad, bool &copied ) = 0;
	virtual bool printCollectionInfo( void ) const = 0;
	virtual bool runQuery( const char *query, int qnum,
						   bool two_way, int &matches ) = 0;
	virtual bool getViewMembers( int & ) const = 0;

	virtual bool releaseMemory( void ) = 0;
	int getAdCount( void ) const;

  private:

};

#endif
