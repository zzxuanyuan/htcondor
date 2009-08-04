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
#include "ReplicatorUploader.h"

using namespace std;


// ==================================================
// ==== Methods for the Replicator Uploader class
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
	extension += UPLOADING_TEMPORARY_FILES_EXTENSION;

	FilesOperations::safeUnlinkFile(
		m_replica.getFileInfo.getVersionFilePath(),
		extension.Value() );
	FilesOperations::safeUnlinkFile(
		m_replicat.getFileInfo.getFilePath(),
		extension.Value() );
}

// ================================================
// ==== Static helper functions for the list class
// ================================================
static int
convert( const list<ReplicatorTransferer *>	&inlist,
		 list<ReplicatorUploader *>			&outlist )
{
	int		num = 0;
	list <ReplicatorTransferer *>::const_iterator iter;
	for( iter = inlist.begin(); iter != inlist.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		ReplicatorUploader		*up =
			dynamic_cast<ReplicatorUploader*>(trans);
		ASSERT(up);
		outlist.push_back(up);
		num++;
	}
	return num;
}

static int
convert( const list<ReplicatorUploader *>	&inlist,
		 list<ReplicatorTransferer *>		&outlist )
{
	int		num = 0;
	list <ReplicatorUploader *>::const_iterator iter;
	for( iter = inlist.begin(); iter != inlist.end(); iter++ ) {
		ReplicatorUploader		*up = *iter;
		outlist.push_back(up);
		num++;
	}
	return num;
}


// ========================================
// ==== Replicator Uploader List class ====
// ========================================

// C-Tors / D-Tors
ReplicatorUploaderList::ReplicatorUploaderList( void )
{
}

int
ReplicatorUploaderList::getList(
	list<ReplicatorUploader*>		&uplist )
{
	return convert( m_list, uplist );
}

int
ReplicatorUploaderList::getOldList(
	time_t							 maxage,
	list<ReplicatorUploader *>		&uplist )
{
	list<ReplicatorTransferer*>	trans;
	if ( getOldTransList( maxage, trans ) < 0 ) {
		return -1;
	}
	return convert( trans, uplist );
}

int
ReplicatorUploaderList::killList(
	int									 signum,
	const list<ReplicatorUploader *>	&uplist )
{
	list<ReplicatorTransferer*>	translist;
	convert( uplist, translist );
	return killTransList( signum, translist );
}

bool
ReplicatorUploaderList::cleanupTempFiles( void ) const
{
	const list<ReplicatorUploader*>	uplist;
	convert( m_list, uplist );
	return cleanupTempFiles( uplist );
}

bool
ReplicatorUploaderList::cleanupTempFiles(
	const list<ReplicatorUploader*>	&uplist ) const
{
	list <const ReplicatorUploader *>::const_iterator iter;
	for( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		const ReplicatorUploader	*up = *iter;
		up->cleanupTempFiles( );
	}
	return true;
}
