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

#ifndef CLASSAD_BENCHMARK_CONSTRAINTS_OPTIONS_H
#define CLASSAD_BENCHMARK_CONSTRAINTS_OPTIONS_H

using namespace std;

// Options
class ClassAdConstraintBenchmarkOptions
{
public:
	ClassAdConstraintBenchmarkOptions( void );
	~ClassAdConstraintBenchmarkOptions( void ) { };

	bool Verify( void ) const;

	// Accessors
	void setVerbosity( int v ) { m_verbosity = v; };
	void incVerbosity( void ) { m_verbosity++; };
	int getVerbosity( void ) const { return m_verbosity; };

	void setNumAds( int num ) { m_num_ads = num; };
	int getNumAds( void ) const { return m_num_ads; };
	void setNumQueries( int num ) { m_num_queries = num; };
	int getNumQueries( void ) const { return m_num_queries; };
	void setAdFile( const char *f ) { m_ad_file = f; };
	const char * getAdFile( void ) const { return m_ad_file; };
	void setQuery( const char *q ) { m_query = q; };
	const char * getQuery( void ) const { return m_query; };
	void setViewExpr( const char *expr ) { m_view_expr = expr; };
	const char * getViewExpr( void ) const { return m_view_expr; };
	void setTwoWay( bool two_way ) { m_two_way = two_way; };
	bool getTwoWay( void ) const { return m_two_way; };

private:
	int			 m_verbosity;
	int			 m_num_ads;
	int			 m_num_queries;
	const char	*m_ad_file;
	const char	*m_query;
	const char	*m_view_expr;
	bool		 m_two_way;
};

#endif
