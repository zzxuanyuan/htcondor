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

#ifndef REPLICATOR_REMOTE_VERSION_H
#define REPLICATOR_REMOTE_VERSION_H

#include "ReplicatorVersion.h"
#include "ReplicatorFileSet.h"
#include "condor_classad.h"
#include "string_list.h"

/* Class      : ReplicatorRemoteVersion
 * Description: class, representing a version of state file, including gid of
 *				the pool, logical clock and last modification time of the state
 *				file
 */
class ReplicatorPeer;
class ReplicatorRemoteVersion : public ReplicatorVersion
{
  public:
	ReplicatorRemoteVersion( const ReplicatorPeer & );
	virtual ~ReplicatorRemoteVersion( void ) { };

	bool initialize( const ClassAd &ad );

	const ReplicatorPeer &getPeer( void ) const {
		return m_peer;
	};
	bool isPeerValid( void ) const;

	const ReplicatorSimpleFileSet &getFiles( void ) const {
		return m_fileset;
	};
	bool sameFiles( const ReplicatorRemoteVersion &other ) const {
		return m_fileset.sameFiles( other.getFiles() );
	};

	// Shouldn't have to have one of these here, but it appears that
	// there's a bug in gcc 4.1.2 that makes it neccessary
    virtual const char *toString( void ) const {
		return toString( m_string );
	};
    const char *toString( MyString &str ) const;

  protected:
	const ReplicatorPeer		&m_peer;
	ReplicatorSimpleFileSet		 m_realFileset;
};

// Like above, but the peer is mutable
class ReplicatorRemoteVersionMutable : public ReplicatorRemoteVersion
{
  public:
	ReplicatorRemoteVersionMutable( const ReplicatorPeer & );
	~ReplicatorRemoteVersionMutable( void ) { };

	bool setPeer( const ReplicatorRemoteVersion &other );
	bool setPeer( const ReplicatorPeer &other );

  protected:
};

#endif // REPLICATOR_REMOTE_VERSION_H
