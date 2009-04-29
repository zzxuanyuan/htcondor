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

#ifndef CABENCH_QUERY_BASE_H
#define CABENCH_QUERY_BASE_H

#include "../condor_procapi/procapi.h"

#include "cabench_adwrap_base.h"
#include "cabench_query_options.h"
using namespace std;
#include <vector>
#include "stdio.h"


class CaBenchQueryBase
{
  public:
	CaBenchQueryBase( const CaBenchQueryOptions & );
	virtual ~CaBenchQueryBase( void );

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
	virtual CaBenchAdWrapBase *parseTemplateAd( FILE *fp ) = 0;
	virtual bool generateInsertAd( const CaBenchAdWrapBase *template_ad,
								   bool &copied ) = 0;

	virtual bool createView( const char *expr ) = 0;
	virtual bool printCollectionInfo( void ) const = 0;
	virtual bool runQuery( const char *expr, int qnum,
						   bool two_way, int &matches ) = 0;
	virtual bool getViewMembers( int & ) const = 0;

	bool releaseMemory( void );
	virtual int getAdCount( void ) const = 0;

	int Verbose( void ) const {
		return m_options.getVerbosity();
	};
	bool isVerbose( int level ) const {
		return (m_options.getVerbosity() >= level);
	};

	int numTemplates( void ) const { return m_template_offsets.size(); };
	
  protected:
	vector <fpos_t> 			 m_template_offsets;
	const CaBenchQueryOptions	&m_options;
	int							 m_num_ads;

	struct procInfo				 m_procinfo_init;
	struct procInfo				 m_procinfo_initdone;
	struct procInfo				 m_procinfo_query;
	struct procInfo				 m_procinfo_querydone;

};

#endif
