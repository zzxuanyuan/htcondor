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

#include "condor_daemon_core.h"
#include "condor_config.h"
#include "basename.h"
#include "stat_wrapper.h"
#include <list>
#include "ReplicatorTransferer.h"
#include "ReplicatorDownloader.h"

using namespace std;


// ==================================================
// ==== Methods for the Replicator Downloader class
// ==================================================
bool
ReplicatorDownloader::cleanupTempFiles( void ) const
{

	// when the process is killed, it could have not yet erased its
	// temporary files, this is why we ensure it by erasing it in killer
	// function
	MyString extension( m_pid );

	// the .down ending is needed in order not to confuse between
	// upload and download processes temporary files
	extension += ".";
	extension += DOWNLOADING_TEMPORARY_FILES_EXTENSION;

	FilesOperations::safeUnlinkFile( m_fileInfo.getVersionFilePath(),
									 extension.Value( ) );
	FilesOperations::safeUnlinkFile( m_fileInfo.getFilePath(),
									 extension.Value( ) );
}


// ==================================================
// ==== Static helper functions for the List class
// ==================================================
static int
convert( const list<ReplicatorTransferer *>	&inlist,
		 list<ReplicatorDownloader *>		&outlist )
{
	int		num = 0;
	list <ReplicatorTransferer *>::const_iterator iter;
	for( iter = inlist.begin(); iter != inlist.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		ReplicatorDownloader		*down =
			dynamic_cast<ReplicatorDownloader*>(trans);
		ASSERT(down);
		outlist.push_back( down );
		num++;
	}
	return num;
}

static int
convert( const list<ReplicatorDownloader *>	&inlist,
		 list<ReplicatorTransferer *>		&outlist )
{
	int		num = 0;
	list <ReplicatorDownloader *>::const_iterator iter;
	for( iter = inlist.begin(); iter != inlist.end(); iter++ ) {
		ReplicatorDownloader		*down = *iter;
		outlist.push_back( down );
		num++;
	}
	return num;
}


// ==========================================
// ==== Replicator Downloader List class ====
// ==========================================

// C-Tors / D-Tors
ReplicatorDownloaderList::ReplicatorDownloaderList( void )
{
}

int
ReplicatorDownloaderList::getList(
	list<ReplicatorDownloader*>	&downlist )
{
	return convert( m_list, downlist );
}

int
ReplicatorDownloaderList::getOldList(
	time_t						 maxage,
	list<ReplicatorDownloader*>	&downlist )
{
	list<ReplicatorTransferer*>	trans;
	if ( getOldTransList( maxage, trans ) < 0 ) {
		return -1;
	}
	return convert( trans, downlist );
}

int
ReplicatorDownloaderList::killList(
	int								 	signum,
	const list<ReplicatorDownloader*>	&downlist )
{
	list<ReplicatorTransferer*>	translist;
	convert( downlist, translist );
	return killTransList( signum, translist );
}

bool
ReplicatorDownloaderList::cleanupTempFiles( void ) const
{
	const list<ReplicatorDownloader*>	downlist;
	convert( m_list, downlist );
	return cleanupTempFiles( downlist );
}

bool
ReplicatorDownloaderList::cleanupTempFiles(
	const list<ReplicatorDownloader*>	&downlist ) const
{
	list <const ReplicatorDownloader *>::const_iterator iter;
	for( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		const ReplicatorDownloader	*down = *iter;
		down->cleanupTempFiles( );
	}
	return true;
}
