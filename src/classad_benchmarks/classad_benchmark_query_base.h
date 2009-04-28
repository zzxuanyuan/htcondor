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

#ifndef CLASSAD_BENCHMARK_QUERY_BASE_H
#define CLASSAD_BENCHMARK_QUERY_BASE_H

#include "../condor_procapi/procapi.h"
#include "classad_benchmark_query_options.h"
using namespace std;
#include <vector>
#include "stdio.h"

class ClassAdGenericBase
{
  public:
	ClassAdGenericBase( bool dtor_del_ad );
	virtual ~ClassAdGenericBase( void );

	void setDtorDelAd( bool v ) { m_dtor_del_ad = v; };
	bool getDtorDelAd( void ) const { return m_dtor_del_ad; };
	virtual void deleteAd( void ) = 0;

  private:
	bool		 m_dtor_del_ad;
};

class ClassAdQueryBenchmarkBase
{
  public:
	ClassAdQueryBenchmarkBase( const ClassAdQueryBenchmarkOptions & );
	virtual ~ClassAdQueryBenchmarkBase( void );

	bool readAdFile( void );

	// Finish the setup
	bool setup( void );

	// Do real work
	bool runQueries( void );

	// Clean up
	bool cleanup( void );
	void memoryDump( const char *label, const piPTR, bool start ) const;
	void memoryDump( const char *label, const piPTR ref,
					 const piPTR values ) const;

	// Pure-Virtual member methods
	virtual ClassAdGenericBase *parseTemplateAd( FILE *fp, bool dtor_del_ad)=0;
	virtual bool generateAd( const ClassAdGenericBase *template_ad ) = 0;
	virtual bool createView( const char *expr ) = 0;
	virtual bool printCollectionInfo( void ) const = 0;
	virtual bool runQuery( const char *expr, int qnum,
						   bool two_way, int &matches ) = 0;
	virtual bool getViewMembers( int & ) const = 0;
	virtual bool collectionCopiesAd( void ) = 0;
	virtual void releaseMemory( void ) = 0;
	virtual int getAdCount( void ) const = 0;

	int Verbose( void ) const {
		return m_options.getVerbosity();
	};
	bool isVerbose( int level ) const {
		return (m_options.getVerbosity() >= level);
	};

	int numTemplates( void ) const { return m_template_offsets.size(); };
	
  protected:
	vector <fpos_t> 					 m_template_offsets;
	const ClassAdQueryBenchmarkOptions	&m_options;
	int									 m_num_ads;

	piPTR								 m_procinfo_init;
	piPTR								 m_procinfo_initdone;
	piPTR								 m_procinfo_query;
	piPTR								 m_procinfo_querydone;

};

#endif
