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
#include <vector>


//
// CaBenchSample base class methods
//
CaBenchSampleBase::CaBenchSampleBase( DebugTimerSimple *timerp,
									  bool const_use_count,
									  const char *name,
									  int count )
		: m_timerp( timerp ),
		  m_const_use_count( const_use_count ),
		  m_count( count ),
		  m_use_count( 0 )
{
	setName( name );
	start( );
}

CaBenchSampleBase::~CaBenchSampleBase( void )
{
	reset();
}

bool
CaBenchSampleBase::setName( const char *name )
{
	if ( name != NULL ) {
		strncpy( m_name, name, sizeof(m_name) );
	}
	else {
		m_name[0] = '\0';
	}
	return  true;
}

bool
CaBenchSampleBase::reset( void )
{
	setName( NULL );
	return true;
}

bool
CaBenchSampleBase::restart( const char *name )
{
	reset( );
	setName( name );
	return start(  );
}

bool
CaBenchSampleBase::start( void )
{
	int		status;
	piPTR	pi = &m_pi;
    ProcAPI::getProcInfo(getpid(), pi, status);
	m_timerp->Sample( );
	if ( status ) {
		return false;
	}
	return true;
}

bool
CaBenchSampleBase::print( void ) const
{
	printf( "%s @ %13.2fs: %luk %luk\n",
			getName(), m_timerp->Sample(false), m_pi.imgsize, m_pi.rssize );
	return true;
}

bool
CaBenchSampleBase::print( const CaBenchSampleBase &ref ) const
{
	return print( ref, getName(), getCount() );
}

bool
CaBenchSampleBase::print( const CaBenchSampleBase &ref,
						 const char *name,
						 int count) const
{
	long	imgdiff 	= (m_pi.imgsize - ref.getImageSize() );
	long	rssdiff 	= (m_pi.rssize  - ref.getRSS() );
	double	timediff	= m_timerp->Diff( ref.getTimer() );

	DebugTimerPrintf	*dtp = dynamic_cast<DebugTimerPrintf *>(m_timerp);
	if ( NULL != dtp ) {
		if ( count >= 0 ) {
			dtp->Log( ref.getTimer(), count, name );
		}
		else {
			dtp->Log( ref.getTimer() );
		}
	}
	char	imgbuf[32];
	char	rssbuf[32];
	if ( count >= 0 ) {
		double	imgper	= (1.0 * imgdiff) / (1.0 * count);
		double	rssper	= (1.0 * rssdiff) / (1.0 * count);
		snprintf( imgbuf, sizeof(imgbuf), "%ldk (%.2fk/s)", imgdiff, imgper );
		snprintf( rssbuf, sizeof(rssbuf), "%ldk (%.2fk/s)", rssdiff, rssper );
	}
	else {
		snprintf( imgbuf, sizeof(imgbuf), "%ldk", imgdiff );
		snprintf( rssbuf, sizeof(rssbuf), "%ldk", rssdiff );
	}
	printf( "  %-20s @ %9.5fs img:%-20s rss:%-20s [diff]\n",
			name, timediff, imgbuf, rssbuf );
	return true;
}

//
// CaBenchSampleBaseline class methods
//
CaBenchSampleBaseline::CaBenchSampleBaseline( bool const_use_count,
											  const char *name )
		: CaBenchSampleBase( &m_timer, const_use_count, name, -1 ),
		  m_timer( name )
{
}

CaBenchSampleBaseline::~CaBenchSampleBaseline( void )
{
}


//
// CaBenchSample class methods
//
CaBenchSample::CaBenchSample( bool const_use_count,
							  const char *name, int count )
		: CaBenchSampleBase( &m_timer, const_use_count, name, count ),
		  m_timer( name, true )
{
}

CaBenchSample::~CaBenchSample( void )
{
}


//
// CaBenchSamplePair methods
//
CaBenchSamplePair::CaBenchSamplePair( void ) 
		: m_baseline( true, "" )
{
}

CaBenchSamplePair::CaBenchSamplePair( const char *name ) 
		: m_baseline( true, name )
{
}

CaBenchSamplePair::~CaBenchSamplePair( void ) 
{
}

bool
CaBenchSamplePair::complete( int count )
{
	char			buf[128];
	snprintf( buf, sizeof(buf), "%s complete", m_baseline.getName() );
	CaBenchSample	sample( true, buf, count );
	return sample.print( m_baseline );
}

bool
CaBenchSamplePair::restart( const char *name )
{
	return m_baseline.restart( name );
}


//
// CaBenchSampleRef methods
//
CaBenchSampleRef::CaBenchSampleRef( CaBenchSampleBase *sample )
		: m_sample( sample ),
		  m_label( NULL ),
		  m_count( 0 )
{
	sample->incUseCount();
}

CaBenchSampleRef::CaBenchSampleRef( CaBenchSampleBase *sample,
									const char *label,
									int count )
		: m_sample( sample ),
		  m_label( label ),
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

CaBenchSampleSet::CaBenchSampleSet( CaBenchSampleBase *ref )
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
	clearSamples( );

	if ( m_baseline ) {
		delete m_baseline;
		m_baseline = NULL;
	}
}

bool
CaBenchSampleSet::clearSamples( void )
{
	vector <CaBenchSampleRef *>::iterator iter;
	for ( iter = m_samples.begin(); iter != m_samples.end(); iter++ ) {
		CaBenchSampleRef	*sample = *iter;
		delete sample;
	}
	m_samples.clear();
	return true;
}

bool
CaBenchSampleSet::init( const char *name )
{
	if ( NULL == name ) {
		name = "baseline";
	}
	CaBenchSampleBase *baseline = new CaBenchSampleBaseline( false, name );
	return init( baseline );
}

bool
CaBenchSampleSet::reInit( void )
{
	if ( m_baseline ) {
		delete m_baseline;
	}
	clearSamples( );
	return init( );
}

bool
CaBenchSampleSet::reInit( CaBenchSampleBase *ref )
{
	if ( m_baseline ) {
		delete m_baseline;
	}
	clearSamples( );
	return init( ref );
}

bool
CaBenchSampleSet::init( CaBenchSampleBase *ref, const char *name )
{
	if ( NULL == name ) {
		name = "baseline";
	}
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
CaBenchSampleSet::addSample( CaBenchSampleBase *sample,
							 const char *label,
							 int count )
{
	CaBenchSampleRef	*p = new CaBenchSampleRef( sample, label, count );
	m_samples.push_back( p );
	return true;
}

bool
CaBenchSampleSet::final( int count )
{
	addSample( "final", count );
	printAll( );
	clearSamples( );
	return true;
}

bool
CaBenchSampleSet::printAll( void ) const
{
	typedef vector<CaBenchSampleRef *>			SampleList;
	typedef vector<const CaBenchSampleRef *>	ConstList;

	const ConstList *const_list = (const ConstList *) &m_samples;
	ConstList &samples = *(const_cast<ConstList *>(const_list));

	CaBenchSampleBase	*baseline = m_baseline->getSample();
	baseline->print( );

	vector <const CaBenchSampleRef *>::iterator iter;
	for ( iter = samples.begin(); iter != samples.end(); iter++ ) {
		const CaBenchSampleRef	*ref = *iter;
		const CaBenchSampleBase	*sample = ref->getConstSample();
		if ( ref->isLocal() ) {
			sample->print( *baseline, ref->getLabel(), ref->getCount() );
		}
		else {
			sample->print( *baseline );
		}
	}
	return true;
}
