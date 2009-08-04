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

#ifndef REPLICATOR_TRANSFERER_H
#define REPLICATOR_TRANSFERER_H

#include <list>
using namespace std;

/* The structure encapsulates process id and the last timestamp of the
 * process creation. The structure is used for downloading/uploading
 * transferer processes
 */
class ReplicatorFileBase;
class ReplicatorTransferer
{
  public:
	ReplicatorTransferer( void )
		: m_pid(-1), m_time(-1) {
	};
	virtual ~ReplicatorTransferer( void );

	// Setters
	bool activate( int pid ) {
		m_pid = pid;
		if ( pid > 0 ) {
			m_time = time(NULL);
		} else {
			m_time = -1;
		}
		return true;
	};
	bool clear( void ) {
		m_pid  = -1;
		m_time = -1;
		return true;
	};

	// Accessors
	bool isValid( void ) const {
		return (  (m_pid != -1) && (m_time != -1)  );
	};
	bool isActive( void ) const {
		return (m_pid != -1);
	};
	virtual ReplicatorFileBase &getFileInfo( void ) = 0;
	int getPid( void ) const {
		return m_pid;
	};
	time_t getTime( void ) const {
		return m_time;
	};
	time_t getAge( time_t now = 0 ) const;
	bool kill( int sig ) const;

	virtual bool cleanupTempFiles( void ) const = 0;

  private:
	int			 m_pid;
	time_t		 m_time;
};

class ReplicatorTransfererList
{
  public:
	ReplicatorTransfererList( void );
	virtual ~ReplicatorTransfererList( void );
	bool clear( void );

	bool Register( ReplicatorTransferer &process );
	ReplicatorTransferer * Find( int pid );
	int numActive( void ) const;

	bool killAll( int sig ) const;

  protected:
	bool killTransList( int sig, const list<ReplicatorTransferer *>& );
	int getOldTransList( time_t maxage, list<ReplicatorTransferer *>& );

  protected:
	list<ReplicatorTransferer *>	m_list;
};

#endif // REPLICATOR_TRANSFERER_H
