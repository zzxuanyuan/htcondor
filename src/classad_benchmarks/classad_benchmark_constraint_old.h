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
#include <vector>

class ClassAdConstraintBenchmarkOld : public ClassAdConstraintBenchmarkBase
{
  public:
	ClassAdConstraintBenchmarkOld( const ClassAdConstraintBenchmarkOptions & );
	virtual ~ClassAdConstraintBenchmarkOld( void );

	bool parseTemplateAd( FILE *fp );
	bool createView( const char *expr );
	bool generateAd( int template_num );
	bool printCollectionInfo( void ) const;
	bool runQuery( const char *query, bool two_way, int &matches );
	int numTemplates( void ) const;
	bool getViewMembers( int & ) const;

  private:
	ClassAdCollection		 m_collection;
	vector<const ClassAd *>	 m_template_ads;
};

#endif
