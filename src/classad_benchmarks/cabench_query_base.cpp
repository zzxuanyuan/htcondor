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
		: CaBenchBase( options ),
		  m_num_ads( 0 ),
		  m_filter_str( NULL )
{
}

CaBenchQueryBase::~CaBenchQueryBase( void )
{
}

bool
CaBenchQueryBase::setup( void )
{
	const char	*view_expr = Options().getViewExpr();

	// Invoke the base class's setup
	if ( !CaBenchBase::setup( ) ) {
		return false;
	}

	// Scan the template file
	if ( !scanAdFile( ) ) {
		return false;
	}

	// Template ad?
	int		num_templates = numTemplates();
	if ( !num_templates ) {
		return false;
	}

	// Computer # of target ads
	int		num_ads = Options().getNumAds();
	if ( num_ads <= 0 ) {
		num_ads = num_templates * Options().getAdMult( );
	}
	printf( "Target: %d ads\n", num_ads );

	// Setup the view
	if ( view_expr && (!createView( view_expr ) )  ) {
		return false;
	}

	const char	*fname = Options().getDataFile();
	FILE		*fp = fopen( fname, "r" );
	if ( !fp ) {
		fprintf( stderr, "Error opening %s\n", fname );
		return false;
	}

    // Use ProcAPI to get what we can
	CaBenchSampleSet samples( "setup" );
	for( int i = 0;  i < num_ads;  i++ ) {
		int			 ad_num;
		if ( Options().getUseRandom() ) {
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
	samples.addSample( "done", num_ads );
	fclose( fp );

	samples.dumpSamples( );

	if ( !printCollectionInfo( ) ) {
		return false;
	}

	m_num_ads = num_ads;

	return true;
}

bool
CaBenchQueryBase::scanAdFile( void )
{
	const char	*fname = Options().getDataFile();
	FILE		*fp = fopen( fname, "r" );
	if ( !fp ) {
		fprintf( stderr, "Error opening %s\n", fname );
		return false;
	}

	m_filter_str = Options().getFilterExpr( );
	initFilter( );
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
		bool match = filterAd( template_ad );

		template_ad->setDtorDelAd( true );
		delete( template_ad );

		if ( match ) {
			m_template_offsets.push_back( offset );
		}
	}
	fclose( fp );
	printf( "Found %d template ads\n", numTemplates() );
	return ( numTemplates() != 0 );
}

bool
CaBenchQueryBase::runLoops( void )
{
	int					 total_matches = 0;
	int					 view_members;
	int					 num_loops	= Options().getNumLoops();
	const char			*query      = Options().getQuery();
	bool				 two_way	= Options().getTwoWay();

	if ( !strcmp( query, "-" ) ) {
		return true;
	}

	CaBenchSampleSet samples( "queries" );
	getViewMembers( view_members);
	for( int loop = 0;  loop < num_loops;  loop++ ) {
		int					 matches = 0;
		CaBenchSampleSet qsamples( "query" );
		if ( !runQuery( query, loop, two_way, matches ) ) {
			fprintf( stderr, "runQuery failed\n" );
			return false;
		}

		CaBenchSample *sample = new CaBenchSample( false, "done" );
		qsamples.addSample( sample );
		qsamples.addSample( sample, "Ads", m_num_ads );
		qsamples.addSample( sample, "View Members", view_members );
		qsamples.addSample( sample, "Query matches", matches );
		qsamples.dumpSamples();
		total_matches += matches;
	}
	
	CaBenchSample *samp = new CaBenchSample( false, "done" );
	samples.addSample( samp, "Searches", num_loops );
	samples.addSample( samp, "Total Ads", m_num_ads * num_loops );
	samples.addSample( samp, "Total View Members", view_members * num_loops );
	samples.addSample( samp, "Total Query matches", total_matches );

	return true;
}

bool
CaBenchQueryBase::finish( void )
{
	printf( "Final ad count: %d\n", getAdCount() );

	m_template_offsets.clear( );
	return true;
}
