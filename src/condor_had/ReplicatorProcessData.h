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

#ifndef REPLICATOR_PROCESS_DATA_H
#define REPLICATOR_PROCESS_DATA_H

#include <list>

using namespace std;

/* The structure encapsulates process id and the last timestamp of the
 * process creation. The structure is used for downloading/uploading
 * transferer processes
 */
class ReplicatorFile;
class ReplicatorProcessData
{
  public:
	ReplicatorProcessData( void )
		: m_pid(-1), m_time(-1) {
	};
	virtual ~ReplicatorProcessData( void );

	// Setters
	bool registerProcess( int pid ) {
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
	virtual ReplicatorFile &getFileInfo( void ) = 0;
	int getPid( void ) const {
		return m_pid;
	};
	time_t getTime( void ) const {
		return m_time;
	};
	bool operator == ( ReplicatorProcessData &other ) const {
		return m_pid == other.getPid( );
	};

  private:
	int			 m_pid;
	time_t		 m_time;
};

class ReplicatorProcessList
{
  public:
	ReplicatorProcessList( void );
	~ReplicatorProcessList( void );

	bool Register( ReplicatorProcessData &process );
	ReplicatorProcessData *Find( int pid );

  private:
	list<ReplicatorProcessData *>	m_list;
};

#endif // REPLICATOR_PROCESS_DATA_H
