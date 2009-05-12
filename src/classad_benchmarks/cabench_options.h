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
	enum OptStatus { OPT_ERROR = -1, OPT_DONE, OPT_HANDLED, OPT_OTHER };
	CaBenchOptions( const char *version, const char *name, const char *opts );
	virtual ~CaBenchOptions( void ) { };

	// Process command line
	OptStatus ProcessArg( SimpleArg &arg, int &index );
	virtual bool ProcessArgs( int argc, const char *argv[] ) = 0;

	virtual void Usage( void ) const;
	virtual bool Verify( void ) const = 0;
	virtual const char *getOpts( void ) const = 0;

	const char *getName( void ) { return m_name; };

	// Accessors
	int getVerbosity( void ) const { return m_verbosity; };

private:
	const char	*m_version;
	const char	*m_name;
	const char	*m_opts;

	int			 m_verbosity;
};

#endif
