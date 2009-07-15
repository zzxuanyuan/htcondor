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

#include "string_list.h"
#include "ReplicatorFile.h"

/* Class      : ReplicatorFileSet
 * Description: class, representing a set of file that are
 *              versioned and replicated as a set
 */
class ReplicatorFileSet : public ReplicatorFileBase
{
  public:
	ReplicatorFileSet( const char *spool, StringList *files );
	ReplicatorFileSet( void );
	~ReplicatorFileSet( void );

	void setPaths( StringList *paths );
	void setNames( StringList *names );

	bool rotate( int pid ) const;

	// Comparison operators
	bool operator == ( const ReplicatorFileBase &other ) const;
	bool operator == ( const ReplicatorFileSet &other ) const {
		return ( *this == other.getFileList() );
	};
	bool operator == ( StringList &other ) const {
		return getFileList().equivilent( other );
	};

	// Types
	virtual bool isFile( void ) const { return false; };
	virtual bool isSet( void ) const { return true; };

	// Comparison methods
	virtual bool identical(  const ReplicatorFileBase &other ) const {
		return ( *this == other );
	};
	bool equivilent( StringList &other ) const {
		return getNameList().equivilent(other);
	};
	bool equivilent( const ReplicatorFileSet &other ) const {
		return getNameList().equivilent( other.getNameList() );
	};
	bool equivilent( const ReplicatorFileBase &other ) const {
		const ReplicatorFileSet	*setptr =
			dynamic_cast<const ReplicatorFileSet*>(&other);
		if ( NULL == setptr ) {
			return false;
		} else {
			return equivilent( *setptr );
		}
	};

	// Accessors
	StringList &getNameList( void ) const {
		return *m_nameList;
	};
	const char *getFilePath( void ) const {
		return ( m_pathList ? m_pathList->first() : NULL );
	};
	StringList &getFileList( void ) const {
		return ( m_pathList ? (*m_pathList) : (*m_nameList) );
	};

	const char *getFiles( void ) const {
		return m_pathListStr;
	};
	const char *getNames( void ) const {
		return m_nameListStr;
	};

	// The mtime of the file / file set
	bool getMtime( time_t &mtime ) const;


  private:
	void setNamesFromPaths( StringList *paths );

	mutable StringList	*m_nameList;
	char				*m_nameListStr;
	mutable StringList	*m_pathList;
	char				*m_pathListStr;

};	/* class ReplilcatorFileSet */


#endif // REPLICATOR_FILE_SET_H
