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


// ========================================
// ==== Replicator Uploader List class ====
// ========================================

// C-Tors / D-Tors
ReplicatorUploaderList::ReplicatorUploaderList( void )
{
}

void
ReplicatorUploaderList::getOldList(
	time_t						 maxage,
	list<ReplicatorUploader*>	&uploaders )
{
	list<ReplicatorTransferer*>	trans;
	int num = getOldList( maxage, trans );

	list <ReplicatorTransferer *>::iterator iter;
	for( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		ReplicatorUploader		*up =
			dyanmic_cast<ReplicatorUploader*>(trans);
		ASSERT(up);
		uploaders.push_back( up );
	}
	return num;
}
