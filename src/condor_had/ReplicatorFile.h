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

#ifndef REPLICATOR_FILE_H
#define REPLICATOR_FILE_H

#include <list>
using namespace std;

#include "condor_classad.h"
#include "Utils.h"
#include "ReplicatorTransferer.h"
#include "ReplicatorDownloader.h"
#include "ReplicatorFileVersion.h"

// Pre-declare the peer classes
class ReplicatorPeer;
class ReplicatorPeerList;
class ReplicatorFileBase;
class ReplicatorFileReplica;

/* Class      : ReplicatorFileBase
 * Description: Base class, representing a file or set of files that are
 *              versioned and replicated
 */
class ReplicatorFileBase
{
  public:

	ReplicatorFileBase( const char *spool );
	ReplicatorFileBase( void );
	virtual ~ReplicatorFileBase( void );

	// Initializers
	bool initVersionInfo( const char *spool );

	// Comparison operators
	virtual bool operator == ( const ReplicatorFileBase &other ) const = 0;

	// Comparison methods
	virtual bool identical(  const ReplicatorFileBase &other ) const;
	virtual bool equivilent( const ReplicatorFileBase &other ) const;

	// Types
	virtual bool isFile( void ) const = 0;
	virtual bool isSet( void ) const = 0;

	// Accessors
	virtual const char *getFilePath( void ) const = 0;
	const char * getVersionFilePath( void ) const {
		return m_versionFilePath.Value();
	};
	const ReplicatorFileVersion &getMyVersion( void ) const {
		return m_myVersion;
	};
	ReplicatorDownloader &getDownloader( void ) const {
		return m_downloader;
	};

	// Get count of active up / downloads
	int numActiveUploads( void ) const;
	int numActiveDownloads( void ) const {
		return m_downloader.isActive() ? 1 : 0;
	};

	// Set the peer list
	bool setPeers( const ReplicatorPeerList &peers );

	// Register all of my transferers with a transferer list
	bool registerUploaders( ReplicatorTransfererList &transferers ) const;
	bool registerDownloaders( ReplicatorTransfererList &transferers ) const;

	// Replica operators
	bool hasReplica( const ReplicatorFileReplica & ) const;
	bool addReplica( ReplicatorFileReplica * );
	bool findReplica( const char *hostname,
					  const ReplicatorFileReplica *& ) const;
	bool findReplica( const ReplicatorPeer &peer,
					  const ReplicatorFileReplica *& ) const;
	bool getReplicaList( list<ReplicatorFileReplica *> &out ) {
		out = m_replicaList;
		return true;
	};

	// Rotate in the downloaded file
	bool synchronize( bool clock_synced ) {
		return m_myVersion.synchronize( clock_synced );
	};
	virtual bool rotate( int pid ) const = 0;

	// Send message to a single peer
	bool sendMessage( int command, bool send_ad,
					  const ReplicatorPeer &, int &errors );

	// Send message to all peers
	bool sendMessage( int command, bool send_ad, int &errors );

	// The a string representation of the file(s)
	virtual const char *getFiles( void ) const = 0;

	// The mtime of the file / file set
	virtual bool getMtime( time_t &mtime ) const = 0;

  protected:
	bool sendMessage( int command, const ClassAd *ad,
					  const ReplicatorPeer &peer, int &errors );
	bool rotateFile( int pid, const char *file ) const;
	static bool getFileMtime( const char *path, time_t &mtime );

  protected:
	MyString						 m_versionFilePath;
	ReplicatorFileVersion			 m_myVersion;
	list<ReplicatorFileReplica *>	 m_replicaList;
	ClassAd							 m_classAd;

	// process ids of uploading/downloading 'condor_transferer' processes for
	// monitoring and handling the problem of stuck transferer processes and
    // starting times of uploading/downloading 'condor_transferer' processes
	// for handling the problem of stuck transferer processes
	mutable ReplicatorDownloader	 m_downloader;

};	/* class ReplicatorFileBase */


/* Class      : ReplicatorFile
 * Description: class, representing a single file that is
 *              versioned and replicated
 */
class ReplicatorFile : public ReplicatorFileBase
{
  public:
	ReplicatorFile( const char *file, const char *spool );
	ReplicatorFile( const char *file );
	~ReplicatorFile( void );

	// Comparison operators
	bool operator == ( const ReplicatorFileBase &other ) const;
	bool operator == ( const ReplicatorFile &other ) const {
		return ( *this == other.getFilePath() );
	};
	bool operator == ( const char *file_path ) const {
		return ( m_filePath == file_path );
	};

	// Comparison methods
	virtual bool identical(  const ReplicatorFileBase &other ) const {
		return ( *this == other );
	};
	virtual bool equivilent( const ReplicatorFileBase &other ) const {
		return ( *this == other );
	};

	// Types
	virtual bool isFile( void ) const { return true; };
	virtual bool isSet( void ) const { return false; };

	// Accessors
	const char *getFilePath( void ) const {
		return m_filePath.Value();
	};

	// Rotate in the downloaded file
	bool rotate( int pid ) const;
	bool rotateFile( int pid ) const;

	// The a string representation of the file(s)
	const char *getFiles( void ) const {
		return m_filePath.Value();
	};

	// The mtime of the file / file set
	bool getMtime( time_t &mtime ) const;


  protected:
	MyString	m_filePath;

};	/* class ReplicatorFile */


#endif // REPLICATOR_FILE_H
