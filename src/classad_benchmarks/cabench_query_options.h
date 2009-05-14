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

#ifndef CABENCH_QUERY_OPTIONS_H
#define CABENCH_QUERY_OPTIONS_H

#include "cabench_options.h"
using namespace std;

// Options
class CaBenchQueryOptions : public CaBenchOptions
{
public:
	CaBenchQueryOptions( const char *version, bool views, const char *name );
	~CaBenchQueryOptions( void ) { };

	// Process command line
	OptStatus ProcessArgLocal( SimpleArg &arg, int &fixed, int &index );
	const char *getUsage( void ) const;
	const char *getOpts( void ) const;
	const char *getFixed( void ) const;
	bool Verify( void ) const;

	// Accessors
	const char *getFilterExpr( void ) const { return m_filter_expr; };
	const char *getQuery( void ) const { return m_query; };
	const char *getViewExpr( void ) const { return m_view_expr; };
	bool getTwoWay( void ) const { return m_two_way; };

private:
	bool		 m_support_views;
	const char	*m_filter_expr;
	const char	*m_query;
	const char	*m_view_expr;
	bool		 m_two_way;
};

#endif
