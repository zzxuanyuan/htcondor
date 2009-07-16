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

#ifndef REPLICATOR_UPLOADER_H
#define REPLICATOR_UPLOADER_H

#include <list>
#include "ReplicatorTransferer.h"
#include "ReplicatorPeer.h"

using namespace std;

// Pre-declare a couple of classes
class ReplicatorFileReplica;	// Pre-declare the file version info

/* Class      : ReplicatorUploader
 * Description: class, representing a version of state file, including gid of
 *				the pool, logical clock and last modification time of the state
 *				file
 */
class ReplicatorUploader : public ReplicatorTransferer
{
  public:
	ReplicatorUploader( ReplicatorFileReplica &replica )
		: m_replica( replica ) {
	};
	~ReplicatorUploader( void ) { };
	ReplicatorFileBase &getFileInfo( void );

  private:
	ReplicatorFileReplica	&m_replica;
};

class ReplicatorUploaderList : public ReplicatorTransfererList
{
  public:
	ReplicatorUploaderList( void );
	~ReplicatorUploaderList( void );
	bool clear( void );

	bool getOldList( time_t maxage, list<ReplicatorFileReplica*>& );
	bool getList( list<ReplicatorFileReplica*>& );

	bool killList( int sig, list<const ReplicatorFileReplica *>& );
};

#endif // REPLICATOR_UPLOADER_H
