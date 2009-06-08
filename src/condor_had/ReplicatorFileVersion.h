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

#ifndef REPLICATOR_FILE_VERSION_H
#define REPLICATOR_FILE_VERSION_H

#include "Utils.h"

/* Class      : ReplicatorFileVersion
 * Description: class, representing a version of state file, including gid of
 *				the pool, logical clock and last modification time of the state
 *				file
 */
class ReplicatorFile;
class ReplicatorFileVersion
{
public:

    /* Function: ReplicatorFileVersion constructor
     */
	ReplicatorFileVersion( const ReplicatorFileBase & );


	// ==== Operations ====

# if 0
	/* Function   : initialize
	 * Arguments  : pStateFilePath - OS path to state file
	 *  			pVersionFilePath - OS path to version file
	 * Description: initializes all data members
	 */
    bool initialize( const ReplicatorFileBase & );
# endif

	/* Function    : synchronize
     * Arguments   : isLogicalClockIncremented - whether to increment the 
	 *				 logical clock or not
	 * Return value: true - if the state file was modified since the last known
	 *				 modification time and 'isLogicalClockIncremented' is true;
	 *				 false - otherwise
	 * Description : synchronizes local state file version according to the OS
	 *				 state file; if it has been updated and the last 
	 *				 modification time of it is later than the recorded one, 
	 *				 then the Replica object is updated, i.e. the OS file is 
	 *				 opened, its fields are loaded into the data members and its
	 *				 last modification time is assigned to 'm_lastModifiedTime'
     */
    bool synchronize(bool isLogicalClockIncremented = true);

	// ==== End of operations ====


	// ==== Inspectors ====

	/* Function    : getGid
     * Return value: int - gid
     * Description : returns gid
     */
    int getGid(void) const { return m_gid; };

    /* Function    : getLogicalClock
     * Return value: int - logical clock
     * Description : returns logical clock
     */
	int getLogicalClock(void) const { return m_logicalClock; };

	/* Function    : getFileInfo
	 * Return value: ReplicatorFile - Information on the replicated file
	 * Description : returns the related file info object
	 */
    const ReplicatorFile &getFileInfo(void) const {
		return m_fileInfo;
	};

    /* Function    : readVersionFile
 	 * Arguments   : temporaryGid - the value of OS file gid field will be 
	 *								assigned to the parameter
 	 *               temporaryLogicalClock - the value of OS file logical clock
 	 *                                       field will be assigned to the 
	 *										 parameter
 	 * Return value: bool - success/failure value
 	 * Description : loads Replica components from the underlying OS file to
 	 *               to the specified arguments
 	 */
	bool readVersionFile( int& temporaryGid, int& temporaryLogicalClock ) const;

	// ==== End of inspectors ====


	// ==== Comparison operators ====

	/* Function    : isComparable
	 * Arguments   : replica - the compared replica
	 * Return value: bool - true/false value
     * Description : the replicas are comparable, if their gids are identical
     */
    bool isComparable(const ReplicatorFileVersion &version) const;

	/* Function    : operator >
     * Arguments   : replica - the compared replica
     * Return value: bool - true/false value
     * Description : the version is bigger than another, if its logical clock is
	 *				 bigger or if the state of the local daemon is 
	 *				 REPLICATION_LEADER, whilst the state of the remote daemon
	 *				 is not
	 * Note        : the comparison is used, while choosing the best replica in
	 *				 VERSION_DOWNLOADING state, to simply compare the logical
	 *				 clocks the replicas' states must be set to BACKUP
     */
	bool operator > (const ReplicatorFileVersion &version) const;

	/* Function    : operator >=
     * Arguments   : version - the compared version
     * Return value: bool - true/false value
     * Description : the version is bigger/equal than another, if its logical 
	 * 				 clock is bigger/equal or if the state of the local daemon
	 *				 is REPLICATION_LEADER, whilst the state of the remote
	 *				 daemon is not
     */
    bool operator >= (const ReplicatorFileVersion &version) const;

	// ==== End of comparison operators ====


	// ==== Mutators ====

	/* Function   : setGid
     * Arguments  : newGid - new gid of the version
     * Description: sets the gid of the version
     */
	void setGid(int newGid, bool save = true ) {
		m_gid = newGid;
		if (save) writeVersionFile( );
	};

	/* Function   : setLogicalClock
     * Arguments  : newLogicalClock - new logical clock of the version
     * Description: sets the logical clock of the version
     */
	void setLogicalClock(int clock, bool save = false) {
		m_logicalClock = clock;
		if (save) writeVersionFile( );
	};

	// ==== End of mutators ====


	// ==== Convertors ====

	/* Function    : toString
     * Return value: MyString - string representation of Version object
	 * Description : represents the Version object as string
     */
    MyString & toString( MyString &str ) const;

	// ==== End of convertors ====


	// === Protected methods ===
  protected:

    bool readVersionFile( void );
    bool writeVersionFile( void );

	
	//  === Protected data ===
  protected:

	// static data members
    static time_t            m_lastModifiedTime;
	
	// File info
	const ReplicatorFile	&m_fileInfo;

	// components of the version
    int                      m_gid;
    int                      m_logicalClock;
};

#endif // REPLICATOR_FILE_VERSION_H
