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

#include "cabench_adwrap_base.h"
#include "cabench_query_base.h"

#include <vector>


CaBenchQueryBase::CaBenchQueryBase( 
	const CaBenchQueryOptions &options ) 
		: m_options( options )
{
}

CaBenchQueryBase::~CaBenchQueryBase( void )
{
}

bool
CaBenchQueryBase::readAdFile( void )
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

		CaBenchAdWrapBase *template_ad = parseTemplateAd( fp );
		if ( !template_ad ) {
			break;		// Do nothing
		}
		else {
			template_ad->setDtorDelAd( true );
			delete( template_ad );
			m_template_offsets.push_back( offset );
		}
	}
	fclose( fp );
	printf( "Read %d template ads\n", numTemplates() );
	return ( numTemplates() != 0 );
}

bool
CaBenchQueryBase::setup( void )
{
	int			 num_ads   = m_options.getNumAds();
	const char	*view_expr = m_options.getViewExpr();
	int			 status;
	piPTR		 pi;

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
	pi = &m_procinfo_init;
    ProcAPI::getProcInfo(getpid(), pi, status);

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
		CaBenchAdWrapBase *template_ad = parseTemplateAd( fp );
		if ( !template_ad ) {
			fclose( fp );
			fprintf( stderr, "Failed to read template ad %d\n", ad_num );
			return false;
		}
		bool	copied;
		if ( !generateInsertAd( template_ad, copied ) ) {
			fclose( fp );
			fprintf( stderr, "Ad generation failed\n" );
			return false;
		}
		template_ad->setDtorDelAd( copied );
		delete template_ad;
	}	
	timer.Log( "setup ads", num_ads );
	fclose( fp );

	pi = &m_procinfo_initdone;
    ProcAPI::getProcInfo(getpid(), pi, status);

	if ( !printCollectionInfo( ) ) {
		return false;
	}

	m_num_ads = num_ads;
	return true;
}

bool
CaBenchQueryBase::runQueries( void )
{
	DebugTimerPrintf	 timer;
	int					 total_matches = 0;
	int					 view_members;
	int					 num_queries	= m_options.getNumQueries();
	const char			*query      	= m_options.getQuery();
	bool				 two_way		= m_options.getTwoWay();
	int					 status;
	piPTR				 pi;

	pi = &m_procinfo_query;
    ProcAPI::getProcInfo(getpid(), pi, status);
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

	pi = &m_procinfo_querydone;
    ProcAPI::getProcInfo(getpid(), pi, status);

	return true;
}

bool
CaBenchQueryBase::cleanup( void )
{
	releaseMemory( );

	struct procInfo	 procinfo;
	piPTR			 pi;
	int				 status;

	pi = &procinfo;
    ProcAPI::getProcInfo(getpid(), pi, status);

	memoryDump( "init",    &m_procinfo_init, true );
	memoryDump( "init",    &m_procinfo_init, &m_procinfo_initdone );
	memoryDump( "query",   &m_procinfo_query, true );
	memoryDump( "query",   &m_procinfo_query, &m_procinfo_querydone );
	memoryDump( "release", &procinfo, true );
	memoryDump( "release", &m_procinfo_init, &procinfo );
	printf( "Final ad count: %d\n", getAdCount() );

	return true;
}

bool
CaBenchQueryBase::releaseMemory( void )
{
	m_template_offsets.clear( );
	return true;
}

void
CaBenchQueryBase::memoryDump(
	const char *label, const piPTR values, bool start ) const
{
	printf( "Memory @ %10s/%-8s: %lu %lu\n",
			label, start?"start":"end", values->imgsize, values->rssize );
}

void
CaBenchQueryBase::memoryDump(
	const char *label, const piPTR ref, const piPTR values ) const
{
	memoryDump( label, values, false );

	unsigned long	imgdiff = (values->imgsize - ref->imgsize);
	unsigned long	rssdiff = (values->rssize  - ref->rssize);
	printf( "  Diff @ %10s/%-8s: %lu %lu\n",
			label, "end", imgdiff, rssdiff );
}
