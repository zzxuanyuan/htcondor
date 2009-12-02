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

#ifndef REPLICATOR_LOCAL_VERSION_H
#define REPLICATOR_LOCAL_VERSION_H

#include "ReplicatorVersion.h"
#include "ReplicatorFileSet.h"

class ReplicatorLocalVersion : public ReplicatorVersion
{
  public:
	ReplicatorLocalVersion( ReplicatorFileSet &file_set,
							const MyString &version_file );
	virtual ~ReplicatorLocalVersion( void );
	bool init( void );

	/* Function    : synchronize
     * Arguments   : isLogicalClockIncremented - whether to increment the 
	 *				 logical clock or not
	 * Return value: true - if the state file was modified since the last
	 *				 known modification time and 'isLogicalClockIncremented'
	 *				 is true; false - otherwise
	 * Description : synchronizes local state file version according to the
	 *				 OS state file; if it has been updated and the last 
	 *				 modification time of it is later than the recorded one, 
	 *				 then the Replica object is updated, i.e. the OS file is 
	 *				 opened, its fields are loaded into the data members and
	 *				 its last modification time is assigned to
	 *				 'm_lastModifiedTime'
     */
    bool synchronize(bool isLogicalClockIncremented = true);
	bool updateMtime( bool &newer );

	/* Function   : setGid
     * Arguments  : newGid - new gid of the version
     * Description: sets the gid of the version
     */
	void setGid(int newGid, bool save = true );

	/* Function   : setLogicalClock
	 * Arguments  : newLogicalClock - new logical clock of the version
	 * Description: sets the logical clock of the version
	 */
	void setLogicalClock(int clock, bool save = false);

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
	bool readVersionFile( void );
	bool readVersionFile( int &temporaryGid,
						  int &temporaryLogicalClock ) const;
	bool writeVersionFile( void );

	bool sameFiles( const ReplicatorLocalVersion &other ) const;
	const ReplicatorFileSet &getRealFileset( void ) const {
		return m_realFileset;
	};

	bool updateAd( ClassAd &ad ) const;

	// Shouldn't have to have one of these here, but it appears that
	// there's a bug in gcc 4.1.2 that makes it neccessary
    virtual const char *toString( void ) const {
		return toString( m_string );
	};
    virtual const char *toString( MyString &str ) const;

  protected:
	const MyString			&m_versionFilePath;
	ReplicatorFileSet		&m_realFileset;

  private:
	bool createFile( void ) const;
};

#endif // REPLICATOR_LOCAL_VERSION_H
