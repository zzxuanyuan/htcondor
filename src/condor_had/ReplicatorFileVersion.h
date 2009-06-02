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

#ifndef REPLICATOR_FILE_VERSION_H
#define REPLICATOR_FILE_VERSION_H

// for 'ReplicatorState'
#include "ReplicatorProcessData.h"
#include "ReplicatorPeer.h"
#include "Utils.h"
#include "reli_sock.h"


// the state of replicator daemon
enum ReplicatorState {
	VERSION_REQUESTING = 0,
	VERSION_DOWNLOADING = 1,
	BACKUP,
	REPLICATION_LEADER
};

/* Class      : ReplicatorFileVersionProcessData
 * Description: class, representing a version of state file, including gid of
 *				the pool, logical clock and last modification time of the state
 *				file
 */
class ReplicatorFile;			// Pre-declare the file info
class ReplicatorFileVersion;	// Pre-declare the file version info
class ReplicatorUploadProcessData : public ReplicatorProcessData
{
  public:
	ReplicatorUploadProcessData( ReplicatorFileVersion &version_info )
		: m_versionInfo( version_info ) {
	};
	~ReplicatorUploadProcessData( void ) { };
	ReplicatorFile &getFileInfo( void );

  private:
	ReplicatorFileVersion	&m_versionInfo;
};

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
#  if 0
	ReplicatorFileVersion( void );
#  endif
	ReplicatorFileVersion( const ReplicatorFile &, const char *sinful );


	// ==== Operations ====

	/* Function   : initialize
	 * Arguments  : pStateFilePath - OS path to state file
	 *  			pVersionFilePath - OS path to version file
	 * Description: initializes all data members
	 */
    void initialize( const ReplicatorFile & );

	/* Function    : synchronize
     * Arguments   : isLogicalClockIncremented - whether to increment the 
	 *				 logical clock or not
	 * Return value: true - if the state file was modified since the last known
	 *				 modification time and 'isLogicalClockIncremented' is true;
	 *				 false - otherwise
	 * Description : synchronizes local state file version according to the OS
	 *				 state file; if it has been updated and the last 
	 *				 modification time of it is later than the recorded one, 
	 *				 then the Version object is updated, i.e. the OS file is 
	 *				 opened, its fields are loaded into the data members and its
	 *				 last modification time is assigned to 'm_lastModifiedTime'
     */
    bool synchronize(bool isLogicalClockIncremented = true);

	/* Function   : code
	 * Arguments  : socket - socket, through which the date is written
     * Description: write the inner Version object representation to the socket
     */
    bool code(ReliSock& );

	/* Function   : decode
     * Arguments  : stream - socket, through which the date is received
     * Description: receive remote Version object representation from the socket
     */
    bool decode(Stream* );

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

    /* Function    : getSinfulString
     * Return value: MyString: the sinful string
     * Description : returns the sinful string
     */
    MyString getMySinfulString(void) const { return m_mySinfulString; };

	/* Function    : getHostName
	 * Return value: MyString - this replication daemon host name
	 * Description : returns this replication daemon host name
	 */
    MyString getHostName(void) const;

	/* Function    : getFileInfo
	 * Return value: ReplicatorFile - Information on the replicated file
	 * Description : returns the related file info object
	 */
    const ReplicatorFile &getFileInfo(void) const {
		return m_fileInfo;
	};

	/* Function    : getPeerInfo
	 * Return value: ReplicatorPeer - Information on the peer host
	 * Description : returns the related file info object
	 */
    const ReplicatorPeer &getPeerInfo(void) const {
		return m_peerInfo;
	};

	/* Function    : getProcessInfo
	 * Return value: Process info object
	 * Description : returns the related process info object
	 */
    ReplicatorUploadProcessData &getProcessInfo(void) {
		return m_uploadProcessData;
	};

    /* Function    : load
 	 * Arguments   : temporaryGid - the value of OS file gid field will be 
	 *								assigned to the parameter
 	 *               temporaryLogicalClock - the value of OS file logical clock
 	 *                                       field will be assigned to the 
	 *										 parameter
 	 * Return value: bool - success/failure value
 	 * Description : loads Version components from the underlying OS file to
 	 *               to the specified arguments
 	 */
	bool load( int& temporaryGid, int& temporaryLogicalClock ) const;

	// ==== End of inspectors ====


	// ==== Comparison operators ====

	/* Function    : isSameHost
	 * Arguments   : hostname - the hostname to compare to
	 * Return value: bool - true/false value
     * Description : Returns true if the hostnames are the same
     */
    bool isSameHost(const char *hostname) const {
		return m_peerInfo == hostname;
	};

	/* Function    : isSameHost
	 * Arguments   : other - Version to compare to
	 * Return value: bool - true/false value
     * Description : Returns true if the hostnames are the same
     */
    bool isSameHost(const ReplicatorFileVersion &other) const {
		return other.getPeerInfo() == m_peerInfo;
	};

	/* Function    : isComparable
	 * Arguments   : version - the compared version
	 * Return value: bool - true/false value
     * Description : the versions are comparable, if their gids are identical
     */
    bool isComparable(const ReplicatorFileVersion &version) const;

	/* Function    : operator >
     * Arguments   : version - the compared version
     * Return value: bool - true/false value
     * Description : the version is bigger than another, if its logical clock is
	 *				 bigger or if the state of the local daemon is 
	 *				 REPLICATION_LEADER, whilst the state of the remote daemon
	 *				 is not
	 * Note        : the comparison is used, while choosing the best version in
	 *				 VERSION_DOWNLOADING state, to simply compare the logical
	 *				 clocks the versions' states must be set to BACKUP
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
	//friend bool operator == (const ReplicatorFileVersion &, const Version & );

	// ==== End of comparison operators ====


	// ==== Mutators ====

	/* Function   : setState
	 * Arguments  : newState - new state of the replication daemon to send to
	 *				the newly joined machine
     * Description: sets the state of the replication daemon to send to the
	 *				newly joined machine
     */
    void setState(const ReplicatorState& newState) { m_state = newState; };

	/* Function   : setState
     * Arguments  : version - the version, the state of which is assigned to
     *                        the current version's one
     * Description: sets the state of the replication daemon to send to the
     *              newly joined machine as the specified version's one
     */
    void setState(const ReplicatorFileVersion &version) {
		m_state = version.getState();
	};

	/* Function   : setGid
     * Arguments  : newGid - new gid of the version
     * Description: sets the gid of the version
     */
	void setGid(int newGid) { m_gid = newGid; save( ); };

	// ==== End of mutators ====


	// ==== Convertors ====

	/* Function    : toString
     * Return value: MyString - string representation of Version object
	 * Description : represents the Version object as string
     */
    MyString toString( void ) const;

	// ==== End of convertors ====


	// === Private methods ===
  private:

	/* Function    : getState
     * Return value: ReplicatorState - this replication daemon current state
     * Description : returns this replication daemon current state
     */   
    const ReplicatorState& getState( void ) const { return m_state; };
    bool load( void );
    void save( void );

	
	//  === Private data ===
  private:

	// static data members
    static time_t            m_lastModifiedTime;
	
	// File info
	const ReplicatorFile	&m_fileInfo;
	const ReplicatorPeer	&m_peerInfo;

	// My process data
	ReplicatorUploadProcessData m_uploadProcessData;
 
	// components of the version
    int                      m_gid;
    int                      m_logicalClock;
    MyString                 m_mySinfulString;
	ReplicatorState          m_state;

	// added support for conservative policy of accepting updates from primary
	// HAD machines only
	bool    				 m_isPrimary;
};

#endif // REPLICATOR_FILE_VERSION_H
