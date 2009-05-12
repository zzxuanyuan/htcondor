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

#ifndef CABENCH_SAMPLE_H
#define CABENCH_SAMPLE_H

#include "../condor_procapi/procapi.h"

using namespace std;

#include <list>
#include "debug_timer.h"

class CaBenchSample
{
  public:
	CaBenchSample( bool is_baseline, const char *name, int count = -1 );
	~CaBenchSample( void );

	bool dump( void ) const;
	bool dump( const CaBenchSample &other ) const;
	bool dump( const CaBenchSample &other, const char *name, int count ) const;

	const char *getName( void ) const { return m_name; };
	int getCount( void ) const { return m_count; };

	const piPTR getPI( void ) const { return m_pi; };
	unsigned long getImageSize( void ) const { return m_pi->imgsize; };
	unsigned long getRSS( void ) const { return m_pi->rssize; };
	const DebugTimerSimple &getTimer( void ) const { return *m_timer; };

	int	getUseCount( void ) const { return m_use_count; };
	int incUseCount( void ) { return ++m_use_count; };
	int decUseCount( void ) { return --m_use_count; };

  private:
	const char				*m_name;
	piPTR					 m_pi;
	DebugTimerSimple		*m_timer;
	int						 m_count;
	int						 m_use_count;
};

class CaBenchSampleRef
{
  public:
	CaBenchSampleRef( CaBenchSample *sample );
	CaBenchSampleRef( CaBenchSample *sample, const char *name, int count );
	~CaBenchSampleRef( void );
	CaBenchSample *getSample( void ) {
		return m_sample;
	}
	const CaBenchSample *getConstSample( void ) const {
		return m_sample;
	}
	const char *getName( void ) const { return m_name; };
	int getCount( void ) const { return m_count; };
	bool isLocal( void ) const { return m_name != NULL; };

  private:
	CaBenchSample	*m_sample;
	const char		*m_name;
	int				 m_count;
};

class CaBenchSampleSet
{
  public:
	CaBenchSampleSet( void );
	CaBenchSampleSet( const char *name );
	CaBenchSampleSet( CaBenchSample *ref );
	CaBenchSampleSet( CaBenchSampleSet *ref );
	virtual ~CaBenchSampleSet( void );

	// Sampling information
	bool init( const char *name = NULL );
	bool init( CaBenchSample *ref );
	bool addSample( const char *label, int count = -1 );
	bool addSample( CaBenchSample *sample,
					const char *label = NULL, int count = -1 );
	bool dumpSamples( void ) const;

	// Accessors
	CaBenchSample *getBaseline( void ) const {
		return m_baseline->getSample();
	};

  private:
	CaBenchSampleRef			*m_baseline;
	list<CaBenchSampleRef *>	 m_samples;
};

#endif
