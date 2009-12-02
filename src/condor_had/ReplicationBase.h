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

#ifndef REPLICATION_BASE_H
#define REPLICATION_BASE_H

#include "condor_common.h"
#include "dc_service.h"
#include "ReplicatorFileSet.h"
#include "ReplicatorLocalVersion.h"
#include "ReplicatorPeer.h"
#include "SafeFileOps.h"

/* Class      : ReplicationBase
 * Description: base class shared by the replicator and the transferer
 *              mostly handles shared configuration crap
 */
class ReplicationBase: public Service
{
  public:

    /* Function: ReplicatorBase constructor
     */
    ReplicationBase( ReplicatorFileSet *, SafeFileRotator * );

	/* Function: ReplicatorBase destructor
	 */
    virtual ~ReplicationBase( void );

	// Accessors
	ReplicatorVersion::State getState( void ) const {
		return m_myVersion.getState( );
	};
	bool isState( ReplicatorVersion::State s ) const {
		return m_myVersion.isState( s );
	};
	ReplicatorVersion::State setState( ReplicatorVersion::State s ) {
		return m_myVersion.setState( s );
	};

	// Initialization methods
	bool initializeAll( void );
    virtual bool preInit( void );
    virtual bool postInit( void );
    virtual bool reconfig( void );

	// ClassAd things
    virtual bool updateAd( ClassAd &ad, bool file_details ) const;
	bool initAd( ClassAd &ad, const char *label ) const;

	// Common elements for parsing messages
	bool commonCommandHandler(
		int								 command,
		Stream							*stream,
		const char						*from,
		ClassAd							&ad,
		ReplicatorPeer					&peer,
		bool							 eom = true );
	bool sameFiles(
		const ReplicatorSimpleFileSet	&filset,
		const ReplicatorPeer			&peer ) const;

	// File extensions
	static const char *getDownloadExtension( void );
	static const char *getUploadExtension( void );

  protected:

	// configuration variables
	ReplicatorFileSet		*m_fileset;
	MyString				 m_versionFilePath;
	const char				*m_localName;	// We don't own this

	// local version
    ReplicatorLocalVersion	 m_myVersion;

	// socket connection timeout
    int						 m_connectionTimeout;
	int						 m_maxTransferLifetime;

	// File operators
	SafeFileRotator			*m_versionRotator;
};

extern const ReplicationBase *getReplicator( void );

#endif // REPLICATION_BASE_H

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// c-basic-offset:4 ***
// End: ***
