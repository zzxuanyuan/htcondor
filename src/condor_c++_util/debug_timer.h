/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#ifndef __DEBUG_TIMER_H__
#define __DEBUG_TIMER_H__

#include "condor_common.h"

// Simple base timer -- the base of the hiearchy
class DebugTimerSimple
{
  public:
	DebugTimerSimple( bool sample = true );
	DebugTimerSimple( const DebugTimerSimple &ref, bool sample = true );
	virtual ~DebugTimerSimple( void );

	double Sample( bool store = true );
	double Get( void ) const { return m_time; };

	// Diff methods
	double Diff( double ref ) const { return m_time - ref; };
	double Diff( const DebugTimerSimple &ref ) const {
		return Get() - ref.Get();
	};
	double Diff( void ) const {
		assert( NULL != m_ref );
		return Diff(m_ref);
	}

  protected:
	const DebugTimerSimple &GetRef( void ) const;

  private:
	double	dtime( void ) const;
	double	m_time;
	const DebugTimerSimple *m_ref;
};

// Debug timer with output
class DebugTimerOut : public DebugTimerSimple
{
  public:
	DebugTimerOut( const char *label,
				   bool sample = true );
	DebugTimerOut( const DebugTimerSimple &ref,
				   const char *label,
				   bool sample = true );
	~DebugTimerOut( void );

	// Basic logging methods
	void Log(const DebugTimerSimple &ref) const;
	void Log(const DebugTimerSimple &ref, int count) const;
	void Log(const DebugTimerSimple &ref, int count, const char *label) const;

	// These assume that a reference has been defined!
	void Log(void ) const;
	void Log(int count ) const;
	void Log(int count, const char *label ) const;

  protected:
	virtual void Output( const char *) const = 0;

	void Log( double diff ) const;
	void Log( double diff, int count ) const;
	void Log( double diff, int count, const char *label ) const;

  private:
	const char *m_label;
};

#endif//__DEBUG_TIMER_H__
