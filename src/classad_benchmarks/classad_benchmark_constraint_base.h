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

#ifndef CLASSAD_BENCHMARK_CONSTRAINTS_BASE_H
#define CLASSAD_BENCHMARK_CONSTRAINTS_BASE_H

using namespace std;
#include <vector>

class ClassAdConstraintBenchmarkBase
{
  public:
	ClassAdConstraintBenchmarkBase( void );
	virtual ~ClassAdConstraintBenchmarkBase( void );

	bool setUseView( bool use_view );

	bool setVerbosity( int );
	bool incVerbosity( void );

	bool readAdFile( const char *fname );

	// Finish the setup
	bool setup( int num_ads );

	// Do real work
	bool runQueries( int num_queries, const char *query, bool two_way );

	// Pure-Virtual member methods
	virtual bool parseTemplateAd( FILE *fp ) = 0;
	virtual bool generateAd( int template_num ) = 0;
	virtual bool createView( const char *key, const char *value ) = 0;
	virtual bool collectionInfo( void ) = 0;
	virtual bool runQuery( const char *constraint, bool two_way, int &matches ) = 0;
	virtual int numTemplates( void ) const = 0;
	

  protected:
	int			m_verbosity;
	bool		m_view;
};

#endif
