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

#ifndef TRANSFERER_BASE_H
#define TRANSFERER_BASE_H

#include "dc_service.h"
#include "MyString.h"
#include "reli_sock.h"
#include "ReplicationBase.h"

/* Class      : BaseTransfererer
 * Description: class, encapsulating the downloading/uploading
 *              'condor_transferer' process behaviour
 *              Generally, the file transfer works in the following way.
 *	            After the backup replication daemon decides to download a
 *              version from the replication leader, the downloading
 *				transferer is started.  It opens a listening socket, the
 *              port of which it sends to the replication leader by means
 *				of REPLICATION_TRANSFER_FILE command.
 *				The replication leader creates an uploading transferer
 *              process upon receiving the command.
 *				The uploading process sends the file
 *              to the downloading transferer to the port, which has been sent
 *              along with the REPLICATION_TRANSFER_FILE command.
 *              In uploading process we do not transfer the actual state and
 *				version files, we copy them to temporary files and send these
 *				temporary ones to the downloading process. The downloading
 *				process in its turn receives the files into temporary copies.
 *				After the transfer is finished, the uploading process gets rid
 *				of the temporary copies, while the downloading one replaces the
 *				actual state and version files with their just received
 *				temporary copies.
 */

class BaseTransferer: public ReplicationBase
{
 public:
	enum Status { XFER_INIT, XFER_TRANSFERING, XFER_COMPLETE,
				  XFER_CANCELED, XFER_FAILED };

	/* Function  : BaseReplicaTransferer constructor
	 * Arguments : pDaemonSinfulString  - downloading/uploading daemon
	 *                                    sinfull string
	 *             pVersionFilePath     - version string in dot-separated
	 *									  format
	 *             pStateFilesPathsList - list of paths to the state files
	 */
	BaseTransferer( ReplicatorFileSet * );
	~BaseTransferer( void );

	/* Function    : reinit
	 * Return value: true   - upon success
	 *               false  - upon failure
	 * Description : the main function of 'condor_transferer' process,
	 *               in which all the communication between the
	 *               downloading and the uploading 'condor_transferer'
	 *               happens
	 */
	bool reinit( void );

	virtual bool cleanupTempFiles( void ) = 0;
	virtual bool finish( void ) = 0;

	void reset( Status status = XFER_TRANSFERING ) {
	  m_done = false;
	  m_status = status;
	};
	Status getStatus( void ) const {
		return m_status;
	};
	const char *getStatusStr( void ) const;
	bool isDone( void ) const {
		return m_done;
	};
	bool isTransfering( void ) const {
		return m_status == XFER_TRANSFERING;
	};
	bool isCanceled( void ) const {
		return m_status == XFER_CANCELED;
	};
	bool isFailed( void ) const {
		return m_status == XFER_FAILED;
	};
	void stopTransfer( Status status = XFER_CANCELED ) {
		m_done = true;
		m_status = status;
	};


 protected:
	bool		 m_done;
	Status		 m_status;

};

#endif // TRANSFERER_BASE_H

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// End: ***
