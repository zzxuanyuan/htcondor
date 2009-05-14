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

#ifndef CABENCH_BASE_H
#define CABENCH_BASE_H

#include "../condor_procapi/procapi.h"

#include "cabench_options.h"
#include "cabench_sample.h"
using namespace std;

#include <list>
#include "stdio.h"

class CaBenchBase
{
  public:
	CaBenchBase( const CaBenchOptions & );
	virtual ~CaBenchBase( void );

	// Setup work
	virtual bool setup( void );

	// Loop through the tests
	virtual bool runLoops( void ) = 0;

	// Finish up
	virtual bool finish( void );

	// Verbose?
	int Verbose( void ) const {
		return m_options.getVerbosity();
	};
	bool isVerbose( int level ) const {
		return (m_options.getVerbosity() >= level);
	};

  protected:
	const CaBenchOptions	&m_options;
	CaBenchSampleSet		 m_samples;
};

#endif
