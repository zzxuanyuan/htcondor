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

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_config.h"

#include "TransfererBase.h"
#include "FilesOperations.h"
#include "Utils.h"

extern int main_shutdown_graceful( void );

BaseTransferer::BaseTransferer( ReplicatorFileSet *fileset )
: ReplicationBase( fileset, new SafeFileRotator(false) ),
  m_done( false ),
  m_status( XFER_INIT )
{
}

BaseTransferer::~BaseTransferer( void )
{
}

const char *
BaseTransferer::getStatusStr( void ) const
{
	static char buf[32];
	switch ( m_status ) {
	case XFER_INIT:
		return "Initialized";
	case XFER_TRANSFERING:
		return "Transfering";
	case XFER_COMPLETE:
		return "Complete";
	case XFER_CANCELED:
		return "Canceled";
	case XFER_FAILED:
		return "FAILED";
	default:
		snprintf( buf, sizeof(buf), "Unknown (%d)", m_status );
		return buf;
	}

}

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// End: ***
