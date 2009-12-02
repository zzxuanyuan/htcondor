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

#include "condor_common.h"
#include "condor_classad.h"
#include "SafeFileOps.h"

/* Class      : ReplicatorFile
 * Description: class, representing single file that are
 *              versioned and replicated as a set
 */
class ReplicatorFile
{
  public:

	ReplicatorFile( void );
	bool init( const char &base, const char &path, bool is_log );
	bool init( const ClassAd &ad, const MyString &attr_base );
	virtual ~ReplicatorFile( void );

	// mtime operations
	StatStructTime getMtime( void ) const{
		return m_mtime;
	}
	bool checkFile( void );
	bool checkFile( StatStructTime &mtime, bool &newer );

	// Get the full path out
	const char *getFullPath( void ) const {
		return m_full_path;
	};
	const char *getDownloadTmpPath( void ) const {
		return ( m_download_rotator ?
				 m_download_rotator->getTmpFilePath( ) : NULL );
	};
	const char *getUploadTmpPath( void ) const {
		return ( m_upload_copier ?
				 m_upload_copier->getTmpFilePath( ) : NULL );
	};
	const char *getRelPath( void ) const {
		return m_rel_path;
	};

	// Other accessors
	bool isLogFile( void ) const { return m_is_log; };
	StatStructOff getSize( void ) const { return m_size; };
	bool exists( void ) const { return m_exists; };

	// Comparison operator
	bool operator == ( const ReplicatorFile &other ) const;
	bool operator != ( const ReplicatorFile &other ) const {
		return ( !(*this == other) );
	};
	bool operator == ( const MyString &other ) const;
	bool operator != ( const MyString &other ) const {
		return ( !(*this == other) );
	};
	bool operator == ( const char *other ) const;
	bool operator != ( const char *other ) const {
		return ( !(*this == other) );
	};

	// Update the class ad
	bool updateAd( ClassAd &ad, const MyString &attr_base ) const;

	// Rotate in the new file
	bool installNewFile( void );
	bool setDownloaderPid( int pid );
	bool cleanupTempDownloadFiles( void );
	bool cleanupTempUploadFiles( int pid );
	virtual bool shouldRotate( void ) const {
		return ( !m_is_log && !m_exists );
	};
	virtual bool shouldCopy( void ) const {
		return ( NULL != m_upload_copier );
	};

	// Send messages
	bool sendMessage( bool command, const ClassAd *ad );

	// Create the rotator
  protected:
	virtual SafeFileRotator *createRotator( void ) const;
	virtual SafeFileCopier  *createCopier( void ) const;

  protected:
	bool			 m_initialized;
	const char		*m_full_path;
	SafeFileRotator	*m_download_rotator;
	SafeFileCopier	*m_upload_copier;
	const char		*m_rel_path;
	StatWrapper		*m_stat;
	bool			 m_exists;
	StatStructTime	 m_mtime;
	StatStructOff	 m_size;
	bool			 m_is_log;
};


#endif // REPLICATOR_FILE_H

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// End: ***
