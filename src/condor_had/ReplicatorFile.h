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

#include "Utils.h"
#include "ReplicatorFileVersion.h"
#include "ReplicatorProcessData.h"


/* Class      : ReplicatorFileProcessData
 * Description: class, representing a file that's versioned and replicated
 */
class ReplicatorDownloadProcessData : public ReplicatorProcessData
{
  public:
	ReplicatorDownloadProcessData( ReplicatorFile &file_info )
		: m_fileInfo( file_info ) {
	};
	~ReplicatorDownloadProcessData( void ) { };
	ReplicatorFile &getFileInfo( void ) {
		return m_fileInfo;
	};

  private:
	ReplicatorFile	&m_fileInfo;
};

/* Class      : ReplicatorFile
 * Description: class, representing a file that's versioned and replicated
 */
class ReplicatorFile
{
  public:
	ReplicatorFile( const char *spool, const char *path );
	~ReplicatorFile( void );

	// Comparison operators
	bool operator == ( const ReplicatorFile &other ) const;
	bool operator == ( const char *file_path ) const;

	// Accessors
	const MyString &getFilePath( void ) const {
		return m_filePath;
	};
	const MyString &getVersionFilePath( void ) const {
		return m_versionFilePath;
	};
	const ReplicatorFileVersion &getMyVersion( void ) const {
		return m_myVersion;
	};

	// Version operators
	bool findVersion( const char *hostname,
					  const ReplicatorFileVersion *& ) const;
	bool hasVersion( const ReplicatorFileVersion & ) const;
	bool updateVersion( ReplicatorFileVersion & );

	// Rotate in the downloaded file
	bool rotateFile( int pid ) const;
	bool synchronize( bool clock_synced ) {
		return m_myVersion.synchronize( clock_synced );
	};

  private:
	MyString						 m_filePath;
	MyString						 m_versionFilePath;
	ReplicatorFileVersion			 m_myVersion;
	list<ReplicatorFileVersion *>	 m_versionList;

	// process ids of uploading/downloading 'condor_transferer' processes for
	// monitoring and handling the problem of stuck transferer processes and
    // starting times of uploading/downloading 'condor_transferer' processes
	// for handling the problem of stuck transferer processes
	ReplicatorDownloadProcessData	 m_downloadProcessData;

};

class ReplicatorFileList
{
  public:
	ReplicatorFileList( void );
	~ReplicatorFileList( void );

	bool registerFile( ReplicatorFile * );
	bool hasFile( const ReplicatorFile * ) const;
	bool findFile( const char *path, ReplicatorFile ** );

  private:
	list<ReplicatorFile *>		 m_fileList;
};

#endif // REPLICATION_FILE_H
