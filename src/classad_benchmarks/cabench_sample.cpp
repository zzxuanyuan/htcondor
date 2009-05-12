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

#include "condor_common.h"
#include "condor_debug.h"
#include "debug_timer_printf.h"

#include "cabench_sample.h"
#include <list>


CaBenchSample::CaBenchSample( bool is_baseline, const char *name, int count )
		: m_name( name ),
		  m_pi( NULL ),
		  m_timer( NULL ),
		  m_count( count ),
		  m_use_count( 0 )
{
	int		status;
    ProcAPI::getProcInfo(getpid(), m_pi, status);
	if ( ! is_baseline ) {
		m_timer = new DebugTimerPrintf( name, true );
	}
	else {
		m_timer = new DebugTimerSimple( );
	}
}

CaBenchSample::~CaBenchSample( void )
{
	m_name = NULL;
	delete m_pi;
	m_pi = NULL;
	delete m_timer;
	m_timer = NULL;
}

bool
CaBenchSample::dump( void ) const
{
	printf( "%s @ %13.2fs: %luk %luk\n",
			getName(), m_timer->Sample(false), m_pi->imgsize, m_pi->rssize );
	return true;
}

bool
CaBenchSample::dump( const CaBenchSample &ref ) const
{
	return dump( ref, getName(), getCount() );
}

bool
CaBenchSample::dump( const CaBenchSample &ref,
					 const char *name,
					 int count) const
{
	unsigned long	imgdiff  = (m_pi->imgsize - ref.getImageSize() );
	unsigned long	rssdiff  = (m_pi->rssize  - ref.getRSS() );
	double			timediff = m_timer->Diff( ref.getTimer() );

	DebugTimerPrintf	*dtp = dynamic_cast<DebugTimerPrintf *>(m_timer);
	if ( NULL == dtp ) {
		printf( "  %-20s @ %9.5fs %8luk %8luk [diff]\n",
				name, timediff, imgdiff, rssdiff );
	}
	else {
		if ( count >= 0 ) {
			dtp->Log( ref.getTimer(), count, name );
		}
		else {
			dtp->Log( ref.getTimer() );
		}
		printf( "  %-20s @ %9.5fs %8luk %8luk [diff]\n",
				name, timediff, imgdiff, rssdiff );
	}
	return true;
}


//
// CaBenchSampleRef methods
//
CaBenchSampleRef::CaBenchSampleRef( CaBenchSample *sample )
		: m_sample( sample ),
		  m_name( NULL ),
		  m_count( -1 )
{
	sample->incUseCount();
}

CaBenchSampleRef::CaBenchSampleRef( CaBenchSample *sample,
									const char *name,
									int count )
		: m_sample( sample ),
		  m_name( name ),
		  m_count( count )
{
	sample->incUseCount();
}

CaBenchSampleRef::~CaBenchSampleRef( void )
{
	if ( m_sample->decUseCount() <= 0 ) {
		delete m_sample;
	}
	m_sample = NULL;
}

//
// CaBenchSampleSet methods
//
CaBenchSampleSet::CaBenchSampleSet( void )
		: m_baseline( NULL )
{
}

CaBenchSampleSet::CaBenchSampleSet( const char *name )
		: m_baseline( NULL )
{
	init( name );
}

CaBenchSampleSet::CaBenchSampleSet( CaBenchSample *ref )
		: m_baseline( NULL )
{
	init( ref );
}

CaBenchSampleSet::CaBenchSampleSet( CaBenchSampleSet *ref )
		: m_baseline( NULL )
{
	init( ref->getBaseline() );
}

CaBenchSampleSet::~CaBenchSampleSet( void )
{
	list <CaBenchSampleRef *>::iterator iter;
	for ( iter = m_samples.begin(); iter != m_samples.end(); iter++ ) {
		CaBenchSampleRef	*sample = *iter;
		delete sample;
	}
	m_samples.clear();

	if ( m_baseline ) {
		delete m_baseline;
		m_baseline = NULL;
	}
}

bool
CaBenchSampleSet::init( const char *name )
{
	if ( NULL == name ) {
		name = "baseline";
	}
	CaBenchSample *baseline = new CaBenchSample( true, name );
	return init( baseline );
}

bool
CaBenchSampleSet::init( CaBenchSample *ref )
{
	m_baseline = new CaBenchSampleRef( ref );
	return true;
}

bool
CaBenchSampleSet::addSample( const char *label, int count )
{
	CaBenchSample *sample = new CaBenchSample( false, label, count );
	return addSample( sample );
}
	
bool
CaBenchSampleSet::addSample( CaBenchSample *sample,
							 const char *label,
							 int count )
{
	CaBenchSampleRef	*p = new CaBenchSampleRef( sample, label, count );
	m_samples.push_back( p );
	return true;
}

bool
CaBenchSampleSet::dumpSamples( void ) const
{
	typedef list<CaBenchSampleRef *>		SampleList;
	typedef list<const CaBenchSampleRef *>	ConstList;

	const ConstList *const_list = (const ConstList *) &m_samples;
	ConstList &samples = *(const_cast<ConstList *>(const_list));

	CaBenchSample	*baseline = m_baseline->getSample();
	baseline->dump( );

	list <const CaBenchSampleRef *>::iterator iter;
	for ( iter = samples.begin(); iter != samples.end(); iter++ ) {
		const CaBenchSampleRef	*ref = *iter;
		const CaBenchSample		*sample = ref->getConstSample();
		if ( ref->isLocal() ) {
			sample->dump( *baseline, ref->getName(), ref->getCount() );
		}
		else {
			sample->dump( *baseline );
		}
	}
	return true;
}
