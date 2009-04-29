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

#ifndef CABENCH_QUERY_OLD_LIST_H
#define CABENCH_QUERY_OLD_LIST_H

#include <list>

#include "condor_classad.h"

#include "cabench_adwrap_base.h"
#include "cabench_query_base.h"
#include "cabench_query_old.h"

class CaBenchQueryOldList : public CaBenchQueryOld
{
  public:
	CaBenchQueryOldList( const CaBenchQueryOptions & );
	virtual ~CaBenchQueryOldList( void );

	bool insertAd( const char *key, ClassAd *ad, bool &copied );
	bool createView( const char *expr );
	bool printCollectionInfo( void ) const;
	bool runQuery( const char *query, int qnum, bool two_way, int &matches );
	bool getViewMembers( int & ) const;

	bool releaseMemory( void );

  private:
	list<ClassAd *>		m_list;
};

#endif
