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

#ifndef CABENCH_QUERY_NEW_LIST_H
#define CABENCH_QUERY_NEW_LIST_H

#include <list>
#include "cabench_query_new.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
using namespace std;

class CaBenchQueryNewList : public CaBenchQueryNew
{
  public:
	CaBenchQueryNewList( const CaBenchQueryOptions & );
	virtual ~CaBenchQueryNewList( void );

	bool createView( const char *expr );
	bool insertAd( const char *key, classad::ClassAd *ad, bool &copied );
	bool printCollectionInfo( void ) const;
	bool runQuery( const char *query, int qnum, bool two_way, int &matches );
	bool getViewMembers( int & ) const;

	bool releaseMemory( void );

  private:
	list<classad::ClassAd *>	m_list;
};

#endif
