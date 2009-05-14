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

#ifndef CABENCH_OPTIONS_H
#define CABENCH_OPTIONS_H

#include "simple_arg.h"

// Options
class CaBenchOptions
{
public:
	enum OptStatus { OPT_ERROR = -1,
					 OPT_DONE,
					 OPT_HANDLED,
					 OPT_HELP,
					 OPT_OTHER };
	CaBenchOptions( const char *version, const char *name );
	virtual ~CaBenchOptions( void ) { };

	// Process command line
	OptStatus ProcessArgs( int argc, const char *argv[] );
	virtual OptStatus ProcessArgLocal( SimpleArg &arg,
									   int index ) = 0;
	virtual OptStatus ProcessArgLocal( SimpleArg &arg,
									   int index,
									   int fixed ) = 0;

	void Usage( void ) const;
	virtual const char *getUsage( void ) const = 0;
	virtual const char *getOpts( void ) const = 0;
	virtual const char *getFixed( void ) const = 0;
	virtual bool Verify( void ) const;

	const char *getName( void ) { return m_name; };

	const char *getDataFile( void ) const { return m_data_file; };
	int getNumLoops( void ) const { return m_num_loops; };
	int getNumAds( void ) const { return m_num_ads; };
	int getAdMult( void ) const { return m_ad_mult; };
	bool getUseRandom( void ) const { return m_random; };

	// Accessors
	int getVerbosity( void ) const { return m_verbosity; };

  private:
	OptStatus ProcessArg( SimpleArg &arg, int index );
	OptStatus ProcessArg( SimpleArg &arg, int index, int &fixed );

  private:
	const char	*m_version;
	const char	*m_name;

	int			 m_verbosity;

	int			 m_num_loops;
	const char	*m_data_file;
	int			 m_num_ads;
	int			 m_ad_mult;

	bool		 m_random;
};

#endif
