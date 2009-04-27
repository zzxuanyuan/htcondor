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

#ifndef CLASSAD_BENCHMARK_CONSTRAINTS_OLD_H
#define CLASSAD_BENCHMARK_CONSTRAINTS_OLD_H

#include "classad_benchmark_constraint_base.h"
#include "classad_collection.h"

class ClassAdGenericOld : public ClassAdGenericBase
{
  public:
	ClassAdGenericOld( ClassAd *ad ) : m_ad(ad) { };
	virtual ~ClassAdGenericOld( void ) { };

	ClassAd *get( void ) const { return m_ad; };
	void freeAd( void ) { delete m_ad; };

  private:
	ClassAd	*m_ad;
};


class ClassAdConstraintBenchmarkOld : public ClassAdConstraintBenchmarkBase
{
  public:
	ClassAdConstraintBenchmarkOld( const ClassAdConstraintBenchmarkOptions & );
	virtual ~ClassAdConstraintBenchmarkOld( void );

	ClassAdGenericBase *parseTemplateAd( FILE *fp );
	bool createView( const char *expr );
	bool generateAd( const ClassAdGenericBase *template_ad );
	bool printCollectionInfo( void ) const;
	bool runQuery( const char *query, int qnum, bool two_way, int &matches );
	bool getViewMembers( int & ) const;

  private:
	ClassAdCollection		 m_collection;
};

#endif
