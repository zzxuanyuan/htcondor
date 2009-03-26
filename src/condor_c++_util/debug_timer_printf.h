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

#ifndef __DEBUG_TIMER_PRINTF_H__
#define __DEBUG_TIMER_PRINTF_H__

#include "debug_timer.h"
#include <stdio.h>

// Debug timer which outputs via printf()
class DebugTimerPrintf : public DebugTimerBase
{
  public:
	DebugTimerPrintf( bool start = true ) : DebugTimerBase( start ) { };
	virtual ~DebugTimerPrintf( void ) { };
	virtual void Output( const char *buf ) {
		fputs( buf, stdout );
	}

  private:
};

#endif//__DEBUG_TIMER_DPRINTF_H__
