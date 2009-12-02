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

#ifndef REPLICATOR_VERSION_H
#define REPLICATOR_VERSION_H

#include "condor_common.h"
#include "MyString.h"
#include "ReplicatorFileSet.h"

/* Class      : ReplicatorVersion
 * Description: class, representing a version of state file, including gid of
 *				the pool, logical clock and last modification time of the state
 *				file
 */
class ReplicatorVersion
{
  public:

	// The state of the replication daemon(s)
	enum State
	{
		STATE_INVALID = -1,
		STATE_REQUESTING = 0,
		STATE_DOWNLOADING = 1,
		STATE_BACKUP = 2,
		STATE_LEADER = 3
	};

    /* Constructor / destructors
     */
	ReplicatorVersion( ReplicatorSimpleFileSet &file_set,
					   State state = STATE_INVALID );
	//ReplicatorVersion( const ReplicatorVersion & );
	virtual ~ReplicatorVersion( void );

	// ==== Inspectors ====
	int getMtime( void ) const { return m_mtime; };
    int getGid( void ) const { return m_gid; };
	int getLogicalClock( void ) const { return m_logicalClock; };
	bool getIsPrimary( void ) const { return m_isPrimary; };
	State getState( void ) const { return m_state; };
	bool isState( State state ) const { return state == m_state; };
	bool isValidState( void ) const { return !isState(STATE_INVALID); };
	const ReplicatorSimpleFileSet &getFileSet( void ) const {
		return m_fileset;
	};

	// ==== Comparison operators ====

	/* Function    : isComparable
	 * Arguments   : replica - the compared replica
	 * Return value: bool - true/false value
     * Description : the replicas are comparable, if their gids are identical
     */
    bool operator > ( const ReplicatorVersion &other ) const;
    bool operator >= ( const ReplicatorVersion &other ) const;
    //bool operator < ( const ReplicatorVersion &other ) const;

	// GID comparisons
	bool sameGid( int gid ) const {
		return this->m_gid == gid;
	};
	bool sameGid( const ReplicatorVersion &other ) const {
		return sameGid( other.getGid() );
	};
	bool diffGid( int gid ) const {
		return !sameGid( gid );
	};
	bool diffGid( const ReplicatorVersion &other ) const {
		return diffGid( other.getGid() );
	};
	bool setRandomGid( void );
	bool isComparable( const ReplicatorVersion &other ) const {
		return sameGid( other );
	}

	// Place holder
	virtual bool sameFiles( const ReplicatorVersion & /*other*/ ) const {
		return false;
	};


	// ==== Mutators ====

	/* Function   : setGid
     * Arguments  : newGid - new gid of the version
     * Description: sets the gid of the version
     */
	void setGid( int newGid ) {
		m_lastUpdate = time(NULL);
		m_gid = newGid;
	};

	/* Function   : setLogicalClock
     * Arguments  : newLogicalClock - new logical clock of the version
     * Description: sets the logical clock of the version
     */
	void setLogicalClock( int clock ) {
		m_lastUpdate = time(NULL);
		m_logicalClock = clock;
	};

	// Set primary and state
	bool setIsPrimary( bool is_primary ) {
		m_lastUpdate = time(NULL);
		return m_isPrimary = is_primary;
	};
	State setState( ReplicatorVersion::State state ) {
		m_lastUpdate = time(NULL);
		return m_state = state;
	};
	State setState( const ReplicatorVersion &other ) {
		return setState( other.getState() );
	};
	bool reset( State state = STATE_INVALID );


	/* Function   : update
     * Arguments  : other - Version info to update from
     * Description: Updates my verion info
     */
	bool update( const ReplicatorVersion &version );


	// ==== Convertors ====

	/* Function    : toString
     * Return value: MyString - string representation of Version object
	 * Description : represents the Version object as string
     */
    virtual const char *toString( void ) const {
		return toString( m_string );
	};
    virtual const char *toString( MyString &str ) const;

	// Get state name
	const char *getStateName( State ) const;
	const char *getStateName( void ) const {
		return getStateName( m_state );
	};
	
	//  === Protected data ===
  protected:

	// components of the version
    time_t					 m_mtime;
    int						 m_gid;
    int						 m_logicalClock;
	bool					 m_isPrimary;
    State					 m_state;
	time_t					 m_lastUpdate;
	mutable MyString		 m_string;

	ReplicatorSimpleFileSet	&m_fileset;
};

#endif // REPLICATOR_VERSION_H
