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

#ifndef CABENCH_QUERY_OLD_COLLECTION_H
#define CABENCH_QUERY_OLD_COLLECTION_H

#include "classad_collection.h"

#include "cabench_adwrap_base.h"
#include "cabench_query_old.h"

class CaBenchQueryOldCollection : public CaBenchQueryOld
{
  public:
	CaBenchQueryOldCollection( const CaBenchQueryOptions & );
	virtual ~CaBenchQueryOldCollection( void );

	bool createView( const char *expr );
	bool insertAd( const char *key, ClassAd *ad, bool &copied );
	bool printCollectionInfo( void ) const;
	bool runQuery( const char *query, int qnum, bool two_way, int &matches );
	bool getViewMembers( int & ) const;

	bool releaseMemory( void );

  private:
	ClassAdCollection		 *m_collection;
};

#endif
