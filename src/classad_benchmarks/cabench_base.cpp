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


CaBenchBase::CaBenchBase( 
	const CaBenchOptions &options ) 
		: m_options( options )
{
}

CaBenchBase::~CaBenchBase( void )
{
}

bool
CaBenchBase::setup( void )
{
	return m_samples.init( "main/baseline" );
}

bool
CaBenchBase::finish( void )
{
	m_samples.addSample( false, "done" );
	m_samples.printAll( );
	return true;
}
