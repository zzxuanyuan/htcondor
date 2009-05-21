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
#include "simple_arg.h"

#include "cabench_query_options.h"

CaBenchQueryOptions::CaBenchQueryOptions( const char *v,
										  bool support_views,
										  const char *name ) 
		: CaBenchOptions( v, name ),
		  m_support_views( support_views ),
		  m_filter_expr( NULL ),
		  m_query( NULL ),
		  m_query_enabled( false ),
		  m_view_expr( NULL ),
		  m_two_way( false )
{
	/* Do nothing */
}

bool
CaBenchQueryOptions::Verify( void ) const
{
	if ( m_query == NULL ) {
		printf( "No query specified; queries disabled\n" );
	}
	return CaBenchOptions::Verify( );
}

const char *
CaBenchQueryOptions::getUsage( void ) const
{
	static const char *	usage = "[<query>]";
	return usage;
}

const char *
CaBenchQueryOptions::getOpts( void ) const
{
	static const char *	opts =
		"  --filter <expr>: Filter ads from file with <expr>\n"
		"  --view <expr>: Use view with <expr>\n"
		"  --disable-view: Disable view\n"
		"  --[en|dis]able-2way: En/Dis-able 2-way matching <disabled>\n"
		"";
	return opts;
}

const char *
CaBenchQueryOptions::getFixed( void ) const
{
	static const char *	fixed =
		"  <query>: query constraint (or '-' for none)\n"
		"";
	return fixed;
}

CaBenchQueryOptions::OptStatus
CaBenchQueryOptions::ProcessArgLocal(SimpleArg &arg,
									 int /*index*/ )
{
	if ( arg.Match( "view" ) ) {
		if ( !m_support_views ) {
			fprintf( stderr, "Views not supported by %s\n", getName() );
			return CaBenchOptions::OPT_ERROR;
		}
		m_view_expr = NULL;
		if ( !arg.getOpt( m_view_expr, true ) ) {
			fprintf(stderr, "No view expr specified\n" );
			return CaBenchOptions::OPT_ERROR;
		}
		return CaBenchOptions::OPT_HANDLED;
	}
	else if ( arg.Match( "filter" ) ) {
		m_filter_expr = NULL;
		if ( !arg.getOpt( m_filter_expr, true ) ) {
			fprintf(stderr, "No filter expr specified\n" );
			return CaBenchOptions::OPT_ERROR;
		}
		return CaBenchOptions::OPT_HANDLED;
	}
	else if ( arg.Match( "disable-view" ) ) {
		m_view_expr = NULL;
		return CaBenchOptions::OPT_HANDLED;
	}
	else if ( arg.Match( "enable-2way" ) ) {
		m_two_way = true;
		return CaBenchOptions::OPT_HANDLED;
	}
	else if ( arg.Match( "disable-2way" ) ) {
		m_two_way = false;
		return CaBenchOptions::OPT_HANDLED;
	}

	return CaBenchOptions::OPT_OTHER;
}

CaBenchQueryOptions::OptStatus
CaBenchQueryOptions::ProcessArgLocal( SimpleArg &arg,
									  int /*index*/,
									  int fixed )
{
	if ( 0 == fixed ) {
		if ( !arg.getOpt( m_query ) ) {
			fprintf(stderr, "Invalid query %s\n", arg.Arg() );
			return CaBenchOptions::OPT_ERROR;
		}
		if ( strcmp( m_query, "-" ) ) {
			m_query_enabled = true;
		}
		return CaBenchOptions::OPT_HANDLED;
	}

	return CaBenchOptions::OPT_OTHER;
}
