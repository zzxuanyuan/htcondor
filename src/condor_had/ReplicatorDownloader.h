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
#include "ReplicatorFileSet.h"


/* Class      : ReplicatorDownloader
 * Description: class, representing a file that's versioned and replicated
 */
class ReplicatorDownloader : public ReplicatorTransferer
{
  public:
	ReplicatorDownloader( ReplicatorFileSet &fileset );
	~ReplicatorDownloader( void ) { };
	const ReplicatorFileSet &getFileSet( void ) const {
		return m_fileset;
	};

	bool cleanupTempFiles( void ) {
		return m_fileset.cleanupTempDownloadFiles( );
	};

  private:
	ReplicatorFileSet	&m_fileset;
};

// Currently unused
#if 0
class ReplicatorDownloaderList : public ReplicatorTransfererList
{
  public:
	ReplicatorDownloaderList( void );
	~ReplicatorDownloaderList( void );
	bool clear( void );

	int getList( list<ReplicatorDownloader *>& );
	int getOldList( time_t maxage, list<ReplicatorDownloader *>& );
	int killList( int sig, const list<ReplicatorDownloader *>& );

	bool cleanupTempFiles( void ) const;
	bool cleanupTempFiles( const list<ReplicatorDownloader *>& ) const;
};
#endif

#endif // REPLICATOR_DOWNLOADER_H
