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
#include "condor_random_num.h"
#include "debug_timer_printf.h"

#include "cabench_adwrap_base.h"
#include "cabench_inst_base.h"

#include <list>

CaBenchInstData::CaBenchInstData( int lineno, char *linebuf )
		: m_type( NONE ),
		  m_attribute( NULL ),
		  m_dup_percent( 0.0 ),
		  m_dup_v( 0.0 ),
		  m_value_boolean( false ),
		  m_value_integer( 0 ),
		  m_value_string( NULL )
{
	char		*cur;
	char		*next;
	char		*tmp;
	DataType	type = NONE;

	cur = linebuf;
	next = strchr( cur, ':' );
	if ( NULL == next ) {
		fprintf( stderr, "Failed to parse line %d: missing type\n", lineno );
	}
	*next = '\0';
	next++;
	if ( strcmp( cur, "bool" ) == 0 ) {
		type = BOOLEAN;
	}
	else if ( strcmp( cur, "int" ) == 0 ) {
		type = INTEGER;
	}
	else if ( strcmp( cur, "float" ) == 0 ) {
		type = FLOAT;
	}
	else if ( strcmp( cur, "str" ) == 0 ) {
		type = STRING;
	}
	else {
		fprintf( stderr, "Failed to parse line %d: invalid type '%s'\n",
				 lineno, cur );
	}

	cur = next;
	next = strchr( cur, ':' );
	if ( NULL == next ) {
		fprintf( stderr, "Failed to parse line %d: missing duplicate %%\n",
				 lineno);
		return;
	}
	*next = '\0';
	next++;
	tmp = NULL;
	m_dup_percent = strtod( cur, &tmp );
	m_dup_v = m_dup_percent / 100.0;
	if ( NULL == tmp ) {
		fprintf( stderr,
				 "Failed to parse line %d: invalid duplicate %% '%s'\n",
				 lineno, cur );
		return;
	}

	cur = next;
	next = strchr( cur, ':' );
	if ( NULL == next ) {
		fprintf( stderr,
				 "Failed to parse line %d: missing attribute name\n", lineno );
		return;
	}
	*next = '\0';
	next++;
	if ( *cur == '\0' ) {
		fprintf( stderr,
				 "Failed to parse line %d: invalid attribute '%s'\n",
				 lineno, cur );
		return;
	}
	m_attribute = strdup( cur );
	if ( NULL == m_attribute ) {
		fprintf( stderr,
				 "Failed to parse line %d: strdup('%s') failed\n",
				 lineno, cur );
		return;
	}

	cur = next;
	if ( NULL == cur ) {
		fprintf( stderr,
				 "Failed to parse line %d: missing value\n", lineno );
		return;
	}
	if ( BOOLEAN == type ) {
		if ( !strcasecmp( cur, "false" ) ) {
			m_value_boolean = false;
		}
		else if ( !strcasecmp( cur, "true" ) ) {
			m_value_boolean = true;
		}
		else {
			fprintf( stderr,
					 "Failed to parse line %d: invalid boolean '%s'\n",
					 lineno, cur );
			return;
		}
	}
	else if ( INTEGER == type ) {
		tmp = NULL;
		m_value_integer = strtol( cur, &tmp, 10 );
		if ( NULL == tmp ) {
			fprintf( stderr,
					 "Failed to parse line %d: invalid integer '%s'\n",
					 lineno, cur );
			return;
		}
	}
	else if ( FLOAT == type ) {
		tmp = NULL;
		m_value_float = strtod( cur, &tmp );
		if ( NULL == tmp ) {
			fprintf( stderr,
					 "Failed to parse line %d: invalid float '%s'\n",
					 lineno, cur );
			return;
		}
	}
	else if ( STRING == type ) {
		m_value_string = strdup( cur );
		if ( NULL == m_value_string ) {
			fprintf( stderr,
					 "Failed to parse line %d: "
					 "strdup(%.20s) (len %d) failed\n",
					 lineno, cur, strlen(cur) );
			return;
		}
	}

	// Made it here?  All good
	m_type = type;
}

CaBenchInstData::~CaBenchInstData( void )
{
	if ( m_attribute ) {
		free( const_cast<char*>(m_attribute) );
		m_attribute = NULL;
	}
	if ( m_value_string ) {
		free( const_cast<char*>(m_value_string) );
		m_value_string = NULL;
	}
	m_type = NONE;
}


// Instantiation benchmark base class methods
CaBenchInstBase::CaBenchInstBase( 
	const CaBenchInstOptions &options ) 
		: CaBenchBase( options )
{
}

CaBenchInstBase::~CaBenchInstBase( void )
{
}

bool
CaBenchInstBase::setup( void )
{
	// Invoke the base class's setup
	if ( !CaBenchBase::setup( ) ) {
		return false;
	}
	return readDataFile( );
}

bool
CaBenchInstBase::readDataFile( void )
{
	FILE	*fp = fopen( Options().getDataFile( ), "r" );
	if ( NULL == fp ) {
		fprintf( stderr, "Error opening %s\n", Options().getDataFile() );
		return false;
	}

	char	buf[102400];
	int		lineno = 0;
	while( fgets( buf, sizeof(buf), fp ) != NULL ) {
		buf[sizeof(buf)-1] = '\0';
		int		len = strlen(buf);
		while ( len  && ('\n' == buf[len-1]) ) {
			buf[len-1] = '\0';
			len--;
		}
		if ( '\0' == buf[0] ) {
			break;
		}
		CaBenchInstData	*dp = new CaBenchInstData( ++lineno, buf );
		if ( !dp->isValid() ) {
			fprintf( stderr, "Error reading data file\n" );
			fclose( fp );
			return false;
		}
		m_avlist.push_back( dp );
	}
	fclose( fp );
	printf( "Read %d A/V lines\n", m_avlist.size() );

	return m_avlist.size() != 0;
}

bool
CaBenchInstBase::runLoops( void )
{
	int		num_loops	= Options().getNumLoops();
	char	buf[32];

	CaBenchSampleSet samples( "loops" );
	for( int loop = 0;  loop < num_loops;  loop++ ) {
		snprintf( buf, sizeof(buf), "loop %d", loop+1 );
		printf( "\n** loop %d **\n", loop+1 );

		CaBenchSampleSet	lsamples( buf );
		CaBenchSamplePair	pair( "init" );
		if ( !initAds( Options().getNumAds() ) ){
			return false;
		}
		pair.complete( Options().getNumAds() );
		lsamples.addSample( "generation", Options().getNumAds() );

		list <CaBenchInstData *>::iterator iter = m_avlist.begin();
		int		avno = 0;
		int		dups = 0;
		int		num = Options().getNumAds() * Options().getNumAttrs();

		pair.restart( "population" );
		for( int adno = 0;  adno < Options().getNumAds();  adno++ ) {
			for( int attr = 0;  attr < Options().getNumAttrs();  attr++ ) {
				CaBenchInstData *dp = *iter;
				if ( !addAttr( *dp, adno, avno ) ) {
					return false;
				}
				if ( dups ) {
					dups--;
				}
				else {
					dups = dp->getDupAds( Options().getNumAds() );
					iter++;
					avno++;
					if ( iter == m_avlist.end() ) {
						printf("Recycling attrs\n");
						iter = m_avlist.begin();
						avno = 0;
					}
				}
			}
		}
		pair.complete( num );
		lsamples.addSample( "Population", num );

		pair.restart( "delete" );
		if ( !deleteAds( ) ) {
			return false;
		}
		pair.complete( Options().getNumAds() );
		lsamples.addSample( "Deletion", Options().getNumAds() );
		lsamples.dumpSamples( );
	}
	samples.dumpSamples( );

	return true;
}

bool
CaBenchInstBase::addAttr( const CaBenchInstData &dp, int adno, int avno )
{
	switch ( dp.getType() ) {
	case CaBenchInstData::BOOLEAN:
	{
		bool	v;
		if ( !dp.getValue( v ) ) {
			fprintf( stderr, "Error pulling boolean from dp #%d!\n", avno );
			return false;
		}
		addAttr( adno, dp.getAttr(), v );
		break;
	}
	case CaBenchInstData::INTEGER:
	{
		int		v;
		if ( !dp.getValue( v ) ) {
			fprintf( stderr, "Error pulling int from dp #%d!\n", avno );
			return false;
		}
		addAttr( adno, dp.getAttr(), v );
		break;
	}
	case CaBenchInstData::FLOAT:
	{
		double	v;
		if ( !dp.getValue( v ) ) {
			fprintf( stderr, "Error pulling double from dp #%d!\n", avno );
			return false;
		}
		addAttr( adno, dp.getAttr(), v );
		break;
	}
	case CaBenchInstData::STRING:
	{
		const char	*v;
		if ( !dp.getValue( v ) ) {
			fprintf( stderr, "Error pulling string from dp #%d!\n", avno );
			return false;
		}
		addAttr( adno, dp.getAttr(), v );
		break;
	}
	default:
		return false;
	}
	return true;
}

bool
CaBenchInstBase::finish( void )
{
	list <CaBenchInstData *>::iterator iter;
	for ( iter = m_avlist.begin(); iter != m_avlist.end(); iter++ ) {
		CaBenchInstData *dp = *iter;
		delete dp;
	}
	m_avlist.clear( );
	return true;
}
