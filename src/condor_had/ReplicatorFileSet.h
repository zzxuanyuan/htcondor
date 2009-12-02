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

#ifndef REPLICATOR_FILE_SET_H
#define REPLICATOR_FILE_SET_H

#include <list>
#include "string_list.h"
#include "stat_wrapper.h"
#include "ReplicatorFile.h"

using namespace std;

/* Class      : ReplicatorSimpleFileSet
 * Description: class, representing a set of file that are
 *              versioned and replicated as a set
 */
class ReplicatorSimpleFileSet
{
  public:
	ReplicatorSimpleFileSet( void );
	ReplicatorSimpleFileSet( const ReplicatorSimpleFileSet &other );
	virtual ~ReplicatorSimpleFileSet( void );

	bool init( const StringList &files,
			   const StringList &logfiles,
			   bool &modified );
	bool init( const StringList &files, const StringList &logfiles ) {
		bool	modified;
		return init( files, logfiles, modified );
	};
	bool init( const MyString &files, const MyString &logfiles, bool &mod );
	bool init( const MyString &files, const MyString &logfiles ) {
		bool	modified;
		return init( files, logfiles, modified );
	};
	bool init( const ClassAd &ad );
	void clear( void );

	// Comparison
	bool sameFiles( const ReplicatorSimpleFileSet &other ) const;

	// Accessors
	const StringList &getFileList( void ) const {
		return *m_file_names;
	};
	const char *getFileListStr( void ) const {
		return m_file_names_str;
	};

	const StringList &getLogfileList( void ) const {
		return *m_logfile_names;
	};
	const char *getLogfileListStr( void ) const {
		return m_logfile_names_str;
	};

	int numFileNames( void ) const;

  protected:
	const StringList		*m_file_names;
	const char				*m_file_names_str;
	const StringList		*m_logfile_names;
	const char				*m_logfile_names_str;

};	/* class ReplilcatorSimpleFileSet */


/* Class      : ReplicatorFileSet
 * Description: class, representing a set of file that are
 *              versioned and replicated as a set
 */
class ReplicatorFileSet : public ReplicatorSimpleFileSet
{
  public:
	ReplicatorFileSet( );
	virtual ~ReplicatorFileSet( void );

	bool init( const char &basedir,
			   const StringList &files,
			   const StringList &logfiles,
			   bool &modified );
	bool init( const ClassAd &ad );

	// Create a new file object
	virtual ReplicatorFile	*createFileObject( void ) const {
		return new ReplicatorFile( );
	};

	// Accessors
	const char *getBaseDir( void ) const {
		return m_base_dir;
	};
	bool sameFiles( const ReplicatorFileSet &other ) const;
	bool sameFiles( const ReplicatorSimpleFileSet &other ) const {
		return ReplicatorSimpleFileSet::sameFiles( other );
	};
	bool getFileList( StringList & ) const;
	ReplicatorFile *findFile( const ReplicatorFile &other );
	ReplicatorFile *findFile( const char *name );
	const ReplicatorFile *findConstFile( const ReplicatorFile &other ) const;
	const ReplicatorFile *findConstFile( const char *name ) const;
	int numFiles( void ) const {
		return m_files.size();
	};

	// Update the classad
	bool updateAd( ClassAd &ad, bool details ) const;
	bool publishFileDetails( ClassAd &ad, unsigned fileno ) const;

	// The mtime of the file / file set
	StatStructTime getCurMtime( void ) const {
		return m_max_mtime;
	};
	bool checkFiles( StatStructTime &mtime, bool &newer, int &num_mtimes );

	// Install downloaded files
	bool setDownloaderPid( int pid );
	bool installNewFiles( void ) const;
	bool cleanupTempDownloadFiles( void ) const;
	bool cleanupTempUploadFiles( int pid ) const;

	// Broadcast messages
	bool sendMessage( int command, const ClassAd *ad );

  protected:

	bool rebuild( void );

	const char				*m_base_dir;
	list<ReplicatorFile *>	 m_files;
	StatStructTime			 m_max_mtime;
	bool					 m_is_remote;

};	/* class ReplilcatorFileSet */


#endif // REPLICATOR_FILE_SET_H

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// c-basic-offset:4 ***
// End: ***
