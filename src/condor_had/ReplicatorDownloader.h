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

#ifndef REPLICATOR_DOWNLOADER_H
#define REPLICATOR_DOWNLOADER_H

#include <list>
using namespace std;

#include "ReplicatorTransferer.h"

// Pre-declare the FileBase class
class ReplicatorFileBase;

/* Class      : ReplicatorDownloader
 * Description: class, representing a file that's versioned and replicated
 */
class ReplicatorDownloader : public ReplicatorTransferer
{
  public:
	ReplicatorDownloader( ReplicatorFileBase &file_info )
		: m_fileInfo( file_info ) {
	};
	~ReplicatorDownloader( void ) { };
	ReplicatorFileBase &getFileInfo( void ) {
		return m_fileInfo;
	};

  private:
	ReplicatorFileBase	&m_fileInfo;
};

class ReplicatorDownloaderList : public ReplicatorTransfererList
{
  public:
	ReplicatorDownloaderList( void );
	~ReplicatorDownloaderList( void );
	bool clear( void );

	int getOldList( time_t maxage, list<ReplicatorFileBase *>& );
	int getList( list<ReplicatorFileBase *>& );

	bool killList( int sig, const list<const ReplicatorFileBase *>& ) const;

  private:
	int getList( const list<ReplicatorTransferer *>&,
				 list<ReplicatorFileBase *>& );
	
};


#endif // REPLICATOR_UPLOADER_H
