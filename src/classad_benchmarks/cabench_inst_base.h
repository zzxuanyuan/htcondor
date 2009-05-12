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

#ifndef CABENCH_INSTANTIATE_BASE_H
#define CABENCH_INSTANTIATE_BASE_H

#include "cabench_base.h"
#include "cabench_adwrap_base.h"
#include "cabench_instantiate_options.h"
using namespace std;
#include <vector>
#include "stdio.h"


class CaBenchInstantiateBase : public CaBenchBase
{
  public:
	CaBenchInstantiateBase( const CaBenchInstantiateOptions & );
	virtual ~CaBenchInstantiateBase( void );

	// Finish the setup
	bool setup( void );

	// Do real work
	bool runQueries( void );

	// Done; dump final info
	bool finish( void );

	// Pure-Virtual member methods
	virtual CaBenchAdWrapBase *parseTemplateAd( FILE *fp ) = 0;
	virtual bool generateInsertAd( const CaBenchAdWrapBase *template_ad,
								   bool &copied ) = 0;
	virtual bool initFilter( void ) = 0;
	virtual bool filterAd( const CaBenchAdWrapBase *base_ad ) const = 0;
	virtual bool createView( const char *expr ) = 0;
	virtual bool printCollectionInfo( void ) const = 0;
	virtual bool runQuery( const char *expr, int qnum,
						   bool two_way, int &matches ) = 0;
	virtual bool getViewMembers( int & ) const = 0;

	bool releaseMemory( void );
	virtual int getAdCount( void ) const = 0;

	int numTemplates( void ) const { return m_template_offsets.size(); };

	const CaBenchQueryOptions & Options( void ) const {
		return dynamic_cast<const CaBenchQueryOptions &>(m_options);
	};
	
  protected:

  private:
};

#endif
