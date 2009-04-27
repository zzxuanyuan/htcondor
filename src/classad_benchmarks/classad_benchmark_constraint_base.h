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

#include "classad_benchmark_constraint_options.h"
using namespace std;
#include <vector>
#include "stdio.h"

class ClassAdGenericBase
{
  public:
	ClassAdGenericBase( void ) { };
	virtual ~ClassAdGenericBase( void ) { };

	virtual void freeAd( void ) = 0;

  private:
	// nothing
};

class ClassAdConstraintBenchmarkBase
{
  public:
	ClassAdConstraintBenchmarkBase( const ClassAdConstraintBenchmarkOptions & );
	virtual ~ClassAdConstraintBenchmarkBase( void );

	bool readAdFile( void );

	// Finish the setup
	bool setup( void );

	// Do real work
	bool runQueries( void );

	// Pure-Virtual member methods
	virtual ClassAdGenericBase *parseTemplateAd( FILE *fp ) = 0;
	virtual bool generateAd( const ClassAdGenericBase *template_ad ) = 0;
	virtual bool createView( const char *expr ) = 0;
	virtual bool printCollectionInfo( void ) const = 0;
	virtual bool runQuery( const char *expr, int qnum, bool two_way, int &matches ) = 0;
	virtual bool getViewMembers( int & ) const = 0;

	int Verbose( void ) const { return m_options.getVerbosity(); };
	bool isVerbose( int level ) const { return (m_options.getVerbosity() >= level); };

	int numTemplates( void ) const { return m_template_offsets.size(); };
	
  protected:
	vector <fpos_t> 						 m_template_offsets;
	const ClassAdConstraintBenchmarkOptions	&m_options;
	int										 m_num_ads;
};

#endif
