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

#include <vector>
#include "debug_timer.h"
#include "debug_timer_printf.h"

// Abstract base class
class CaBenchSampleBase
{
  public:
	CaBenchSampleBase( DebugTimerSimple *timerp,
					   bool const_use_count,
					   const char *name, int count = -1 );
	virtual ~CaBenchSampleBase( void );
	virtual void abstract( void ) const = 0;

	bool restart( const char *name = NULL );

	bool print( void ) const;
	bool print( const CaBenchSampleBase &other ) const;
	bool print( const CaBenchSampleBase &other,
				const char *name, int count ) const;

	// Accessors
	bool setName( const char *name );
	const char *getName( void ) const { return m_name; };
	bool setCount( int count ) { m_count = count; return true; };
	int getCount( void ) const { return m_count; };

	const procInfo* getPI( void ) const { return &m_pi; };
	unsigned long getImageSize( void ) const { return m_pi.imgsize; };
	unsigned long getRSS( void ) const { return m_pi.rssize; };

	const DebugTimerSimple &getTimer( void ) const {
		return *m_timerp;
	};
	virtual bool isBaseline( void ) const = 0;

	int	getUseCount( void ) const {
		return ( m_const_use_count ? 1 : m_use_count );
	};
	int incUseCount( void ) { return ++m_use_count; };
	int decUseCount( void ) { return --m_use_count; };

	// private methods
  private:
	bool reset( void );
	bool start( void );

  private:
	DebugTimerSimple	*m_timerp;
	bool				 m_const_use_count;
	char				 m_name[64];
	struct procInfo		 m_pi;
	int					 m_count;
	int					 m_use_count;
};

class CaBenchSampleBaseline : public CaBenchSampleBase
{
  public:
	CaBenchSampleBaseline( bool const_use_count, const char *name );
	~CaBenchSampleBaseline( void );
	void abstract( void ) const { };

	virtual bool isBaseline( void ) const { return true; };

  private:
	DebugTimerSimple	m_timer;
};

class CaBenchSample : public CaBenchSampleBase
{
  public:
	CaBenchSample( bool const_use_count,
				   const char *name, int count = -1 );
	~CaBenchSample( void );
	void abstract( void ) const { };

	virtual const DebugTimerSimple &getTimer( void ) const {
		return m_timer;
	};
	virtual bool isBaseline( void ) const { return false; };

  private:
	DebugTimerPrintf	m_timer;
};

class CaBenchSamplePair
{
  public:
	CaBenchSamplePair( void );
	CaBenchSamplePair( const char *name );
	~CaBenchSamplePair( void );

	bool complete( int count = -1 );
	bool restart( const char *name = NULL );

  private:
	CaBenchSampleBaseline	m_baseline;
};

class CaBenchSampleRef
{
  public:
	CaBenchSampleRef( CaBenchSampleBase *sample );
	CaBenchSampleRef( CaBenchSampleBase *sample,
					  const char *label, int count = -1 );
	~CaBenchSampleRef( void );
	CaBenchSampleBase *getSample( void ) {
		return m_sample;
	}
	const CaBenchSampleBase *getConstSample( void ) const {
		return m_sample;
	}
	const char *getLabel( void ) const { return m_label; };
	int getCount( void ) const { return m_count; };
	bool isLocal( void ) const { return m_label != NULL; };

  private:
	CaBenchSampleBase	*m_sample;
	const char			*m_label;
	int					 m_count;
};

class CaBenchSampleSet
{
  public:
	CaBenchSampleSet( void );
	CaBenchSampleSet( const char *name );
	CaBenchSampleSet( CaBenchSampleBase *ref );
	CaBenchSampleSet( CaBenchSampleSet *ref );
	virtual ~CaBenchSampleSet( void );
	bool clearSamples( void );

	// Sampling information
	bool init( const char *name = NULL );
	bool init( CaBenchSampleBase *ref, const char *name = NULL );
	bool reInit( void );
	bool reInit( CaBenchSampleBase *ref );
	bool addSample( const char *label, int count = -1 );
	bool addSample( CaBenchSampleBase *sample,
					const char *label = NULL, int count = -1 );
	bool printAll( void ) const;

	bool final( int count = -1 );

	// Accessors
	CaBenchSampleBase *getBaseline( void ) const {
		return getSample(m_baseline);
	};
	CaBenchSampleBase *getSample( unsigned which ) const {
		return getSample(m_samples[which]);
	};
	CaBenchSampleBase *getSample( void ) const {
		return getSample(m_samples.back( ));
	};

  private:
	CaBenchSampleBase *getSample( CaBenchSampleRef *ref ) const {
		if ( NULL == ref ) {
			return NULL;
		}
		return ref->getSample( );
	}

  private:
	CaBenchSampleRef			*m_baseline;
	vector<CaBenchSampleRef *>	 m_samples;
};

#endif
