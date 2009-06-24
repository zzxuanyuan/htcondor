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

#ifndef REPLICATOR_FILE_LIST_H
#define REPLICATOR_FILE_LIST_H

#include <list>
using namespace std;

#include "condor_classad.h"
#include "Utils.h"
#include "ReplicatorFile.h"

// List of file/sets
class ReplicatorFileList
{
  public:
	ReplicatorFileList( void );
	~ReplicatorFileList( void );

	bool clear( void );
	bool initFromList( StringList &, const char *spool );
	bool initFromList( StringList & );

	bool registerFile( ReplicatorFile * );
	bool hasFile( const ReplicatorFile * ) const;
	bool findFile( const char *path, ReplicatorFile ** );

	int getCount( void ) const { return m_fileList.size(); };
	bool getFirstFile( const ReplicatorFile **item ) const;

	// Get a StringList to represent the file list
	bool getStringList( StringList & ) const;

	// Comparisons
	bool equivilent( const ReplicatorFileList &other ) const;

	// Get the number of up/down-load transfers active
	int numActiveDownloads( void ) const;
	int numActiveUploads( void ) const;

	// Send message to all peers
	bool sendMessage( int command, bool send_ad, int &errors );

	// Send command to specific peer
	bool sendMessage( int command, bool send_ad,
					  const ReplicatorPeer &, int &errors );

	// Access to the list for iterating
	list<ReplicatorFile *> & getList( void ) { return m_fileList; };

	// Private data
  private:
	list<ReplicatorFile *>	m_fileList;

};	/* ReplicatorFileList */

#endif // REPLICATOR_FILE_LIST_H
