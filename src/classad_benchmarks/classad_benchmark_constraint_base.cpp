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
#include "stat_wrapper.h"
#include "condor_random_num.h"
#include "debug_timer_printf.h"

#include "classad_benchmark_constraint_base.h"

#include <vector>

// =======================================
// ClassAdGenericBase methods
// =======================================
ClassAdGenericBase::ClassAdGenericBase( bool dtor_del_ad )
		: m_dtor_del_ad(dtor_del_ad)
{
};
ClassAdGenericBase::~ClassAdGenericBase( void )
{
};



// =======================================
// ClassAdConstraintBenchmarkBase methods
// =======================================
ClassAdConstraintBenchmarkBase::ClassAdConstraintBenchmarkBase( 
	const ClassAdConstraintBenchmarkOptions &options ) 
		: m_options( options ),
		  m_procinfo_init( NULL ),
		  m_procinfo_initdone( NULL ),
		  m_procinfo_query( NULL ),
		  m_procinfo_querydone( NULL )
{
}

ClassAdConstraintBenchmarkBase::~ClassAdConstraintBenchmarkBase( void )
{
}

bool
ClassAdConstraintBenchmarkBase::readAdFile( void )
{
	const char	*fname = m_options.getAdFile();
	FILE		*fp = fopen( fname, "r" );
	if ( !fp ) {
		fprintf( stderr, "Error opening %s\n", fname );
		return false;
	}
	while( true ) {
		fpos_t			 offset;
		if ( fgetpos( fp, &offset ) < 0 ) {
			fprintf( stderr, "fgetpos() failed: %d %s\n",
					 errno, strerror(errno) );
			return false;
		}

		ClassAdGenericBase *ad = parseTemplateAd( fp, true );
		if ( !ad ) {
			break;		// Do nothing
		}
		else {
			delete( ad );
			m_template_offsets.push_back( offset );
		}
	}
	fclose( fp );
	printf( "Read %d template ads\n", numTemplates() );
	return ( numTemplates() != 0 );
}

bool
ClassAdConstraintBenchmarkBase::setup( void )
{
	int			 num_ads   = m_options.getNumAds();
	const char	*view_expr = m_options.getViewExpr();
	int			 status;

	// Template ad?
	int		num_templates = numTemplates();
	if ( !num_templates ) {
		return false;
	}

	// Setup the view
	if ( view_expr && (!createView( view_expr ) )  ) {
		return false;
	}

	const char	*fname = m_options.getAdFile();
	FILE		*fp = fopen( fname, "r" );
	if ( !fp ) {
		fprintf( stderr, "Error opening %s\n", fname );
		return false;
	}

    // Use ProcAPI to get what we can
    ProcAPI::getProcInfo(getpid(), m_procinfo_init, status);

	// Generate ads
	DebugTimerPrintf	timer;
	for( int i = 0;  i < num_ads;  i++ ) {
		int			 ad_num;
		if ( m_options.getRandomizeCollection() ) {
			ad_num = (get_random_int() % num_templates);
		}
		else {
			ad_num = ( i % num_templates );
		}

		fpos_t	offset = m_template_offsets[ad_num];
		if ( fsetpos( fp, &offset ) < 0 ) {
			fclose( fp );
			fprintf( stderr, "fsetpos() failed: %d %s\n",
					 errno, strerror(errno) );
			return false;
		}
		ClassAdGenericBase *template_ad =
			parseTemplateAd( fp, collectionCopiesAd() );
		if ( !template_ad ) {
			fclose( fp );
			fprintf( stderr, "Failed to read template ad %d\n",
					 ad_num );
			return false;
		}
		if ( !generateAd( template_ad ) ) {
			fclose( fp );
			fprintf( stderr, "Ad generation failed\n" );
			return false;
		}
		delete template_ad;
	}	
	timer.Log( "setup ads", num_ads );
	fclose( fp );

    ProcAPI::getProcInfo(getpid(), m_procinfo_initdone, status);

	if ( !printCollectionInfo( ) ) {
		return false;
	}

	m_num_ads = num_ads;
	return true;
}

bool
ClassAdConstraintBenchmarkBase::runQueries( void )
{
	DebugTimerPrintf	 timer;
	int					 total_matches = 0;
	int					 view_members;
	int					 num_queries	= m_options.getNumQueries();
	const char			*query      	= m_options.getQuery();
	bool				 two_way		= m_options.getTwoWay();
	int					 status;

    ProcAPI::getProcInfo(getpid(), m_procinfo_query, status);
	getViewMembers( view_members);
	for( int i = 0;  i < num_queries;  i++ ) {
		int					 matches = 0;
		DebugTimerPrintf	 qtimer( false );

		qtimer.Start();
		if ( !runQuery( query, i, two_way, matches ) ) {
			fprintf( stderr, "runQuery failed\n" );
			return false;
		}
		qtimer.Stop();
		qtimer.Log( "Ads", m_num_ads );
		qtimer.Log( "View Members", view_members );
		qtimer.Log( "Query matches", matches );
		total_matches += matches;
	}
	timer.Log( "Searches", num_queries );
	timer.Log( "Total Ads", m_num_ads * num_queries );
	timer.Log( "Total View Members", view_members * num_queries );
	timer.Log( "Total Query matches", total_matches );
    ProcAPI::getProcInfo(getpid(), m_procinfo_querydone, status);

	return true;
}

bool
ClassAdConstraintBenchmarkBase::cleanup( void )
{
	releaseMemory( );

	piPTR	procinfo = NULL;
	int		status;
    ProcAPI::getProcInfo(getpid(), procinfo, status);

	memoryDump( "init", m_procinfo_init, true );
	memoryDump( "init", m_procinfo_init, m_procinfo_initdone );
	memoryDump( "query", m_procinfo_query, true );
	memoryDump( "query", m_procinfo_query, m_procinfo_querydone );
	memoryDump( "release", procinfo, true );
	memoryDump( "release", m_procinfo_init, procinfo );
	printf( "Final ad count: %d\n", getAdCount() );

	delete m_procinfo_init;
	delete m_procinfo_initdone;
	delete m_procinfo_query;
	delete m_procinfo_querydone;
	delete procinfo;

	return true;
}

void
ClassAdConstraintBenchmarkBase::memoryDump(
	const char *label, const piPTR values, bool start ) const
{
	printf( "Memory @ %10s/%-8s: %lu %lu\n",
			label, start?"start":"end", values->imgsize, values->rssize );
}

void
ClassAdConstraintBenchmarkBase::memoryDump(
	const char *label, const piPTR ref, const piPTR values ) const
{
	memoryDump( label, values, false );

	unsigned long	imgdiff = (values->imgsize - ref->imgsize);
	unsigned long	rssdiff = (values->rssize  - ref->rssize);
	printf( "  Diff @ %10s/%-8s: %lu %lu\n",
			label, "end", imgdiff, rssdiff );
}
